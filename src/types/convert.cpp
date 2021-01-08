// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/convert.hpp"

#include "../inc/unicode.hpp"

#ifdef BUILD_ONECORE_INTERACTIVITY
#include "../../interactivity/inc/VtApiRedirection.hpp"
#endif

#pragma hdrstop

// TODO: MSFT 14150722 - can these const values be generated at
// runtime without breaking compatibility?
static const WORD altScanCode = 0x38;
static const WORD leftShiftScanCode = 0x2A;

// Routine Description:
// - Takes a multibyte string, allocates the appropriate amount of memory for the conversion, performs the conversion,
//   and returns the Unicode UTF-16 result in the smart pointer (and the length).
// Arguments:
// - codepage - Windows Code Page representing the multibyte source text
// - source - View of multibyte characters of source text
// Return Value:
// - The UTF-16 wide string.
// - NOTE: Throws suitable HRESULT errors from memory allocation, safe math, or MultiByteToWideChar failures.
[[nodiscard]] std::wstring ConvertToW(const UINT codePage, const std::string_view source)
{
    // Make a buffer on behalf of the caller.
    std::wstring out;

    // Call the other form of the function.
    ConvertToW(codePage, source, out);

    // Return as a wstring
    return out;
}

// Routine Description:
// - Takes a multibyte string, allocates the appropriate amount of memory for the conversion, performs the conversion,
//   and returns the Unicode UTF-16 result in the smart pointer (and the length).
// - NOTE: This form exists so a frequent caller with a hot path can cache their string
//   buffer between calls instead of letting it get new/deleted in a tight loop.
// Arguments:
// - codepage - Windows Code Page representing the multibyte source text
// - source - View of multibyte characters of source text
// - outBuffer - The buffer to fill with converted wide string data.
// Return Value:
// - The UTF-16 wide string.
// - NOTE: Throws suitable HRESULT errors from memory allocation, safe math, or MultiByteToWideChar failures.
[[nodiscard]] void ConvertToW(const UINT codePage,
                              const std::string_view source,
                              std::wstring& outBuffer)
{
    // If there's nothing to convert, bail early.
    if (source.empty())
    {
        outBuffer.clear();
        return;
    }

    int iSource; // convert to int because Mb2Wc requires it.
    THROW_IF_FAILED(SizeTToInt(source.size(), &iSource));

    // Ask how much space we will need.
    // In certain codepages, Mb2Wc will "successfully" produce zero characters (like in CP50220, where a SHIFT-IN character
    // is consumed but not transformed into anything) without explicitly failing. When it does this, GetLastError will return
    // the last error encountered by the last function that actually did have an error.
    // This is arguably correct (as the documentation says "The function returns 0 if it does not succeed"). There is a
    // difference that we **don't actually care about** between failing and successfully producing zero characters.,
    // Anyway: we need to clear the last error so that we can fail out and IGNORE_BAD_GLE after it inevitably succeed-fails.
    SetLastError(0);
    int const iTarget = MultiByteToWideChar(codePage, 0, source.data(), iSource, nullptr, 0);
    THROW_LAST_ERROR_IF_AND_IGNORE_BAD_GLE(0 == iTarget);

    size_t cchNeeded;
    THROW_IF_FAILED(IntToSizeT(iTarget, &cchNeeded));

    // Allocate ourselves some space
    outBuffer.resize(cchNeeded);

    // Attempt conversion for real.
    THROW_LAST_ERROR_IF_AND_IGNORE_BAD_GLE(0 == MultiByteToWideChar(codePage, 0, source.data(), iSource, outBuffer.data(), iTarget));
}

// Routine Description:
// - Takes a wide string, allocates the appropriate amount of memory for the conversion, performs the conversion,
//   and returns the Multibyte result
// Arguments:
// - codepage - Windows Code Page representing the multibyte destination text
// - source - Unicode (UTF-16) characters of source text
// Return Value:
// - The multibyte string encoded in the given codepage
// - NOTE: Throws suitable HRESULT errors from memory allocation, safe math, or MultiByteToWideChar failures.
[[nodiscard]] std::string ConvertToA(const UINT codepage, const std::wstring_view source)
{
    // Make a buffer on behalf of the caller.
    std::string out;

    // Call the other form of the function.
    ConvertToA(codepage, source, out);

    // Return as a string
    return out;
}

