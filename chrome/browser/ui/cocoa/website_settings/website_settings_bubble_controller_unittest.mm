// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/website_settings/website_settings_bubble_controller.h"

#include <stddef.h>

#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_web_contents_factory.h"
#include "net/test/test_certificate_data.h"
#include "testing/gtest_mac.h"

@interface WebsiteSettingsBubbleController (ExposedForTesting)
- (NSView*)permissionsView;
- (NSButton*)resetDecisionsButton;
- (NSButton*)connectionHelpButton;
@end

@implementation WebsiteSettingsBubbleController (ExposedForTesting)
- (NSView*)permissionsView {
  return permissionsView_;
}
- (NSButton*)resetDecisionsButton {
  return resetDecisionsButton_;
}
- (NSButton*)connectionHelpButton {

  return connectionHelpButton_;
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
  CONTENT_SETTINGS_TYPE_IMAGES,
  CONTENT_SETTINGS_TYPE_JAVASCRIPT,
  CONTENT_SETTINGS_TYPE_PLUGINS,
  CONTENT_SETTINGS_TYPE_POPUPS,
  CONTENT_SETTINGS_TYPE_GEOLOCATION,
  CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
  CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC
};

const ContentSetting kTestSettings[] = {
  CONTENT_SETTING_DEFAULT,
  CONTENT_SETTING_DEFAULT,
  CONTENT_SETTING_ALLOW,
  CONTENT_SETTING_BLOCK,
  CONTENT_SETTING_ALLOW,
  CONTENT_SETTING_BLOCK,
  CONTENT_SETTING_BLOCK
};

const ContentSetting kTestDefaultSettings[] = {
  CONTENT_SETTING_BLOCK,
  CONTENT_SETTING_ASK
};

const content_settings::SettingSource kTestSettingSources[] = {
  content_settings::SETTING_SOURCE_USER,
  content_settings::SETTING_SOURCE_USER,
  content_settings::SETTING_SOURCE_USER,
  content_settings::SETTING_SOURCE_USER,
  content_settings::SETTING_SOURCE_POLICY,
  content_settings::SETTING_SOURCE_POLICY,
  content_settings::SETTING_SOURCE_EXTENSION
};

class WebsiteSettingsBubbleControllerTest : public CocoaTest {
 public:
  WebsiteSettingsBubbleControllerTest() {
    controller_ = nil;
  }

