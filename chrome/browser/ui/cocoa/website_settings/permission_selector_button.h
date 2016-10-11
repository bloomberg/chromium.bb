// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_WEBSITE_SETTINGS_PERMISSION_SELECTOR_BUTTON_H_
#define CHROME_BROWSER_UI_COCOA_WEBSITE_SETTINGS_PERMISSION_SELECTOR_BUTTON_H_

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/website_settings/permission_menu_model.h"
#include "components/content_settings/core/common/content_settings.h"

@class MenuController;

class Profile;

@interface PermissionSelectorButton : NSPopUpButton {
 @private
  std::unique_ptr<PermissionMenuModel> menuModel_;
  base::scoped_nsobject<MenuController> menuController_;
}

// Designated initializer.
- (id)initWithPermissionInfo:
          (const WebsiteSettingsUI::PermissionInfo&)permissionInfo
                      forURL:(const GURL&)url
                withCallback:(PermissionMenuModel::ChangeCallback)callback
                     profile:(Profile*)profile;

// Returns the largest possible size given all of the items in the menu.
- (CGFloat)maxTitleWidthForContentSettingsType:(ContentSettingsType)type
                            withDefaultSetting:(ContentSetting)defaultSetting
                                       profile:(Profile*)profile;

@end

#endif  // CHROME_BROWSER_UI_COCOA_WEBSITE_SETTINGS_PERMISSION_SELECTOR_BUTTON_H_