// Routine Description:
// - Takes a wide string, allocates the appropriate amount of memory for the conversion, performs the conversion,
//   and returns the Multibyte result
// - NOTE: This form exists so a frequent caller with a hot path can cache their string
//   buffer between calls instead of letting it get new/deleted in a tight loop.
// Arguments:
// - codepage - Windows Code Page representing the multibyte destination text
// - source - Unicode (UTF-16) characters of source text
// - outBuffer - The buffer to fill with converted string data.
// Return Value:
// - The multibyte string encoded in the given codepage
// - NOTE: Throws suitable HRESULT errors from memory allocation, safe math, or MultiByteToWideChar failures.
[[nodiscard]] void ConvertToA(const UINT codepage,
                              const std::wstring_view source,
                              std::string& outBuffer)
{
    // If there's nothing to convert, bail early.
    if (source.empty())
    {
        outBuffer.clear();
        return;
    }

    int iSource; // convert to int because Wc2Mb requires it.
    THROW_IF_FAILED(SizeTToInt(source.size(), &iSource));

    // Ask how much space we will need.
    // clang-format off
#pragma prefast(suppress: __WARNING_W2A_BEST_FIT, "WC_NO_BEST_FIT_CHARS doesn't work in many codepages. Retain old behavior.")
    // clang-format on
    int const iTarget = WideCharToMultiByte(codepage, 0, source.data(), iSource, nullptr, 0, nullptr, nullptr);
    THROW_LAST_ERROR_IF(0 == iTarget);

    size_t cchNeeded;
    THROW_IF_FAILED(IntToSizeT(iTarget, &cchNeeded));

    // Allocate ourselves some space
    outBuffer.resize(cchNeeded);

    // Attempt conversion for real.
    // clang-format off
#pragma prefast(suppress: __WARNING_W2A_BEST_FIT, "WC_NO_BEST_FIT_CHARS doesn't work in many codepages. Retain old behavior.")
    // clang-format on
    THROW_LAST_ERROR_IF(0 == WideCharToMultiByte(codepage, 0, source.data(), iSource, outBuffer.data(), iTarget, nullptr, nullptr));
}

// Routine Description:
// - Takes a wide string, and determines how many bytes it would take to store it with the given Multibyte codepage.
// Arguments:
// - codepage - Windows Code Page representing the multibyte destination text
// - source - Array of Unicode characters of source text
// Return Value:
// - Length in characters of multibyte buffer that would be required to hold this text after conversion
// - NOTE: Throws suitable HRESULT errors from memory allocation, safe math, or WideCharToMultiByte failures.
[[nodiscard]] size_t GetALengthFromW(const UINT codepage, const std::wstring_view source)
{
    // If there's no bytes, bail early.
    if (source.empty())
    {
        return 0;
    }

    int iSource; // convert to int because Wc2Mb requires it
    THROW_IF_FAILED(SizeTToInt(source.size(), &iSource));

    // Ask how many bytes this string consumes in the other codepage
    // clang-format off
#pragma prefast(suppress: __WARNING_W2A_BEST_FIT, "WC_NO_BEST_FIT_CHARS doesn't work in many codepages. Retain old behavior.")
    // clang-format on
    int const iTarget = WideCharToMultiByte(codepage, 0, source.data(), iSource, nullptr, 0, nullptr, nullptr);
    THROW_LAST_ERROR_IF(0 == iTarget);

    // Convert types safely.
    size_t cchTarget;
    THROW_IF_FAILED(IntToSizeT(iTarget, &cchTarget));

    return cchTarget;
}

std::deque<std::unique_ptr<KeyEvent>> CharToKeyEvents(const wchar_t wch,
                                                      const unsigned int codepage)
{
    const short invalidKey = -1;
    short keyState = VkKeyScanW(wch);

    if (keyState == invalidKey)
    {
        // Determine DBCS character because these character does not know by VkKeyScan.
        // GetStringTypeW(CT_CTYPE3) & C3_ALPHA can determine all linguistic characters. However, this is
        // not include symbolic character for DBCS.
        WORD CharType = 0;
        GetStringTypeW(CT_CTYPE3, &wch, 1, &CharType);

        if (WI_IsFlagSet(CharType, C3_ALPHA) || GetQuickCharWidth(wch) == CodepointWidth::Wide)
        {
            keyState = 0;
        }
    }

    std::deque<std::unique_ptr<KeyEvent>> convertedEvents;
    if (keyState == invalidKey)
    {
        // if VkKeyScanW fails (char is not in kbd layout), we must
        // emulate the key being input through the numpad
        convertedEvents = SynthesizeNumpadEvents(wch, codepage);
    }
    else
    {
        convertedEvents = SynthesizeKeyboardEvents(wch, keyState);
    }

    return convertedEvents;
}

