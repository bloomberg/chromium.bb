// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utility functions for dealing with localized
// content.

#ifndef APP_L10N_UTIL_H_
#define APP_L10N_UTIL_H_
#pragma once

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

class PrefService;

namespace l10n_util {

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

// For example, for |locale| = "fr" and |display_locale| = "en",
// it returns "French". To get the display name of
// |locale| in the UI language of Chrome, |display_locale| can be
// set to the return value of g_browser_process->GetApplicationLocale()
// in the UI thread.
// If |is_for_ui| is true, U+200F is appended so that it can be
// rendered properly in a RTL Chrome.
string16 GetDisplayNameForLocale(const std::string& locale,
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
string16 GetStringFUTF16(int message_id,
                         const string16& a,
                         size_t* offset);
string16 GetStringFUTF16(int message_id,
                        const string16& a,
                        const string16& b,
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

// In place sorting of std::wstring strings using collation rules for |locale|.
void SortStrings(const std::string& locale,
                 std::vector<std::wstring>* strings);

// In place sorting of string16 strings using collation rules for |locale|.
void SortStrings16(const std::string& locale,
                   std::vector<string16>* strings);

// Returns a vector of available locale codes. E.g., a vector containing
// en-US, es, fr, fi, pt-PT, pt-BR, etc.
const std::vector<std::string>& GetAvailableLocales();

// Returns a vector of locale codes usable for accept-languages.
void GetAcceptLanguagesForLocale(const std::string& display_locale,
                                 std::vector<std::string>* locale_codes);


}  // namespace l10n_util

#endif  // APP_L10N_UTIL_H_
