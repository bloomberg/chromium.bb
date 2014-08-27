// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/website_settings/permission_bubble_controller.h"

#include "base/mac/foundation_util.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/browser/ui/cocoa/run_loop_testing.h"
#import "chrome/browser/ui/cocoa/website_settings/permission_bubble_cocoa.h"
#import "chrome/browser/ui/cocoa/website_settings/split_block_button.h"
#include "chrome/browser/ui/website_settings/mock_permission_bubble_request.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#import "ui/events/test/cocoa_test_event_utils.h"

@class ConstrainedWindowButton;

@interface PermissionBubbleController (ExposedForTesting)
- (void)ok:(id)sender;
- (void)onAllow:(id)sender;
- (void)onBlock:(id)sender;
- (void)onCustomize:(id)sender;
- (void)onCheckboxChanged:(id)sender;
@end

@interface SplitBlockButton (ExposedForTesting)
- (NSMenu*)menu;
@end

namespace {
const char* const kPermissionA = "Permission A";
const char* const kPermissionB = "Permission B";
const char* const kPermissionC = "Permission C";
}

class PermissionBubbleControllerTest : public CocoaTest,
  public PermissionBubbleView::Delegate {
 public:

  MOCK_METHOD2(ToggleAccept, void(int, bool));
  MOCK_METHOD0(SetCustomizationMode, void());
  MOCK_METHOD0(Accept, void());
  MOCK_METHOD0(Deny, void());
  MOCK_METHOD0(Closing, void());
  MOCK_METHOD1(SetView, void(PermissionBubbleView*));

  virtual void SetUp() OVERRIDE {
    CocoaTest::SetUp();
    bridge_.reset(new PermissionBubbleCocoa(nil));
    AddRequest(kPermissionA);
    controller_ = [[PermissionBubbleController alloc]
        initWithParentWindow:test_window()
                      bridge:bridge_.get()];
  }

  virtual void TearDown() OVERRIDE {
    [controller_ close];
    chrome::testing::NSRunLoopRunAllPending();
    STLDeleteElements(&requests_);
    CocoaTest::TearDown();
  }

  void AddRequest(const std::string& title) {
    MockPermissionBubbleRequest* request = new MockPermissionBubbleRequest(
        title,
        l10n_util::GetStringUTF8(IDS_PERMISSION_ALLOW),
        l10n_util::GetStringUTF8(IDS_PERMISSION_DENY));
    requests_.push_back(request);
  }

  NSButton* FindButtonWithTitle(const std::string& title) {
    return FindButtonWithTitle(base::SysUTF8ToNSString(title),
                               [ConstrainedWindowButton class]);
  }

  NSButton* FindButtonWithTitle(int title_id) {
    return FindButtonWithTitle(l10n_util::GetNSString(title_id),
                               [ConstrainedWindowButton class]);
  }

  NSButton* FindMenuButtonWithTitle(int title_id) {
    return FindButtonWithTitle(l10n_util::GetNSString(title_id),
                               [NSPopUpButton class]);
  }

  // IDS_PERMISSION_ALLOW and IDS_PERMISSION_DENY are used for two distinct
  // UI elements, both of which derive from NSButton.  So check the expected
  // class, not just NSButton, as well as the title.
  NSButton* FindButtonWithTitle(NSString* title, Class button_class) {
    for (NSButton* view in [[controller_ bubble] subviews]) {
      if ([view isKindOfClass:button_class] &&
          [title isEqualToString:[view title]]) {
        return view;
      }
    }
    return nil;
  }

  NSTextField* FindTextFieldWithString(const std::string& text) {
    NSView* parent = base::mac::ObjCCastStrict<NSView>([controller_ bubble]);
    return FindTextFieldWithString(parent, base::SysUTF8ToNSString(text));
  }

  NSTextField* FindTextFieldWithString(NSView* view, NSString* text) {
    NSTextField* textField = nil;
    for (NSView* child in [view subviews]) {
      textField = base::mac::ObjCCast<NSTextField>(child);
      if (![[textField stringValue] hasSuffix:text]) {
        textField = FindTextFieldWithString(child, text);
        if (textField)
          break;
      }
    }
    return textField;
  }

  void ChangePermissionMenuSelection(NSButton* menu_button, int next_title_id) {
    NSMenu* menu = [base::mac::ObjCCastStrict<NSPopUpButton>(menu_button) menu];
    NSString* next_title = l10n_util::GetNSString(next_title_id);
    EXPECT_EQ([[menu itemWithTitle:[menu_button title]] state], NSOnState);
    NSMenuItem* next_item = [menu itemWithTitle:next_title];
    EXPECT_EQ([next_item state], NSOffState);
    [menu performActionForItemAtIndex:[menu indexOfItem:next_item]];
  }

  NSMenuItem* FindCustomizeMenuItem() {
    NSButton* button = FindButtonWithTitle(IDS_PERMISSION_DENY);
    if (!button || ![button isKindOfClass:[SplitBlockButton class]])
      return nil;
    NSString* customize = l10n_util::GetNSString(IDS_PERMISSION_CUSTOMIZE);
    SplitBlockButton* block_button =
        base::mac::ObjCCast<SplitBlockButton>(button);
    for (NSMenuItem* item in [[block_button menu] itemArray]) {
      if ([[item title] isEqualToString:customize])
        return item;
    }
    return nil;
  }

 protected:
  PermissionBubbleController* controller_;  // Weak;  it deletes itself.
  scoped_ptr<PermissionBubbleCocoa> bridge_;
  std::vector<PermissionBubbleRequest*> requests_;
  std::vector<bool> accept_states_;
};