// Routine Description:
// - converts a wchar_t into a series of KeyEvents as if it was typed
// using the keyboard
// Arguments:
// - wch - the wchar_t to convert
// Return Value:
// - deque of KeyEvents that represent the wchar_t being typed
// Note:
// - will throw exception on error
std::deque<std::unique_ptr<KeyEvent>> SynthesizeKeyboardEvents(const wchar_t wch, const short keyState)
{
    const byte modifierState = HIBYTE(keyState);

    bool altGrSet = false;
    bool shiftSet = false;
    std::deque<std::unique_ptr<KeyEvent>> keyEvents;

    // add modifier key event if necessary
    if (WI_AreAllFlagsSet(modifierState, VkKeyScanModState::CtrlAndAltPressed))
    {
        altGrSet = true;
        keyEvents.push_back(std::make_unique<KeyEvent>(true,
                                                       1ui16,
                                                       static_cast<WORD>(VK_MENU),
                                                       altScanCode,
                                                       UNICODE_NULL,
                                                       (ENHANCED_KEY | LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED)));
    }
    else if (WI_IsFlagSet(modifierState, VkKeyScanModState::ShiftPressed))
    {
        shiftSet = true;
        keyEvents.push_back(std::make_unique<KeyEvent>(true,
                                                       1ui16,
                                                       static_cast<WORD>(VK_SHIFT),
                                                       leftShiftScanCode,
                                                       UNICODE_NULL,
                                                       SHIFT_PRESSED));
    }

    const auto vk = LOBYTE(keyState);
    const WORD virtualScanCode = gsl::narrow<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
    KeyEvent keyEvent{ true, 1, LOBYTE(keyState), virtualScanCode, wch, 0 };

    // add modifier flags if necessary
    if (WI_IsFlagSet(modifierState, VkKeyScanModState::ShiftPressed))
    {
        keyEvent.ActivateModifierKey(ModifierKeyState::Shift);
    }
    if (WI_IsFlagSet(modifierState, VkKeyScanModState::CtrlPressed))
    {
        keyEvent.ActivateModifierKey(ModifierKeyState::LeftCtrl);
    }
    if (WI_AreAllFlagsSet(modifierState, VkKeyScanModState::CtrlAndAltPressed))
    {
        keyEvent.ActivateModifierKey(ModifierKeyState::RightAlt);
    }

    // add key event down and up
    keyEvents.push_back(std::make_unique<KeyEvent>(keyEvent));
    keyEvent.SetKeyDown(false);
    keyEvents.push_back(std::make_unique<KeyEvent>(keyEvent));

    // add modifier key up event
    if (altGrSet)
    {
        keyEvents.push_back(std::make_unique<KeyEvent>(false,
                                                       1ui16,
                                                       static_cast<WORD>(VK_MENU),
                                                       altScanCode,
                                                       UNICODE_NULL,
                                                       ENHANCED_KEY));
    }
    else if (shiftSet)
    {
        keyEvents.push_back(std::make_unique<KeyEvent>(false,
                                                       1ui16,
                                                       static_cast<WORD>(VK_SHIFT),
                                                       leftShiftScanCode,
                                                       UNICODE_NULL,
                                                       0));
    }

    return keyEvents;
}

