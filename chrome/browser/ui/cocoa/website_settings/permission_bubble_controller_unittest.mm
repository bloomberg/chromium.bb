// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/website_settings/permission_bubble_controller.h"

#include <Carbon/Carbon.h>

#include "base/mac/foundation_util.h"
#import "base/mac/scoped_objc_class_swizzler.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/ui/cocoa/run_loop_testing.h"
#import "chrome/browser/ui/cocoa/website_settings/permission_bubble_cocoa.h"
#import "chrome/browser/ui/cocoa/website_settings/split_block_button.h"
#include "chrome/browser/ui/website_settings/mock_permission_bubble_request.h"
#include "chrome/grit/generated_resources.h"
#include "grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#include "ui/base/cocoa/cocoa_base_utils.h"
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

@interface MockBubbleYesLocationBar : NSObject
@end

@implementation MockBubbleYesLocationBar
- (bool)hasVisibleLocationBar { return true; }
@end

@interface MockBubbleNoLocationBar : NSObject
@end

@implementation MockBubbleNoLocationBar
- (bool)hasVisibleLocationBar { return false; }
@end

namespace {
const char* const kPermissionA = "Permission A";
const char* const kPermissionB = "Permission B";
const char* const kPermissionC = "Permission C";
}

class PermissionBubbleControllerTest : public CocoaProfileTest,
                                       public PermissionBubbleView::Delegate {
 public:

  MOCK_METHOD2(ToggleAccept, void(int, bool));
  MOCK_METHOD0(SetCustomizationMode, void());
  MOCK_METHOD0(Accept, void());
  MOCK_METHOD0(Deny, void());
  MOCK_METHOD0(Closing, void());
  MOCK_METHOD1(SetView, void(PermissionBubbleView*));

  void SetUp() override {
    CocoaProfileTest::SetUp();
    bridge_.reset(new PermissionBubbleCocoa(browser()));
    AddRequest(kPermissionA);
    controller_ =
        [[PermissionBubbleController alloc] initWithBrowser:browser()
                                                     bridge:bridge_.get()];
  }

  void TearDown() override {
    [controller_ close];
    chrome::testing::NSRunLoopRunAllPending();
    STLDeleteElements(&requests_);
    CocoaProfileTest::TearDown();
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

 protected:
  PermissionBubbleController* controller_;  // Weak;  it deletes itself.
  std::unique_ptr<PermissionBubbleCocoa> bridge_;
  std::vector<PermissionBubbleRequest*> requests_;
  std::vector<bool> accept_states_;
};

TEST_F(PermissionBubbleControllerTest, ShowAndClose) {
  EXPECT_FALSE([[controller_ window] isVisible]);
  [controller_ showWindow:nil];
  EXPECT_TRUE([[controller_ window] isVisible]);
}

TEST_F(PermissionBubbleControllerTest, ShowSinglePermission) {
  [controller_ showWithDelegate:this
                    forRequests:requests_
                   acceptStates:accept_states_];

  EXPECT_TRUE(FindTextFieldWithString(kPermissionA));
  EXPECT_TRUE(FindButtonWithTitle(IDS_PERMISSION_ALLOW));
  EXPECT_TRUE(FindButtonWithTitle(IDS_PERMISSION_DENY));
  EXPECT_FALSE(FindButtonWithTitle(IDS_OK));
}

TEST_F(PermissionBubbleControllerTest, ShowMultiplePermissions) {
  AddRequest(kPermissionB);
  AddRequest(kPermissionC);

  accept_states_.push_back(true);  // A
  accept_states_.push_back(true);  // B
  accept_states_.push_back(true);  // C

  [controller_ showWithDelegate:this
                    forRequests:requests_
                   acceptStates:accept_states_];

  EXPECT_TRUE(FindTextFieldWithString(kPermissionA));
  EXPECT_TRUE(FindTextFieldWithString(kPermissionB));
  EXPECT_TRUE(FindTextFieldWithString(kPermissionC));

  EXPECT_FALSE(FindButtonWithTitle(IDS_PERMISSION_ALLOW));
  EXPECT_FALSE(FindButtonWithTitle(IDS_PERMISSION_DENY));
  EXPECT_TRUE(FindButtonWithTitle(IDS_OK));
}

TEST_F(PermissionBubbleControllerTest, ShowMultiplePermissionsAllow) {
  AddRequest(kPermissionB);

  accept_states_.push_back(true);  // A
  accept_states_.push_back(true);  // B

  [controller_ showWithDelegate:this
                    forRequests:requests_
                   acceptStates:accept_states_];

  // Test that all menus have 'Allow' visible.
  EXPECT_TRUE(FindMenuButtonWithTitle(IDS_PERMISSION_ALLOW));
  EXPECT_FALSE(FindMenuButtonWithTitle(IDS_PERMISSION_DENY));

  EXPECT_TRUE(FindButtonWithTitle(IDS_OK));
  EXPECT_FALSE(FindButtonWithTitle(IDS_PERMISSION_ALLOW));
  EXPECT_FALSE(FindButtonWithTitle(IDS_PERMISSION_DENY));
}

TEST_F(PermissionBubbleControllerTest, ShowMultiplePermissionsBlock) {
  AddRequest(kPermissionB);

  accept_states_.push_back(false);  // A
  accept_states_.push_back(false);  // B

  [controller_ showWithDelegate:this
                    forRequests:requests_
                   acceptStates:accept_states_];

  // Test that all menus have 'Block' visible.
  EXPECT_TRUE(FindMenuButtonWithTitle(IDS_PERMISSION_DENY));
  EXPECT_FALSE(FindMenuButtonWithTitle(IDS_PERMISSION_ALLOW));

  EXPECT_TRUE(FindButtonWithTitle(IDS_OK));
  EXPECT_FALSE(FindButtonWithTitle(IDS_PERMISSION_ALLOW));
  EXPECT_FALSE(FindButtonWithTitle(IDS_PERMISSION_DENY));
}

TEST_F(PermissionBubbleControllerTest, ShowMultiplePermissionsMixed) {
  AddRequest(kPermissionB);
  AddRequest(kPermissionC);

  accept_states_.push_back(false);  // A
  accept_states_.push_back(false);  // B
  accept_states_.push_back(true);   // C

  [controller_ showWithDelegate:this
                    forRequests:requests_
                   acceptStates:accept_states_];

  // Test that both 'allow' and 'deny' are visible.
  EXPECT_TRUE(FindMenuButtonWithTitle(IDS_PERMISSION_DENY));
  EXPECT_TRUE(FindMenuButtonWithTitle(IDS_PERMISSION_ALLOW));

  EXPECT_TRUE(FindButtonWithTitle(IDS_OK));
  EXPECT_FALSE(FindButtonWithTitle(IDS_PERMISSION_ALLOW));
  EXPECT_FALSE(FindButtonWithTitle(IDS_PERMISSION_DENY));
}

TEST_F(PermissionBubbleControllerTest, OK) {
  AddRequest(kPermissionB);

  accept_states_.push_back(true);  // A
  accept_states_.push_back(true);  // B

  [controller_ showWithDelegate:this
                    forRequests:requests_
                   acceptStates:accept_states_];

  EXPECT_CALL(*this, Accept()).Times(1);
  [FindButtonWithTitle(IDS_OK) performClick:nil];
}

TEST_F(PermissionBubbleControllerTest, Allow) {
  [controller_ showWithDelegate:this
                    forRequests:requests_
                   acceptStates:accept_states_];

  EXPECT_CALL(*this, Accept()).Times(1);
  [FindButtonWithTitle(IDS_PERMISSION_ALLOW) performClick:nil];
}

TEST_F(PermissionBubbleControllerTest, Deny) {
  [controller_ showWithDelegate:this
                    forRequests:requests_
                   acceptStates:accept_states_];

  EXPECT_CALL(*this, Deny()).Times(1);
  [FindButtonWithTitle(IDS_PERMISSION_DENY) performClick:nil];
}

TEST_F(PermissionBubbleControllerTest, ChangePermissionSelection) {
  AddRequest(kPermissionB);

  accept_states_.push_back(true);   // A
  accept_states_.push_back(false);  // B

  [controller_ showWithDelegate:this
                    forRequests:requests_
                   acceptStates:accept_states_];

  EXPECT_CALL(*this, ToggleAccept(0, false)).Times(1);
  EXPECT_CALL(*this, ToggleAccept(1, true)).Times(1);
  NSButton* menu_a = FindMenuButtonWithTitle(IDS_PERMISSION_ALLOW);
  NSButton* menu_b = FindMenuButtonWithTitle(IDS_PERMISSION_DENY);
  ChangePermissionMenuSelection(menu_a, IDS_PERMISSION_DENY);
  ChangePermissionMenuSelection(menu_b, IDS_PERMISSION_ALLOW);
}

TEST_F(PermissionBubbleControllerTest, EscapeCloses) {
  [controller_ showWithDelegate:this
                    forRequests:requests_
                   acceptStates:accept_states_];

  EXPECT_TRUE([[controller_ window] isVisible]);
  [[controller_ window]
      performKeyEquivalent:cocoa_test_event_utils::KeyEventWithKeyCode(
                               kVK_Escape, '\e', NSKeyDown, 0)];
  EXPECT_FALSE([[controller_ window] isVisible]);
}

TEST_F(PermissionBubbleControllerTest, EnterFullscreen) {
  [controller_ showWithDelegate:this
                    forRequests:requests_
                   acceptStates:accept_states_];

  EXPECT_TRUE([[controller_ window] isVisible]);

  // Post the "enter fullscreen" notification.
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center postNotificationName:NSWindowWillEnterFullScreenNotification
                        object:test_window()];

  EXPECT_TRUE([[controller_ window] isVisible]);
}