TEST_F(PermissionBubbleControllerTest, ShowAndClose) {
  EXPECT_FALSE([[controller_ window] isVisible]);
  [controller_ showWindow:nil];
  EXPECT_TRUE([[controller_ window] isVisible]);
}

TEST_F(PermissionBubbleControllerTest, ShowSinglePermission) {
  [controller_ showAtAnchor:NSZeroPoint
              withDelegate:this
               forRequests:requests_
              acceptStates:accept_states_
         customizationMode:NO];

  EXPECT_TRUE(FindTextFieldWithString(kPermissionA));
  EXPECT_TRUE(FindButtonWithTitle(IDS_PERMISSION_ALLOW));
  EXPECT_TRUE(FindButtonWithTitle(IDS_PERMISSION_DENY));
  EXPECT_FALSE(FindButtonWithTitle(IDS_OK));
  EXPECT_FALSE(FindCustomizeMenuItem());
}

TEST_F(PermissionBubbleControllerTest, ShowMultiplePermissions) {
  AddRequest(kPermissionB);
  AddRequest(kPermissionC);

  [controller_ showAtAnchor:NSZeroPoint
              withDelegate:this
               forRequests:requests_
              acceptStates:accept_states_
         customizationMode:NO];

  EXPECT_TRUE(FindTextFieldWithString(kPermissionA));
  EXPECT_TRUE(FindTextFieldWithString(kPermissionB));
  EXPECT_TRUE(FindTextFieldWithString(kPermissionC));

  EXPECT_TRUE(FindButtonWithTitle(IDS_PERMISSION_ALLOW));
  EXPECT_TRUE(FindButtonWithTitle(IDS_PERMISSION_DENY));
  EXPECT_TRUE(FindCustomizeMenuItem());
  EXPECT_FALSE(FindButtonWithTitle(IDS_OK));
}

TEST_F(PermissionBubbleControllerTest, ShowCustomizationModeAllow) {
  accept_states_.push_back(true);
  [controller_ showAtAnchor:NSZeroPoint
               withDelegate:this
                forRequests:requests_
               acceptStates:accept_states_
          customizationMode:YES];

  // Test that there is one menu, with 'Allow' visible.
  EXPECT_TRUE(FindMenuButtonWithTitle(IDS_PERMISSION_ALLOW));
  EXPECT_FALSE(FindMenuButtonWithTitle(IDS_PERMISSION_DENY));

  EXPECT_TRUE(FindButtonWithTitle(IDS_OK));
  EXPECT_FALSE(FindButtonWithTitle(IDS_PERMISSION_ALLOW));
  EXPECT_FALSE(FindButtonWithTitle(IDS_PERMISSION_DENY));
  EXPECT_FALSE(FindCustomizeMenuItem());
}

