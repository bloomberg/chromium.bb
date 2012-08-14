// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/website_settings_bubble_controller.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest_mac.h"

@interface WebsiteSettingsBubbleController (ExposedForTesting)
- (NSView*)permissionsView;
- (NSImageView*)identityStatusIcon;
- (NSTextField*)identityStatusDescriptionField;
- (NSImageView*)connectionStatusIcon;
- (NSTextField*)connectionStatusDescriptionField;
- (NSTextField*)firstVisitDescriptionField;
@end

@implementation WebsiteSettingsBubbleController (ExposedForTesting)
- (NSView*)permissionsView {
  return permissionsView_;
}

- (NSImageView*)identityStatusIcon {
  return identityStatusIcon_;
}

- (NSTextField*)identityStatusDescriptionField {
  return identityStatusDescriptionField_;
}

- (NSImageView*)connectionStatusIcon {
  return connectionStatusIcon_;
}

- (NSTextField*)connectionStatusDescriptionField {
  return connectionStatusDescriptionField_;
}

- (NSTextField*)firstVisitDescriptionField {
  return firstVisitDescriptionField_;
}
@end

namespace {

// Indices of the menu items in the permission menu.
enum PermissionMenuIndices {
  kMenuIndexContentSettingAllow = 0,
  kMenuIndexContentSettingBlock,
  kMenuIndexContentSettingDefault
};

class WebsiteSettingsBubbleControllerTest : public CocoaTest {
 public:
  WebsiteSettingsBubbleControllerTest() {
    controller_ = nil;
  }

  virtual void TearDown() {
    [controller_ close];
    CocoaTest::TearDown();
  }

 protected:
  WebsiteSettingsUIBridge* bridge_;  // Weak, owned by controller.

  enum MatchType {
    TEXT_EQUAL = 0,
    TEXT_NOT_EQUAL
  };

  void CreateBubble() {
    bridge_ = new WebsiteSettingsUIBridge();

    // The controller cleans up after itself when the window closes.
    controller_ =
        [[WebsiteSettingsBubbleController alloc]
            initWithParentWindow:test_window()
            websiteSettingsUIBridge:bridge_];
    window_ = [controller_ window];
    [controller_ showWindow:nil];
  }

  // Return a pointer to the first NSTextField found that either matches, or
  // doesn't match, the given text.
  NSTextField* FindTextField(MatchType match_type, NSString* text) {
    // The window's only immediate child is an invisible view that has a flipped
    // coordinate origin. It is into this that all views get placed.
    NSArray* window_subviews = [[window_ contentView] subviews];
    EXPECT_EQ(1U, [window_subviews count]);
    NSArray* subviews = [[window_subviews lastObject] subviews];

    // Expect 4 views: the identity, identity status, the segmented control
    // (which implements the tab strip), and the tab view.
    EXPECT_EQ(4U, [subviews count]);

    bool desired_result = match_type == TEXT_EQUAL;

    for (NSView* view in subviews) {
      if ([view isKindOfClass:[NSTextField class]]) {
        NSTextField* text_field = static_cast<NSTextField*>(view);
        if ([[text_field stringValue] isEqual:text] == desired_result)
          return text_field;
      }
    }
    return nil;
  }

