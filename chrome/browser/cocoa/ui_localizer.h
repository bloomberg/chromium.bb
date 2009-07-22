// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_UI_LOCALIZER_H_
#define CHROME_BROWSER_COCOA_UI_LOCALIZER_H_

#include "base/basictypes.h"
#include "base/string16.h"

@class NSString;

namespace ui_localizer {

// Remove the Windows-style accelerator marker and change "..." into an
// ellipsis.  Returns the result in an autoreleased NSString.
NSString* FixUpWindowsStyleLabel(const string16& label);

struct ResourceMap {
  const char* const name;
  unsigned int label_id;
  unsigned int label_arg_id;
};

NSString* LocalizedStringForKeyFromMapList(NSString* key,
                                           const ResourceMap* map_list,
                                           size_t map_list_len);

}  // namespace ui_localizer

#endif  // CHROME_BROWSER_COCOA_UI_LOCALIZER_H_
