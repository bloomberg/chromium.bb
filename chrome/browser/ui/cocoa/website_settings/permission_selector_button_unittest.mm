// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/website_settings/permission_selector_button.h"

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/browser/ui/website_settings/website_settings_ui.h"

@interface PermissionSelectorButton (Testing)
- (NSMenu*)permissionMenu;
@end

namespace {

const ContentSettingsType kTestPermissionType =
    CONTENT_SETTINGS_TYPE_MEDIASTREAM;

class PermissionSelectorButtonTest : public CocoaTest {
 public:
  PermissionSelectorButtonTest() {
    got_callback_ = false;
    WebsiteSettingsUI::PermissionInfo test_info;
    test_info.type = kTestPermissionType;
    test_info.setting = CONTENT_SETTING_BLOCK;
    test_info.source = content_settings::SETTING_SOURCE_USER;
    GURL test_url("http://www.google.com");
    PermissionMenuModel::ChangeCallback callback = base::Bind(
        &PermissionSelectorButtonTest::Callback, base::Unretained(this));
    view_.reset(
        [[PermissionSelectorButton alloc] initWithPermissionInfo:test_info
                                                          forURL:test_url
                                                    withCallback:callback]);
    [[test_window() contentView] addSubview:view_];
  }

  void Callback(const WebsiteSettingsUI::PermissionInfo& permission) {
    EXPECT_TRUE(permission.type == kTestPermissionType);
    got_callback_ = true;
  }

  bool got_callback_;
  base::scoped_nsobject<PermissionSelectorButton> view_;
};

TEST_VIEW(PermissionSelectorButtonTest, view_);

TEST_F(PermissionSelectorButtonTest, Callback) {
  NSMenu* menu = [view_ permissionMenu];
  NSMenuItem* item = [menu itemAtIndex:0];
  [[item target] performSelector:[item action] withObject:item];
  EXPECT_TRUE(got_callback_);
}

}  // namespace
