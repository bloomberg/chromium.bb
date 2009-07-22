// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/ui_localizer.h"

#import <Foundation/Foundation.h>

#include "app/l10n_util.h"
#include "base/sys_string_conversions.h"
#include "base/logging.h"

namespace ui_localizer {

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

NSString* LocalizedStringForKeyFromMapList(NSString* key,
                                           const ResourceMap* map_list,
                                           size_t map_list_len) {
  DCHECK(key != nil);
  DCHECK(map_list != NULL);

  // Look up the string for the resource id to fetch.
  const char* utf8_key = [key UTF8String];
  if (utf8_key) {
    // If we end up with enough string constants in here, look at using bsearch
    // to speed up the searching.
    for (size_t i = 0; i < map_list_len; ++i) {
      int strcmp_result = strcmp(utf8_key, map_list[i].name);
      if (strcmp_result == 0) {
        // Do we need to build the string, or just fetch it?
        if (map_list[i].label_arg_id != 0) {
          const string16 label_arg(
              l10n_util::GetStringUTF16(map_list[i].label_arg_id));
          return FixUpWindowsStyleLabel(
              l10n_util::GetStringFUTF16(map_list[i].label_id, label_arg));
        }

        return FixUpWindowsStyleLabel(
            l10n_util::GetStringUTF16(map_list[i].label_id));
      }

      // If we've passed where the string would be, give up.
      if (strcmp_result < 0)
        break;
    }
  }

  // Sanity check, there shouldn't be any strings with this id that aren't
  // in our map.
  DLOG_IF(WARNING, [key hasPrefix:@"^ID"]) << "Key '" << utf8_key
      << "' wasn't in the resource map?";

  // If we didn't find anything, this string doesn't need localizing.
  return nil;
}

}  // namespace ui_localizer
