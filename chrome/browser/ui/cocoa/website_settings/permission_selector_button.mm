// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/website_settings/permission_selector_button.h"

#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/cocoa/website_settings/website_settings_utils_cocoa.h"
#include "chrome/browser/ui/website_settings/website_settings_ui.h"
#include "chrome/browser/ui/website_settings/website_settings_utils.h"
#import "ui/base/cocoa/menu_controller.h"

@implementation PermissionSelectorButton

- (id)initWithPermissionInfo:
          (const WebsiteSettingsUI::PermissionInfo&)permissionInfo
                      forURL:(const GURL&)url
                withCallback:(PermissionMenuModel::ChangeCallback)callback {
  if (self = [super initWithFrame:NSMakeRect(0, 0, 1, 1) pullsDown:NO]) {
    [self setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
    [self setBordered:NO];
    [[self cell] setControlSize:NSSmallControlSize];

    menuModel_.reset(new PermissionMenuModel(url, permissionInfo, callback));

    menuController_.reset([[MenuController alloc] initWithModel:menuModel_.get()
                                         useWithPopUpButtonCell:NO]);
    [self setMenu:[menuController_ menu]];
    [self selectItemWithTag:permissionInfo.setting];

    // Set the button title.
    base::scoped_nsobject<NSMenuItem> titleItem([[NSMenuItem alloc] init]);
    base::string16 buttonTitle = WebsiteSettingsUI::PermissionActionToUIString(
        permissionInfo.setting,
        permissionInfo.default_setting,
        permissionInfo.source);
    [titleItem setTitle:base::SysUTF16ToNSString(buttonTitle)];
    [[self cell] setUsesItemFromMenu:NO];
    [[self cell] setMenuItem:titleItem.get()];
    // Although the frame is reset, below, this sizes the cell properly.
    [self sizeToFit];

    // Size the button to just fit the visible title - not all of its items.
    [self setFrameSize:SizeForWebsiteSettingsButtonTitle(self, [self title])];
  }
  return self;
}

- (CGFloat)maxTitleWidthWithDefaultSetting:(ContentSetting)defaultSetting {
  // Determine the largest possible size for this button.
  CGFloat maxTitleWidth = 0;
  for (NSMenuItem* item in [self itemArray]) {
    NSString* title =
        base::SysUTF16ToNSString(WebsiteSettingsUI::PermissionActionToUIString(
            static_cast<ContentSetting>([item tag]),
            defaultSetting,
            content_settings::SETTING_SOURCE_USER));
    NSSize size = SizeForWebsiteSettingsButtonTitle(self, title);
    maxTitleWidth = std::max(maxTitleWidth, size.width);
  }
  return maxTitleWidth;
}

// Accessor function for testing only.
- (NSMenu*)permissionMenu {
  return [menuController_ menu];
}

@end
