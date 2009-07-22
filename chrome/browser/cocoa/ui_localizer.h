// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_UI_LOCALIZER_H_
#define CHROME_BROWSER_COCOA_UI_LOCALIZER_H_

#include "base/basictypes.h"
#include "base/string16.h"
#import "third_party/GTM/AppKit/GTMUILocalizer.h"

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

// A base class for generated localizers.
//
// To use this, have the build run generate_localizer on your xib file (see
// chrome.gyp).  Then add an instance of the generated subclass to the xib.
// Connect the owner_ outlet of the instance to the "File's Owner" of the xib.
// It expects the owner_ outlet to be an instance or subclass of
// NSWindowController or NSViewController.  It will then localize any items in
// the NSWindowController's window and subviews, or the NSViewController's view
// and subviews, when awakeFromNib is called on the instance.  You can
// optionally hook up otherObjectToLocalize_ and yetAnotherObjectToLocalize_ and
// those will also be localized. Strings in the xib that you want localized must
// start with ^IDS. The value must be a valid resource constant.
// Things that will be localized are:
// - Titles and altTitles (for menus, buttons, windows, menuitems, -tabViewItem)
// - -stringValue (for labels)
// - tooltips
// - accessibility help
// - accessibility descriptions
// - menus
@interface ChromeUILocalizer : GTMUILocalizer
@end

#endif  // CHROME_BROWSER_COCOA_UI_LOCALIZER_H_