  void TearDown() override {
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
    bridge_ = new WebsiteSettingsUIBridge(nullptr);

    // The controller cleans up after itself when the window closes.
    controller_ = [WebsiteSettingsBubbleControllerForTesting alloc];
    [controller_ setDefaultWindowWidth:default_width];
    [controller_ initWithParentWindow:test_window()
              websiteSettingsUIBridge:bridge_
                          webContents:web_contents_factory_.CreateWebContents(
                                          &profile_)
                                  url:GURL("https://www.google.com")];
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
    NSArray* bubble_subviews = [[window_subviews lastObject] subviews];
    NSArray* security_section_subviews =
        [[bubble_subviews firstObject] subviews];

    /**
     *Expect 3 views:
     * - the identity
     * - identity status
     * - security details link
     */
    EXPECT_EQ(3U, [security_section_subviews count]);

    bool desired_result = match_type == TEXT_EQUAL;
    for (NSView* view in security_section_subviews) {
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
    PermissionInfoList permission_info_list;
    WebsiteSettingsUI::PermissionInfo info;
    for (size_t i = 0; i < arraysize(kTestPermissionTypes); ++i) {
      info.type = kTestPermissionTypes[i];
      info.setting = kTestSettings[i];
      if (info.setting == CONTENT_SETTING_DEFAULT)
        info.default_setting = kTestDefaultSettings[i];
      info.source = kTestSettingSources[i];
      info.is_incognito = false;
      permission_info_list.push_back(info);
    }
    ChosenObjectInfoList chosen_object_info_list;
    bridge_->SetPermissionInfo(permission_info_list,
                               std::move(chosen_object_info_list));
  }

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  content::TestWebContentsFactory web_contents_factory_;

  WebsiteSettingsBubbleControllerForTesting* controller_;  // Weak, owns self.
  NSWindow* window_;  // Weak, owned by controller.
};

TEST_F(WebsiteSettingsBubbleControllerTest, ConnectionHelpButton) {
  WebsiteSettingsUI::IdentityInfo info;
  info.site_identity = std::string("example.com");
  info.identity_status = WebsiteSettings::SITE_IDENTITY_STATUS_UNKNOWN;

  CreateBubble();

  bridge_->SetIdentityInfo(const_cast<WebsiteSettingsUI::IdentityInfo&>(info));

  EXPECT_EQ([[controller_ connectionHelpButton] action],
            @selector(openConnectionHelp:));
}

TEST_F(WebsiteSettingsBubbleControllerTest, ResetDecisionsButton) {
  WebsiteSettingsUI::IdentityInfo info;
  info.site_identity = std::string("example.com");
  info.identity_status = WebsiteSettings::SITE_IDENTITY_STATUS_UNKNOWN;

  CreateBubble();

  // Set identity info, specifying that the button should not be shown.
  info.show_ssl_decision_revoke_button = false;
  bridge_->SetIdentityInfo(const_cast<WebsiteSettingsUI::IdentityInfo&>(info));
  EXPECT_EQ([controller_ resetDecisionsButton], nil);

  // Set identity info, specifying that the button should be shown.
  info.certificate = net::X509Certificate::CreateFromBytes(
      reinterpret_cast<const char*>(google_der), sizeof(google_der));
  info.show_ssl_decision_revoke_button = true;
  bridge_->SetIdentityInfo(const_cast<WebsiteSettingsUI::IdentityInfo&>(info));
  EXPECT_NE([controller_ resetDecisionsButton], nil);

  // Check that clicking the button calls the right selector.
  EXPECT_EQ([[controller_ resetDecisionsButton] action],
            @selector(resetCertificateDecisions:));

  // Since the bubble is only created once per identity, we only need to check
  // the button is *added* when needed. So we don't check that it's removed
  // when we set an identity with `show_ssl_decision_revoke_button == false`
  // again.
}

TEST_F(WebsiteSettingsBubbleControllerTest, SetPermissionInfo) {
  CreateBubble();
  SetTestPermissions();

  // There should be three subviews per permission.
  NSArray* subviews = [[controller_ permissionsView] subviews];
  EXPECT_EQ(arraysize(kTestPermissionTypes) * 3 , [subviews count]);

  // Ensure that there is a distinct label for each permission.
  NSMutableSet* labels = [NSMutableSet set];
  for (NSView* view in subviews) {
    if ([view isKindOfClass:[NSTextField class]])
      [labels addObject:[static_cast<NSTextField*>(view) stringValue]];
  }
  EXPECT_EQ(arraysize(kTestPermissionTypes), [labels count]);

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

  // 3 of the buttons should be disabled -- the ones that have a setting source
  // of SETTING_SOURCE_POLICY or SETTING_SOURCE_EXTENSION.
  EXPECT_EQ(3, disabled_count);
}

TEST_F(WebsiteSettingsBubbleControllerTest, WindowWidth) {
  const CGFloat kBigEnoughBubbleWidth = 310;
  // Creating a window that should fit everything.
  CreateBubbleWithWidth(kBigEnoughBubbleWidth);
  SetTestPermissions();

  CGFloat window_width = NSWidth([[controller_ window] frame]);

  // Check the window was made bigger to fit the content.
  EXPECT_EQ(kBigEnoughBubbleWidth, window_width);

  // Check that the window is wider than the right edge of all the permission
  // popup buttons (LTR locales) or wider than the left edge (RTL locales).
  bool is_rtl = base::i18n::IsRTL();
  for (NSView* view in [[controller_ permissionsView] subviews]) {
    if (is_rtl) {
      if ([view isKindOfClass:[NSPopUpButton class]]) {
        NSPopUpButton* button = static_cast<NSPopUpButton*>(view);
        EXPECT_GT(NSMinX([button frame]), 0);
      }
      if ([view isKindOfClass:[NSImageView class]]) {
        NSImageView* icon = static_cast<NSImageView*>(view);
        EXPECT_LT(NSMaxX([icon frame]), window_width);
      }
    } else {
      if ([view isKindOfClass:[NSImageView class]]) {
        NSImageView* icon = static_cast<NSImageView*>(view);
        EXPECT_GT(NSMinX([icon frame]), 0);
      }
      if ([view isKindOfClass:[NSPopUpButton class]]) {
        NSPopUpButton* button = static_cast<NSPopUpButton*>(view);
        EXPECT_LT(NSMaxX([button frame]), window_width);
      }
    }
  }
}

}  // namespace
