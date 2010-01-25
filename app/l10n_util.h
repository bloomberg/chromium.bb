// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utility functions for dealing with localized
// content.

#ifndef APP_L10N_UTIL_H_
#define APP_L10N_UTIL_H_

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include "build/build_config.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/string_util.h"

#if defined(OS_MACOSX)
#include "app/l10n_util_mac.h"
#endif  // OS_MACOSX

class FilePath;
class PrefService;

namespace l10n_util {

const char16 kRightToLeftMark = 0x200f;
const char16 kLeftToRightMark = 0x200e;
const char16 kLeftToRightEmbeddingMark = 0x202A;
const char16 kRightToLeftEmbeddingMark = 0x202B;
const char16 kPopDirectionalFormatting = 0x202C;

// This method is responsible for determining the locale as defined below. In
// nearly all cases you shouldn't call this, rather use GetApplicationLocale
// defined on browser_process.
//
// Returns the locale used by the Application.  First we use the value from the
// command line (--lang), second we try the value in the prefs file (passed in
// as |pref_locale|), finally, we fall back on the system locale. We only return
// a value if there's a corresponding resource DLL for the locale.  Otherwise,
// we fall back to en-us.
std::string GetApplicationLocale(const std::wstring& pref_locale);

// Given a locale code, return true if the OS is capable of supporting it.
// For instance, Oriya is not well supported on Windows XP and we return
// false for "or".
bool IsLocaleSupportedByOS(const std::string& locale);

// This method returns the display name of the locale code in |display_locale|.

// For example, for |locale_code| = "en-US" and |display_locale| = "en",
// it returns "English (United States)". To get the display name of
// |locale_code| in the UI language of Chrome, |display_locale| can be
// set to the return value of g_browser_process->GetApplicationLocale()
// in the UI thread.
// If |is_for_ui| is true, U+200F is appended so that it can be
// rendered properly in a RTL Chrome.
string16 GetDisplayNameForLocale(const std::string& locale_code,
                                 const std::string& display_locale,
                                 bool is_for_ui);

//
// Mac Note: See l10n_util_mac.h for some NSString versions and other support.
//

// Pulls resource string from the string bundle and returns it.
std::wstring GetString(int message_id);
std::string GetStringUTF8(int message_id);
string16 GetStringUTF16(int message_id);

// Get a resource string and replace $1-$2-$3 with |a| and |b|
// respectively.  Additionally, $$ is replaced by $.
string16 GetStringFUTF16(int message_id,
                         const string16& a);
string16 GetStringFUTF16(int message_id,
                         const string16& a,
                         const string16& b);
string16 GetStringFUTF16(int message_id,
                         const string16& a,
                         const string16& b,
                         const string16& c);
string16 GetStringFUTF16(int message_id,
                         const string16& a,
                         const string16& b,
                         const string16& c,
                         const string16& d);
#if defined(WCHAR_T_IS_UTF16)
inline std::wstring GetStringF(int message_id,
                               const std::wstring& a) {
  return GetStringFUTF16(message_id, a);
}
inline std::wstring GetStringF(int message_id,
                               const std::wstring& a,
                               const std::wstring& b) {
  return GetStringFUTF16(message_id, a, b);
}
inline std::wstring GetStringF(int message_id,
                               const std::wstring& a,
                               const std::wstring& b,
                               const std::wstring& c) {
  return GetStringFUTF16(message_id, a, b, c);
}
inline std::wstring GetStringF(int message_id,
                               const std::wstring& a,
                               const std::wstring& b,
                               const std::wstring& c,
                               const std::wstring& d) {
  return GetStringFUTF16(message_id, a, b, c, d);
}
#else
std::wstring GetStringF(int message_id,
                        const std::wstring& a);
std::wstring GetStringF(int message_id,
                        const std::wstring& a,
                        const std::wstring& b);
std::wstring GetStringF(int message_id,
                        const std::wstring& a,
                        const std::wstring& b,
                        const std::wstring& c);
std::wstring GetStringF(int message_id,
                        const std::wstring& a,
                        const std::wstring& b,
                        const std::wstring& c,
                        const std::wstring& d);
#endif
std::string GetStringFUTF8(int message_id,
                           const string16& a);
std::string GetStringFUTF8(int message_id,
                           const string16& a,
                           const string16& b);
std::string GetStringFUTF8(int message_id,
                           const string16& a,
                           const string16& b,
                           const string16& c);
std::string GetStringFUTF8(int message_id,
                           const string16& a,
                           const string16& b,
                           const string16& c,
                           const string16& d);

// Variants that return the offset(s) of the replaced parameters. The
// vector based version returns offsets ordered by parameter. For example if
// invoked with a and b offsets[0] gives the offset for a and offsets[1] the
// offset of b regardless of where the parameters end up in the string.
std::wstring GetStringF(int message_id,
                        const std::wstring& a,
                        size_t* offset);
std::wstring GetStringF(int message_id,
                        const std::wstring& a,
                        const std::wstring& b,
                        std::vector<size_t>* offsets);

// Convenience formatters for a single number.
std::wstring GetStringF(int message_id, int a);
std::wstring GetStringF(int message_id, int64 a);

// Truncates the string to length characters. This breaks the string at
// the first word break before length, adding the horizontal ellipsis
// character (unicode character 0x2026) to render ...
// The supplied string is returned if the string has length characters or
// less.
std::wstring TruncateString(const std::wstring& string, size_t length);

// Returns the lower case equivalent of string.
#if defined(WCHAR_T_IS_UTF32)
// Deprecated.  The string16 version should be used instead.
std::wstring ToLower(const std::wstring& string);
#endif  // defined(WCHAR_T_IS_UTF32)
string16 ToLower(const string16& string);

// Returns the upper case equivalent of string.
string16 ToUpper(const string16& string);

// Represents the text direction returned by the GetTextDirection() function.
enum TextDirection {
  UNKNOWN_DIRECTION,
  RIGHT_TO_LEFT,
  LEFT_TO_RIGHT,
};

// Returns the text direction for the default ICU locale. It is assumed
// that SetICUDefaultLocale has been called to set the default locale to
// the UI locale of Chrome. Its return is one of the following three:
//  * LEFT_TO_RIGHT: Left-To-Right (e.g. English, Chinese, etc.);
//  * RIGHT_TO_LEFT: Right-To-Left (e.g. Arabic, Hebrew, etc.), and;
//  * UNKNOWN_DIRECTION: unknown (or error).
TextDirection GetTextDirection();

// Returns the text direction for |locale_name|.
TextDirection GetTextDirectionForLocale(const char* locale_name);

// Given the string in |text|, returns the directionality of the first
// character with strong directionality in the string. If no character in the
// text has strong directionality, LEFT_TO_RIGHT is returned. The Bidi
// character types L, LRE, LRO, R, AL, RLE, and RLO are considered as strong
// directionality characters. Please refer to http://unicode.org/reports/tr9/
// for more information.
TextDirection GetFirstStrongCharacterDirection(const std::wstring& text);

// Given the string in |text|, this function creates a copy of the string with
// the appropriate Unicode formatting marks that mark the string direction
// (either left-to-right or right-to-left). The new string is returned in
// |localized_text|. The function checks both the current locale and the
// contents of the string in order to determine the direction of the returned
// string. The function returns true if the string in |text| was properly
// adjusted.
//
// Certain LTR strings are not rendered correctly when the context is RTL. For
// example, the string "Foo!" will appear as "!Foo" if it is rendered as is in
// an RTL context. Calling this function will make sure the returned localized
// string is always treated as a right-to-left string. This is done by
// inserting certain Unicode formatting marks into the returned string.
//
// TODO(idana) bug# 1206120: this function adjusts the string in question only
// if the current locale is right-to-left. The function does not take care of
// the opposite case (an RTL string displayed in an LTR context) since
// adjusting the string involves inserting Unicode formatting characters that
// Windows does not handle well unless right-to-left language support is
// installed. Since the English version of Windows doesn't have right-to-left
// language support installed by default, inserting the direction Unicode mark
// results in Windows displaying squares.
bool AdjustStringForLocaleDirection(const std::wstring& text,
                                    std::wstring* localized_text);

// Returns true if the string contains at least one character with strong right
// to left directionality; that is, a character with either R or AL Unicode
// BiDi character type.
bool StringContainsStrongRTLChars(const std::wstring& text);

// Wraps a string with an LRE-PDF pair which essentialy marks the string as a
// Left-To-Right string. Doing this is useful in order to make sure LTR
// strings are rendered properly in an RTL context.
void WrapStringWithLTRFormatting(std::wstring* text);

// Wraps a string with an RLE-PDF pair which essentialy marks the string as a
// Right-To-Left string. Doing this is useful in order to make sure RTL
// strings are rendered properly in an LTR context.
void WrapStringWithRTLFormatting(std::wstring* text);

// Wraps file path to get it to display correctly in RTL UI. All filepaths
// should be passed through this function before display in UI for RTL locales.
void WrapPathWithLTRFormatting(const FilePath& path,
                               string16* rtl_safe_path);

// Given the string in |text|, this function returns the adjusted string having
// LTR directionality for display purpose. Which means that in RTL locale the
// string is wrapped with LRE (Left-To-Right Embedding) and PDF (Pop
// Directional Formatting) marks and returned. In LTR locale, the string itself
// is returned.
std::wstring GetDisplayStringInLTRDirectionality(std::wstring* text);

// Returns the default text alignment to be used when drawing text on a
// gfx::Canvas based on the directionality of the system locale language. This
// function is used by gfx::Canvas::DrawStringInt when the text alignment is
// not specified.
//
// This function returns either gfx::Canvas::TEXT_ALIGN_LEFT or
// gfx::Canvas::TEXT_ALIGN_RIGHT.
int DefaultCanvasTextAlignment();

// In place sorting of strings using collation rules for |locale|.
// TODO(port): this should take string16.
void SortStrings(const std::string& locale,
                 std::vector<std::wstring>* strings);

// Returns a vector of available locale codes. E.g., a vector containing
// en-US, es, fr, fi, pt-PT, pt-BR, etc.
const std::vector<std::string>& GetAvailableLocales();

// Returns a vector of locale codes usable for accept-languages.
void GetAcceptLanguagesForLocale(const std::string& display_locale,
                                 std::vector<std::string>* locale_codes);


}  // namespace l10n_util

#endif  // APP_L10N_UTIL_H_