TEST_F(PermissionBubbleControllerTest, ExitFullscreen) {
  [controller_ showWithDelegate:this
                    forRequests:requests_
                   acceptStates:accept_states_];

  EXPECT_TRUE([[controller_ window] isVisible]);

  // Post the "enter fullscreen" notification.
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center postNotificationName:NSWindowWillExitFullScreenNotification
                        object:test_window()];

  EXPECT_TRUE([[controller_ window] isVisible]);
}

TEST_F(PermissionBubbleControllerTest, AnchorPositionWithLocationBar) {
  base::mac::ScopedObjCClassSwizzler locationSwizzle(
      [PermissionBubbleController class], [MockBubbleYesLocationBar class],
      @selector(hasVisibleLocationBar));

  NSPoint anchor = [controller_ getExpectedAnchorPoint];

  // Expected anchor location will be the same as the page info bubble.
  NSWindow* window = browser()->window()->GetNativeWindow();
  BrowserWindowController* controller =
      [BrowserWindowController browserWindowControllerForWindow:window];
  LocationBarViewMac* location_bar_bridge = [controller locationBarBridge];
  NSPoint expected = location_bar_bridge->GetPageInfoBubblePoint();
  expected = ui::ConvertPointFromWindowToScreen(window, expected);
  EXPECT_NSEQ(expected, anchor);
}