  WebsiteSettingsBubbleController* controller_;  // Weak, owns self.
  NSWindow* window_;  // Weak, owned by controller.
};

TEST_F(WebsiteSettingsBubbleControllerTest, BasicIdentity) {
  WebsiteSettingsUI::IdentityInfo info;
  info.site_identity = std::string("nhl.com");
  info.identity_status = WebsiteSettings::SITE_IDENTITY_STATUS_UNKNOWN;

  CreateBubble();

  // Test setting the site identity.
  bridge_->SetIdentityInfo(const_cast<WebsiteSettingsUI::IdentityInfo&>(info));
  NSTextField* identity_field = FindTextField(TEXT_EQUAL, @"nhl.com");
  ASSERT_TRUE(identity_field != nil);

  // Test changing the site identity, and ensure that the UI is updated.
  info.site_identity = std::string("google.com");
  bridge_->SetIdentityInfo(const_cast<WebsiteSettingsUI::IdentityInfo&>(info));
  EXPECT_EQ(identity_field, FindTextField(TEXT_EQUAL, @"google.com"));

  // Find the identity status field.
  NSTextField* identity_status_field =
      FindTextField(TEXT_NOT_EQUAL, @"google.com");
  ASSERT_NE(identity_field, identity_status_field);

  // Ensure the text of the identity status field changes when the status does.
  NSString* status = [identity_status_field stringValue];
  info.identity_status = WebsiteSettings::SITE_IDENTITY_STATUS_CERT;
  bridge_->SetIdentityInfo(const_cast<WebsiteSettingsUI::IdentityInfo&>(info));
  EXPECT_NSNE(status, [identity_status_field stringValue]);
}

TEST_F(WebsiteSettingsBubbleControllerTest, SetIdentityInfo) {
  WebsiteSettingsUI::IdentityInfo info;
  info.site_identity = std::string("nhl.com");
  info.identity_status = WebsiteSettings::SITE_IDENTITY_STATUS_UNKNOWN;
  info.identity_status_description = std::string("Identity1");
  info.connection_status = WebsiteSettings::SITE_CONNECTION_STATUS_UNKNOWN;
  info.connection_status_description = std::string("Connection1");

  CreateBubble();

  // Set the identity, and test that the description fields on the Connection
  // tab are set properly.
  bridge_->SetIdentityInfo(const_cast<WebsiteSettingsUI::IdentityInfo&>(info));
  EXPECT_NSEQ(@"Identity1",
              [[controller_ identityStatusDescriptionField] stringValue]);
  EXPECT_NSEQ(@"Connection1",
              [[controller_ connectionStatusDescriptionField] stringValue]);

  // Check the contents of the images, and make sure they change after the
  // status changes.

  NSImage* identity_icon = [[controller_ identityStatusIcon] image];
  NSImage* connection_icon = [[controller_ connectionStatusIcon] image];
  // Icons should be the same when they are both unknown.
  EXPECT_EQ(identity_icon, connection_icon);

  info.identity_status = WebsiteSettings::SITE_IDENTITY_STATUS_CERT;
  info.connection_status = WebsiteSettings::SITE_CONNECTION_STATUS_ENCRYPTED;
  bridge_->SetIdentityInfo(const_cast<WebsiteSettingsUI::IdentityInfo&>(info));

  EXPECT_NE(identity_icon, [[controller_ identityStatusIcon] image]);
  EXPECT_NE(connection_icon, [[controller_ connectionStatusIcon] image]);
}

TEST_F(WebsiteSettingsBubbleControllerTest, SetFirstVisit) {
  CreateBubble();
  bridge_->SetFirstVisit(ASCIIToUTF16("Yesterday"));
  EXPECT_NSEQ(@"Yesterday",
              [[controller_ firstVisitDescriptionField] stringValue]);
}

TEST_F(WebsiteSettingsBubbleControllerTest, SetPermissionInfo) {
  CreateBubble();

  ContentSettingsType kTestPermissionTypes[] = {
    CONTENT_SETTINGS_TYPE_IMAGES,
    CONTENT_SETTINGS_TYPE_JAVASCRIPT,
    CONTENT_SETTINGS_TYPE_PLUGINS,
    CONTENT_SETTINGS_TYPE_POPUPS,
    CONTENT_SETTINGS_TYPE_GEOLOCATION,
    CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
  };

  // Create a list of 6 different permissions, all set to "Allow".
  PermissionInfoList list;
  WebsiteSettingsUI::PermissionInfo info;
  info.setting = CONTENT_SETTING_ALLOW;
  info.default_setting = CONTENT_SETTING_ALLOW;
  for (size_t i = 0; i < arraysize(kTestPermissionTypes); ++i) {
    info.type = kTestPermissionTypes[i];
    list.push_back(info);
  }
  bridge_->SetPermissionInfo(list);

  // There should be three subviews per permission (an icon, a label and a
  // select box), plus a text label for the Permission section.
  NSArray* subviews = [[controller_ permissionsView] subviews];
  EXPECT_EQ(arraysize(kTestPermissionTypes) * 3 + 1, [subviews count]);

  // Ensure that there is a distinct label for each permission.
  NSMutableSet* labels = [NSMutableSet set];
  for (NSView* view in subviews) {
    if ([view isKindOfClass:[NSTextField class]])
      [labels addObject:[static_cast<NSTextField*>(view) stringValue]];
  }
  // The section header ("Permissions") will also be found, hence the +1.
  EXPECT_EQ(arraysize(kTestPermissionTypes) + 1, [labels count]);

  // Find the first permission pop-up button
  NSPopUpButton* button = nil;
  for (NSView* view in subviews) {
    if ([view isKindOfClass:[NSPopUpButton class]]) {
      button = static_cast<NSPopUpButton*>(view);
      break;
    }
  }
  ASSERT_NSNE(nil, button);

  // Check that the button title is updated when the setting is changed.

  NSString* original_title = [[[button cell] menuItem] title];
  [button selectItemAtIndex:kMenuIndexContentSettingBlock];
  [controller_ permissionValueChanged:button];
  EXPECT_NSNE(original_title, [[[button cell] menuItem] title]);

  [button selectItemAtIndex:kMenuIndexContentSettingDefault];
  [controller_ permissionValueChanged:button];
  EXPECT_NSNE(original_title, [[[button cell] menuItem] title]);

  [button selectItemAtIndex:kMenuIndexContentSettingAllow];
  [controller_ permissionValueChanged:button];
  EXPECT_NSEQ(original_title, [[[button cell] menuItem] title]);
}

}  // namespace