// Routine Description:
// - converts a wchar_t into a series of KeyEvents as if it was typed
// using Alt + numpad
// Arguments:
// - wch - the wchar_t to convert
// Return Value:
// - deque of KeyEvents that represent the wchar_t being typed using
// alt + numpad
// Note:
// - will throw exception on error
std::deque<std::unique_ptr<KeyEvent>> SynthesizeNumpadEvents(const wchar_t wch, const unsigned int codepage)
{
    std::deque<std::unique_ptr<KeyEvent>> keyEvents;

    //alt keydown
    keyEvents.push_back(std::make_unique<KeyEvent>(true,
                                                   1ui16,
                                                   static_cast<WORD>(VK_MENU),
                                                   altScanCode,
                                                   UNICODE_NULL,
                                                   LEFT_ALT_PRESSED));

    const int radix = 10;
    std::wstring wstr{ wch };
    const auto convertedChars = ConvertToA(codepage, wstr);
    if (convertedChars.size() == 1)
    {
        // It is OK if the char is "signed -1", we want to interpret that as "unsigned 255" for the
        // "integer to character" conversion below with ::to_string, thus the static_cast.
        // Prime example is nonbreaking space U+00A0 will convert to OEM by codepage 437 to 0xFF which is -1 signed.
        // But it is absolutely valid as 0xFF or 255 unsigned as the correct CP437 character.
        // We need to treat it as unsigned because we're going to pretend it was a keypad entry
        // and you don't enter negative numbers on the keypad.
        unsigned char const uch = static_cast<unsigned char>(convertedChars.at(0));

        // unsigned char values are in the range [0, 255] so we need to be
        // able to store up to 4 chars from the conversion (including the end of string char)
        auto charString = std::to_string(uch);

        for (auto& ch : std::string_view(charString))
        {
            if (ch == 0)
            {
                break;
            }
            const WORD virtualKey = ch - '0' + VK_NUMPAD0;
            const WORD virtualScanCode = gsl::narrow<WORD>(MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC));

            keyEvents.push_back(std::make_unique<KeyEvent>(true,
                                                           1ui16,
                                                           virtualKey,
                                                           virtualScanCode,
                                                           UNICODE_NULL,
                                                           LEFT_ALT_PRESSED));
            keyEvents.push_back(std::make_unique<KeyEvent>(false,
                                                           1ui16,
                                                           virtualKey,
                                                           virtualScanCode,
                                                           UNICODE_NULL,
                                                           LEFT_ALT_PRESSED));
        }
    }

    // alt keyup
    keyEvents.push_back(std::make_unique<KeyEvent>(false,
                                                   1ui16,
                                                   static_cast<WORD>(VK_MENU),
                                                   altScanCode,
                                                   wch,
                                                   0));
    return keyEvents;
}

// Routine Description:
// - naively determines the width of a UCS2 encoded wchar
// Arguments:
// - wch - the wchar_t to measure
// Return Value:
// - CodepointWidth indicating width of wch
// Notes:
// 04-08-92 ShunK       Created.
// Jul-27-1992 KazuM    Added Screen Information and Code Page Information.
// Jan-29-1992 V-Hirots Substruct Screen Information.
// Oct-06-1996 KazuM    Not use RtlUnicodeToMultiByteSize and WideCharToMultiByte
//                      Because 950 (Chinese Traditional) only defined 13500 chars,
//                     and unicode defined almost 18000 chars.
//                      So there are almost 4000 chars can not be mapped to big5 code.
// Apr-30-2015 MiNiksa  Corrected unknown character code assumption. Max Width in Text Metric
//                      is not reliable for calculating half/full width. Must use current
//                      display font data (cached) instead.
// May-23-2017 migrie   Forced Box-Drawing Characters (x2500-x257F) to narrow.
// Jan-16-2018 migrie   Separated core lookup from asking the renderer the width
// May-01-2019 MiNiksa  Forced lookup-via-renderer for retroactively recategorized emoji
//                      that used to be narrow but now might be wide. (approx x2194-x2b55, not inclusive)
//                      Also forced block characters segment (x2580-x259F) to narrow
// Oct-25-2020 DuHowett Replaced the entire table with a set of overrides that get built into
//                      CodepointWidthDetector (unicode_width_overrides.xml)
CodepointWidth GetQuickCharWidth(const wchar_t wch) noexcept
{
    if (0x20 <= wch && wch <= 0x7e)
    {
        /* ASCII */
        return CodepointWidth::Narrow;
    }
    return CodepointWidth::Invalid;
}

wchar_t Utf16ToUcs2(const std::wstring_view charData)
{
    THROW_HR_IF(E_INVALIDARG, charData.empty());
    if (charData.size() > 1)
    {
        return UNICODE_REPLACEMENT;
    }
    else
    {
        return charData.front();
    }
}