TEST_F(PermissionBubbleControllerTest, AnchorPositionWithoutLocationBar) {
  base::mac::ScopedObjCClassSwizzler locationSwizzle(
      [PermissionBubbleController class], [MockBubbleNoLocationBar class],
      @selector(hasVisibleLocationBar));

  NSPoint anchor = [controller_ getExpectedAnchorPoint];

  // Expected anchor location will be top center when there's no location bar.
  NSWindow* window = browser()->window()->GetNativeWindow();
  NSRect frame = [window frame];
  NSPoint expected = NSMakePoint(frame.size.width / 2, frame.size.height);
  expected = ui::ConvertPointFromWindowToScreen(window, expected);
  EXPECT_NSEQ(expected, anchor);
}

TEST_F(PermissionBubbleControllerTest,
    AnchorPositionDifferentWithAndWithoutLocationBar) {
  NSPoint withLocationBar;
  {
    base::mac::ScopedObjCClassSwizzler locationSwizzle(
        [PermissionBubbleController class], [MockBubbleYesLocationBar class],
        @selector(hasVisibleLocationBar));
    withLocationBar = [controller_ getExpectedAnchorPoint];
  }

  NSPoint withoutLocationBar;
  {
    base::mac::ScopedObjCClassSwizzler locationSwizzle(
        [PermissionBubbleController class], [MockBubbleNoLocationBar class],
        @selector(hasVisibleLocationBar));
    withoutLocationBar = [controller_ getExpectedAnchorPoint];
  }

  // The bubble should be in different places depending if the location bar is
  // available or not.
  EXPECT_NSNE(withLocationBar, withoutLocationBar);
}
