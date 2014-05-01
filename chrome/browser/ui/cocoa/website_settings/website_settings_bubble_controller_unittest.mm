// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/website_settings/website_settings_bubble_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest_mac.h"

@interface WebsiteSettingsBubbleController (ExposedForTesting)
- (NSSegmentedControl*)segmentedControl;
- (NSTabView*)tabView;
- (NSView*)permissionsView;
- (NSView*)connectionTabContentView;
- (NSImageView*)identityStatusIcon;
- (NSTextField*)identityStatusDescriptionField;
- (NSImageView*)connectionStatusIcon;
- (NSTextField*)connectionStatusDescriptionField;
- (NSTextField*)firstVisitDescriptionField;
- (NSButton*)helpButton;
@end

@implementation WebsiteSettingsBubbleController (ExposedForTesting)
- (NSSegmentedControl*)segmentedControl {
  return segmentedControl_.get();
}
- (NSTabView*)tabView {
  return tabView_.get();
}
- (NSView*)permissionsView {
  return permissionsView_;
}

- (NSView*)connectionTabContentView {
  return connectionTabContentView_;
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

- (NSButton*)helpButton {
  return helpButton_;
}
@end

@interface WebsiteSettingsBubbleControllerForTesting
    : WebsiteSettingsBubbleController {
 @private
  CGFloat defaultWindowWidth_;
}
@end

@implementation WebsiteSettingsBubbleControllerForTesting
- (void)setDefaultWindowWidth:(CGFloat)width {
  defaultWindowWidth_ = width;
}
- (CGFloat)defaultWindowWidth {
  // If |defaultWindowWidth_| is 0, use the superclass implementation.
  return defaultWindowWidth_ ?
      defaultWindowWidth_ : [super defaultWindowWidth];
}
@end

namespace {

// Indices of the menu items in the permission menu.
enum PermissionMenuIndices {
  kMenuIndexContentSettingAllow = 0,
  kMenuIndexContentSettingBlock,
  kMenuIndexContentSettingDefault
};

const ContentSettingsType kTestPermissionTypes[] = {
  // NOTE: FULLSCREEN does not support "Always block", so it must appear as
  // one of the first three permissions.
  CONTENT_SETTINGS_TYPE_FULLSCREEN,
  CONTENT_SETTINGS_TYPE_IMAGES,
  CONTENT_SETTINGS_TYPE_JAVASCRIPT,
  CONTENT_SETTINGS_TYPE_PLUGINS,
  CONTENT_SETTINGS_TYPE_POPUPS,
  CONTENT_SETTINGS_TYPE_GEOLOCATION,
  CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
  CONTENT_SETTINGS_TYPE_MOUSELOCK,
  CONTENT_SETTINGS_TYPE_MEDIASTREAM,
};

const ContentSetting kTestSettings[] = {
  CONTENT_SETTING_DEFAULT,
  CONTENT_SETTING_DEFAULT,
  CONTENT_SETTING_DEFAULT,
  CONTENT_SETTING_ALLOW,
  CONTENT_SETTING_BLOCK,
  CONTENT_SETTING_ALLOW,
  CONTENT_SETTING_BLOCK,
  CONTENT_SETTING_ALLOW,
  CONTENT_SETTING_BLOCK,
};

const ContentSetting kTestDefaultSettings[] = {
  CONTENT_SETTING_ALLOW,
  CONTENT_SETTING_BLOCK,
  CONTENT_SETTING_ASK
};

const content_settings::SettingSource kTestSettingSources[] = {
  content_settings::SETTING_SOURCE_USER,
  content_settings::SETTING_SOURCE_USER,
  content_settings::SETTING_SOURCE_USER,
  content_settings::SETTING_SOURCE_USER,
  content_settings::SETTING_SOURCE_USER,
  content_settings::SETTING_SOURCE_POLICY,
  content_settings::SETTING_SOURCE_POLICY,
  content_settings::SETTING_SOURCE_EXTENSION,
  content_settings::SETTING_SOURCE_EXTENSION,
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

  // Creates a new website settings bubble, with the given default width.
  // If |default_width| is 0, the *default* default width will be used.
  void CreateBubbleWithWidth(CGFloat default_width) {
    bridge_ = new WebsiteSettingsUIBridge();

    // The controller cleans up after itself when the window closes.
    controller_ = [WebsiteSettingsBubbleControllerForTesting alloc];
    [controller_ setDefaultWindowWidth:default_width];
    [controller_ initWithParentWindow:test_window()
                 websiteSettingsUIBridge:bridge_
                 webContents:nil
                 isInternalPage:NO];
    window_ = [controller_ window];
    [controller_ showWindow:nil];
  }

  void CreateBubble() {
    CreateBubbleWithWidth(0.0);
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

  NSMutableArray* FindAllSubviewsOfClass(NSView* parent_view, Class a_class) {
    NSMutableArray* views = [NSMutableArray array];
    for (NSView* view in [parent_view subviews]) {
      if ([view isKindOfClass:a_class])
        [views addObject:view];
    }
    return views;
  }

  // Sets up the dialog with some test permission settings.
  void SetTestPermissions() {
    // Create a list of 5 different permissions, corresponding to all the
    // possible settings:
    // - [allow, block, ask] by default
    // - [block, allow] * [by user, by policy, by extension]
    PermissionInfoList list;
    WebsiteSettingsUI::PermissionInfo info;
    for (size_t i = 0; i < arraysize(kTestPermissionTypes); ++i) {
      info.type = kTestPermissionTypes[i];
      info.setting = kTestSettings[i];
      if (info.setting == CONTENT_SETTING_DEFAULT)
        info.default_setting = kTestDefaultSettings[i];
      info.source = kTestSettingSources[i];
      list.push_back(info);
    }
    bridge_->SetPermissionInfo(list);
  }

  WebsiteSettingsBubbleControllerForTesting* controller_;  // Weak, owns self.
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
  info.cert_id = 0;

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

  // Ensure that the link button for certificate info is not there -- the
  // help link button should be the first one found.
  NSMutableArray* buttons = FindAllSubviewsOfClass(
      [controller_ connectionTabContentView], [NSButton class]);
  ASSERT_EQ(1U, [buttons count]);
  EXPECT_NSEQ([controller_ helpButton], [buttons objectAtIndex:0]);

  // Check that it has a target and action linked up.
  NSButton* link_button = static_cast<NSButton*>([buttons objectAtIndex:0]);
  EXPECT_NSEQ(controller_, [link_button target]);
  EXPECT_TRUE([link_button action] == @selector(showHelpPage:));

  info.identity_status = WebsiteSettings::SITE_IDENTITY_STATUS_CERT;
  info.connection_status = WebsiteSettings::SITE_CONNECTION_STATUS_ENCRYPTED;
  info.cert_id = 1;
  bridge_->SetIdentityInfo(const_cast<WebsiteSettingsUI::IdentityInfo&>(info));

  EXPECT_NE(identity_icon, [[controller_ identityStatusIcon] image]);
  EXPECT_NE(connection_icon, [[controller_ connectionStatusIcon] image]);

  // The certificate info button should be there now.
  buttons = FindAllSubviewsOfClass(
      [controller_ connectionTabContentView], [NSButton class]);
  ASSERT_EQ(2U, [buttons count]);
  EXPECT_NSNE([controller_ helpButton], [buttons objectAtIndex:1]);

  // Check that it has a target and action linked up.
  link_button = static_cast<NSButton*>([buttons objectAtIndex:1]);
  EXPECT_NSEQ(controller_, [link_button target]);
  EXPECT_TRUE([link_button action] == @selector(showCertificateInfo:));
}

TEST_F(WebsiteSettingsBubbleControllerTest, SetFirstVisit) {
  CreateBubble();
  bridge_->SetFirstVisit(base::ASCIIToUTF16("Yesterday"));
  EXPECT_NSEQ(@"Yesterday",
              [[controller_ firstVisitDescriptionField] stringValue]);
}

TEST_F(WebsiteSettingsBubbleControllerTest, SetPermissionInfo) {
  CreateBubble();
  SetTestPermissions();

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

  // Ensure that the button labels are distinct, and look for the correct
  // number of disabled buttons.
  int disabled_count = 0;
  [labels removeAllObjects];
  for (NSView* view in subviews) {
    if ([view isKindOfClass:[NSPopUpButton class]]) {
      NSPopUpButton* button = static_cast<NSPopUpButton*>(view);
      [labels addObject:[[button selectedCell] title]];

      if (![button isEnabled])
        ++disabled_count;
    }
  }
  EXPECT_EQ(arraysize(kTestPermissionTypes), [labels count]);

  // 4 of the buttons should be disabled -- the ones that have a setting source
  // of SETTING_SOURCE_POLICY or SETTING_SOURCE_EXTENSION.
  EXPECT_EQ(4, disabled_count);
}

TEST_F(WebsiteSettingsBubbleControllerTest, SetSelectedTab) {
  CreateBubble();
  NSSegmentedControl* segmentedControl = [controller_ segmentedControl];
  NSTabView* tabView = [controller_ tabView];

  // Test whether SetSelectedTab properly changes both the segmented control
  // (which implements the tabs) as well as the visible tab contents.
  // NOTE: This implicitly (and deliberately) tests that the tabs appear in a
  // specific order: Permissions, Connection.
  EXPECT_EQ(0, [segmentedControl selectedSegment]);
  EXPECT_EQ(0, [tabView indexOfTabViewItem:[tabView selectedTabViewItem]]);
  bridge_->SetSelectedTab(WebsiteSettingsUI::TAB_ID_CONNECTION);
  EXPECT_EQ(1, [segmentedControl selectedSegment]);
  EXPECT_EQ(1, [tabView indexOfTabViewItem:[tabView selectedTabViewItem]]);
}

TEST_F(WebsiteSettingsBubbleControllerTest, WindowWidth) {
  // Try creating a window that is obviously too small.
  CreateBubbleWithWidth(30.0);
  SetTestPermissions();

  CGFloat window_width = NSWidth([[controller_ window] frame]);

  // Check the window was made bigger to fit the content.
  EXPECT_LT(30.0, window_width);

  // Check that the window is wider than the right edge of all the permission
  // popup buttons.
  for (NSView* view in [[controller_ permissionsView] subviews]) {
    if ([view isKindOfClass:[NSPopUpButton class]]) {
      NSPopUpButton* button = static_cast<NSPopUpButton*>(view);
      EXPECT_LT(NSMaxX([button frame]), window_width);
    }
  }
}

}  // namespace
