// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/permission_bubble/permission_bubble_controller.h"

#include <Carbon/Carbon.h>

#include "base/mac/foundation_util.h"
#import "base/mac/scoped_objc_class_swizzler.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/permissions/mock_permission_request.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/bubble_anchor_util.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/location_bar/location_icon_decoration.h"
#import "chrome/browser/ui/cocoa/page_info/split_block_button.h"
#import "chrome/browser/ui/cocoa/permission_bubble/permission_bubble_cocoa.h"
#import "chrome/browser/ui/cocoa/test/cocoa_profile_test.h"
#include "chrome/browser/ui/cocoa/test/run_loop_testing.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
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
+ (bool)hasVisibleLocationBarForBrowser:(Browser*)browser { return true; }
@end

@interface MockBubbleNoLocationBar : NSObject
@end

@implementation MockBubbleNoLocationBar
+ (bool)hasVisibleLocationBarForBrowser:(Browser*)browser { return false; }
@end

namespace {
const char* const kPermissionA = "Permission A";
const char* const kPermissionB = "Permission B";
const char* const kPermissionC = "Permission C";
}

class PermissionBubbleControllerTest : public CocoaProfileTest,
                                       public PermissionPrompt::Delegate {
 public:

  MOCK_METHOD1(TogglePersist, void(bool));
  MOCK_METHOD0(SetCustomizationMode, void());
  MOCK_METHOD0(Accept, void());
  MOCK_METHOD0(Deny, void());
  MOCK_METHOD0(Closing, void());
  MOCK_METHOD1(SetView, void(PermissionPrompt*));

  void SetUp() override {
    CocoaProfileTest::SetUp();
    AddRequest(kPermissionA);
    bridge_.reset(new PermissionBubbleCocoa(browser(), this));
    controller_ =
        [[PermissionBubbleController alloc] initWithBrowser:browser()
                                                     bridge:bridge_.get()];
  }

  void TearDown() override {
    [controller_ close];
    chrome::testing::NSRunLoopRunAllPending();
    owned_requests_.clear();
    CocoaProfileTest::TearDown();
  }

  const std::vector<PermissionRequest*>& Requests() override {
    return requests_;
  }

  void AddRequest(const std::string& title) {
    std::unique_ptr<MockPermissionRequest> request =
        base::MakeUnique<MockPermissionRequest>(
            title, l10n_util::GetStringUTF8(IDS_PERMISSION_ALLOW),
            l10n_util::GetStringUTF8(IDS_PERMISSION_DENY));
    requests_.push_back(request.get());
    owned_requests_.push_back(std::move(request));
  }

  NSButton* FindButtonWithTitle(const std::string& title) {
    return FindButtonWithTitle(base::SysUTF8ToNSString(title),
                               [ConstrainedWindowButton class]);
  }

  NSButton* FindButtonWithTitle(int title_id) {
    return FindButtonWithTitle(l10n_util::GetNSString(title_id),
                               [ConstrainedWindowButton class]);
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

 protected:
  PermissionBubbleController* controller_;  // Weak;  it deletes itself.
  std::unique_ptr<PermissionBubbleCocoa> bridge_;
  std::vector<PermissionRequest*> requests_;
  std::vector<std::unique_ptr<PermissionRequest>> owned_requests_;
};

TEST_F(PermissionBubbleControllerTest, ShowAndClose) {
  EXPECT_FALSE([[controller_ window] isVisible]);
  [controller_ showWindow:nil];
  EXPECT_TRUE([[controller_ window] isVisible]);
}

// Tests the page icon decoration's active state.
TEST_F(PermissionBubbleControllerTest, PageIconDecorationActiveState) {
  base::mac::ScopedObjCClassSwizzler locationSwizzle(
      [PermissionBubbleController class], [MockBubbleYesLocationBar class],
      @selector(hasVisibleLocationBarForBrowser:));

  NSWindow* window = browser()->window()->GetNativeWindow();
  BrowserWindowController* controller =
      [BrowserWindowController browserWindowControllerForWindow:window];
  LocationBarDecoration* decoration =
      [controller locationBarBridge]->GetPageInfoDecoration();

  [controller_ showWindow:nil];
  EXPECT_TRUE([[controller_ window] isVisible]);
  EXPECT_TRUE(decoration->active());

  [controller_ close];
  EXPECT_FALSE(decoration->active());
}

TEST_F(PermissionBubbleControllerTest, ShowSinglePermission) {
  [controller_ showWithDelegate:this];

  EXPECT_TRUE(FindTextFieldWithString(kPermissionA));
  EXPECT_TRUE(FindButtonWithTitle(IDS_PERMISSION_ALLOW));
  EXPECT_TRUE(FindButtonWithTitle(IDS_PERMISSION_DENY));
}

TEST_F(PermissionBubbleControllerTest, ShowMultiplePermissions) {
  AddRequest(kPermissionB);
  AddRequest(kPermissionC);

  [controller_ showWithDelegate:this];

  EXPECT_TRUE(FindTextFieldWithString(kPermissionA));
  EXPECT_TRUE(FindTextFieldWithString(kPermissionB));
  EXPECT_TRUE(FindTextFieldWithString(kPermissionC));

  EXPECT_TRUE(FindButtonWithTitle(IDS_PERMISSION_ALLOW));
  EXPECT_TRUE(FindButtonWithTitle(IDS_PERMISSION_DENY));
}

TEST_F(PermissionBubbleControllerTest, Allow) {
  [controller_ showWithDelegate:this];

  EXPECT_CALL(*this, Accept()).Times(1);
  [FindButtonWithTitle(IDS_PERMISSION_ALLOW) performClick:nil];
}

TEST_F(PermissionBubbleControllerTest, AllowMultiple) {
  AddRequest(kPermissionB);

  [controller_ showWithDelegate:this];

  EXPECT_CALL(*this, Accept()).Times(1);
  [FindButtonWithTitle(IDS_PERMISSION_ALLOW) performClick:nil];
}

TEST_F(PermissionBubbleControllerTest, Deny) {
  [controller_ showWithDelegate:this];

  EXPECT_CALL(*this, Deny()).Times(1);
  [FindButtonWithTitle(IDS_PERMISSION_DENY) performClick:nil];
}

TEST_F(PermissionBubbleControllerTest, DenyMultiple) {
  AddRequest(kPermissionB);

  [controller_ showWithDelegate:this];

  EXPECT_CALL(*this, Deny()).Times(1);
  [FindButtonWithTitle(IDS_PERMISSION_DENY) performClick:nil];
}

TEST_F(PermissionBubbleControllerTest, EscapeCloses) {
  [controller_ showWithDelegate:this];

  EXPECT_TRUE([[controller_ window] isVisible]);
  [[controller_ window]
      performKeyEquivalent:cocoa_test_event_utils::KeyEventWithKeyCode(
                               kVK_Escape, '\e', NSKeyDown, 0)];
  EXPECT_FALSE([[controller_ window] isVisible]);
}

TEST_F(PermissionBubbleControllerTest, EnterFullscreen) {
  [controller_ showWithDelegate:this];

  EXPECT_TRUE([[controller_ window] isVisible]);

  // Post the "enter fullscreen" notification.
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center postNotificationName:NSWindowWillEnterFullScreenNotification
                        object:test_window()];

  EXPECT_TRUE([[controller_ window] isVisible]);
}

