// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#include "app/l10n_util_mac.h"
#include "base/sys_string_conversions.h"

namespace l10n_util {

std::string GetApplicationLocale(const std::wstring& pref_locale) {
  // NOTE: The Win/Linux version of this calls out to CheckAndResolveLocale
  // to do some remapping. Since Mac is using real locales that Cocoa has
  // to be able to load, that shouldn't be needed.
  return [[[NSLocale currentLocale] localeIdentifier] UTF8String];
}

// Remove the Windows-style accelerator marker and change "..." into an
// ellipsis.  Returns the result in an autoreleased NSString.
NSString* FixUpWindowsStyleLabel(const string16& label) {
  const char16 kEllipsisUTF16 = 0x2026;
  string16 ret;
  size_t label_len = label.length();
  ret.reserve(label_len);
  for (size_t i = 0; i < label_len; ++i) {
    char16 c = label[i];
    if (c == '&') {
      if (i + 1 < label_len && label[i + 1] == '&') {
        ret.push_back(c);
        ++i;
      }
    } else if (c == '.' && i + 2 < label_len && label[i + 1] == '.'
               && label[i + 2] == '.') {
      ret.push_back(kEllipsisUTF16);
      i += 2;
    } else {
      ret.push_back(c);
    }
  }

  return base::SysUTF16ToNSString(ret);
}

NSString* GetNSString(int message_id) {
  return base::SysUTF16ToNSString(l10n_util::GetStringUTF16(message_id));
}

NSString* GetNSStringF(int message_id,
                       const string16& a) {
  return base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(message_id,
                                                             a));
}

NSString* GetNSStringF(int message_id,
                       const string16& a,
                       const string16& b) {
  return base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(message_id,
                                                             a, b));
}

NSString* GetNSStringF(int message_id,
                       const string16& a,
                       const string16& b,
                       const string16& c) {
  return base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(message_id,
                                                             a, b, c));
}

NSString* GetNSStringF(int message_id,
                       const string16& a,
                       const string16& b,
                       const string16& c,
                       const string16& d) {
  return base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(message_id,
                                                             a, b, c, d));
}

NSString* GetNSStringWithFixup(int message_id) {
  return FixUpWindowsStyleLabel(l10n_util::GetStringUTF16(message_id));
}

NSString* GetNSStringFWithFixup(int message_id,
                                const string16& a) {
  return FixUpWindowsStyleLabel(l10n_util::GetStringFUTF16(message_id,
                                                           a));
}

NSString* GetNSStringFWithFixup(int message_id,
                                const string16& a,
                                const string16& b) {
  return FixUpWindowsStyleLabel(l10n_util::GetStringFUTF16(message_id,
                                                           a, b));
}

NSString* GetNSStringFWithFixup(int message_id,
                                const string16& a,
                                const string16& b,
                                const string16& c) {
  return FixUpWindowsStyleLabel(l10n_util::GetStringFUTF16(message_id,
                                                           a, b, c));
}

NSString* GetNSStringFWithFixup(int message_id,
                                const string16& a,
                                const string16& b,
                                const string16& c,
                                const string16& d) {
  return FixUpWindowsStyleLabel(l10n_util::GetStringFUTF16(message_id,
                                                           a, b, c, d));
}


}  // namespace l10n_util