TEST_F(PermissionBubbleControllerTest, ShowCustomizationModeBlock) {
  accept_states_.push_back(false);
  [controller_ showAtAnchor:NSZeroPoint
              withDelegate:this
               forRequests:requests_
              acceptStates:accept_states_
         customizationMode:YES];

  // Test that there is one menu, with 'Block' visible.
  EXPECT_TRUE(FindMenuButtonWithTitle(IDS_PERMISSION_DENY));
  EXPECT_FALSE(FindMenuButtonWithTitle(IDS_PERMISSION_ALLOW));

  EXPECT_TRUE(FindButtonWithTitle(IDS_OK));
  EXPECT_FALSE(FindButtonWithTitle(IDS_PERMISSION_ALLOW));
  EXPECT_FALSE(FindButtonWithTitle(IDS_PERMISSION_DENY));
  EXPECT_FALSE(FindCustomizeMenuItem());
}

TEST_F(PermissionBubbleControllerTest, OK) {
  accept_states_.push_back(true);
  [controller_ showAtAnchor:NSZeroPoint
              withDelegate:this
               forRequests:requests_
              acceptStates:accept_states_
         customizationMode:YES];

  EXPECT_CALL(*this, Closing()).Times(1);
  [FindButtonWithTitle(IDS_OK) performClick:nil];
}

TEST_F(PermissionBubbleControllerTest, Allow) {
  [controller_ showAtAnchor:NSZeroPoint
              withDelegate:this
               forRequests:requests_
              acceptStates:accept_states_
         customizationMode:NO];

  EXPECT_CALL(*this, Accept()).Times(1);
  [FindButtonWithTitle(IDS_PERMISSION_ALLOW) performClick:nil];
}

TEST_F(PermissionBubbleControllerTest, Deny) {
  [controller_ showAtAnchor:NSZeroPoint
              withDelegate:this
               forRequests:requests_
              acceptStates:accept_states_
         customizationMode:NO];

  EXPECT_CALL(*this, Deny()).Times(1);
  [FindButtonWithTitle(IDS_PERMISSION_DENY) performClick:nil];
}

TEST_F(PermissionBubbleControllerTest, ChangePermissionSelection) {
  AddRequest(kPermissionB);

  accept_states_.push_back(true);
  accept_states_.push_back(false);

  [controller_ showAtAnchor:NSZeroPoint
              withDelegate:this
               forRequests:requests_
              acceptStates:accept_states_
         customizationMode:YES];

  EXPECT_CALL(*this, ToggleAccept(0, false)).Times(1);
  EXPECT_CALL(*this, ToggleAccept(1, true)).Times(1);
  NSButton* menu_a = FindMenuButtonWithTitle(IDS_PERMISSION_ALLOW);
  NSButton* menu_b = FindMenuButtonWithTitle(IDS_PERMISSION_DENY);
  ChangePermissionMenuSelection(menu_a, IDS_PERMISSION_DENY);
  ChangePermissionMenuSelection(menu_b, IDS_PERMISSION_ALLOW);
}

TEST_F(PermissionBubbleControllerTest, ClickCustomize) {
  AddRequest(kPermissionB);
  [controller_ showAtAnchor:NSZeroPoint
              withDelegate:this
               forRequests:requests_
              acceptStates:accept_states_
         customizationMode:NO];

  EXPECT_CALL(*this, SetCustomizationMode()).Times(1);
  NSMenuItem* customize_item = FindCustomizeMenuItem();
  EXPECT_TRUE(customize_item);
  NSMenu* menu = [customize_item menu];
  [menu performActionForItemAtIndex:[menu indexOfItem:customize_item]];
}