TEST_F(PermissionBubbleControllerTest, ExitFullscreen) {
  [controller_ showWithDelegate:this];

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
      @selector(hasVisibleLocationBarForBrowser:));

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
      @selector(hasVisibleLocationBarForBrowser:));

  NSPoint anchor = [controller_ getExpectedAnchorPoint];

  // Expected anchor location will be top left when there's no location bar.
  NSWindow* window = browser()->window()->GetNativeWindow();
  NSRect frame = [[window contentView] frame];
  NSPoint expected = NSMakePoint(
      NSMinX(frame) + bubble_anchor_util::kNoToolbarLeftOffset, NSMaxY(frame));
  expected = ui::ConvertPointFromWindowToScreen(window, expected);
  EXPECT_NSEQ(expected, anchor);
}

TEST_F(PermissionBubbleControllerTest,
    AnchorPositionDifferentWithAndWithoutLocationBar) {
  NSPoint withLocationBar;
  {
    base::mac::ScopedObjCClassSwizzler locationSwizzle(
        [PermissionBubbleController class], [MockBubbleYesLocationBar class],
        @selector(hasVisibleLocationBarForBrowser:));
    withLocationBar = [controller_ getExpectedAnchorPoint];
  }

  NSPoint withoutLocationBar;
  {
    base::mac::ScopedObjCClassSwizzler locationSwizzle(
        [PermissionBubbleController class], [MockBubbleNoLocationBar class],
        @selector(hasVisibleLocationBarForBrowser:));
    withoutLocationBar = [controller_ getExpectedAnchorPoint];
  }

  // The bubble should be in different places depending if the location bar is
  // available or not.
  EXPECT_NSNE(withLocationBar, withoutLocationBar);
}
