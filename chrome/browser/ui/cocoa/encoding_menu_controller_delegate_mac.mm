// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/encoding_menu_controller_delegate_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/string16.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/toolbar/encoding_menu_controller.h"

namespace {

void AddSeparatorToMenu(NSMenu *parent_menu) {
  NSMenuItem* separator = [NSMenuItem separatorItem];
  [parent_menu addItem:separator];
}

void AppendMenuItem(NSMenu *parent_menu, int tag, NSString *title) {

  NSMenuItem* item = [[[NSMenuItem alloc] initWithTitle:title
                                                 action:nil
                                          keyEquivalent:@""] autorelease];
  [parent_menu addItem:item];
  [item setAction:@selector(commandDispatch:)];
  [item setTag:tag];
}

}  // namespace

// static
void EncodingMenuControllerDelegate::BuildEncodingMenu(Profile *profile,
                                                       NSMenu* encoding_menu) {
  DCHECK(profile);

  typedef EncodingMenuController::EncodingMenuItemList EncodingMenuItemList;
  EncodingMenuItemList menuItems;
  EncodingMenuController controller;
  controller.GetEncodingMenuItems(profile, &menuItems);

  for (EncodingMenuItemList::iterator it = menuItems.begin();
       it != menuItems.end();
       ++it) {
    int item_id = it->first;
    string16 &localized_title_string16 = it->second;

    if (item_id == 0) {
      AddSeparatorToMenu(encoding_menu);
    } else {
      NSString *localized_title =
          base::SysUTF16ToNSString(localized_title_string16);
      AppendMenuItem(encoding_menu, item_id, localized_title);
    }
  }

}
