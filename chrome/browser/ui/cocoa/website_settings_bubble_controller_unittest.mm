// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/website_settings_bubble_controller.h"

namespace {

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

    // Expect 3 views: the identity, identity status, and the tab view.
    EXPECT_EQ(3U, [subviews count]);

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

 private:
  WebsiteSettingsBubbleController* controller_;  // Weak, owns self.
  NSWindow* window_;  // Weak, owned by controller.
};

TEST_F(WebsiteSettingsBubbleControllerTest, IdentityInfo) {
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
  EXPECT_FALSE([status isEqual: [identity_status_field stringValue]]);
}

}  // namespace
