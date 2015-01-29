// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/extensions/browser_action_button.h"
#import "chrome/browser/ui/cocoa/extensions/browser_actions_controller.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#import "chrome/browser/ui/cocoa/wrench_menu/wrench_menu_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "extensions/common/feature_switch.h"
#import "ui/events/test/cocoa_test_event_utils.h"

namespace {

// Returns the center point for a particular |view|.
NSPoint GetCenterPoint(NSView* view) {
  NSWindow* window = [view window];
  NSScreen* screen = [window screen];
  DCHECK(screen);

  // Converts the center position of the view into the coordinates accepted
  // by ui_controls methods.
  NSRect bounds = [view bounds];
  NSPoint center = NSMakePoint(NSMidX(bounds), NSMidY(bounds));
  center = [view convertPoint:center toView:nil];
  center = [window convertBaseToScreen:center];
  return NSMakePoint(center.x, [screen frame].size.height - center.y);
}

// Moves the mouse (synchronously) to the center of the given |view|.
void MoveMouseToCenter(NSView* view) {
  NSPoint centerPoint = GetCenterPoint(view);
  base::RunLoop runLoop;
  ui_controls::SendMouseMoveNotifyWhenDone(
      centerPoint.x, centerPoint.y, runLoop.QuitClosure());
  runLoop.Run();
}

}  // namespace

// A simple helper menu delegate that will keep track of if a menu is opened,
// and closes them immediately (which is useful because message loops with
// menus open in Cocoa don't play nicely with testing).
@interface MenuHelper : NSObject<NSMenuDelegate> {
  // Whether or not a menu has been opened. This can be reset so the helper can
  // be used multiple times.
  BOOL menuOpened_;
}

// Bare-bones init.
- (id)init;

@property(nonatomic, assign) BOOL menuOpened;

@end

@implementation MenuHelper

- (void)menuWillOpen:(NSMenu*)menu {
  menuOpened_ = YES;
  [menu cancelTracking];
}

- (id)init {
  self = [super init];
  return self;
}

@synthesize menuOpened = menuOpened_;

@end

class BrowserActionButtonUiTest : public ExtensionBrowserTest {
 protected:
  BrowserActionButtonUiTest() {}
  ~BrowserActionButtonUiTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    enable_redesign_.reset(new extensions::FeatureSwitch::ScopedOverride(
        extensions::FeatureSwitch::extension_action_redesign(), true));
    ToolbarActionsBar::disable_animations_for_testing_ = true;
  }

  void TearDownOnMainThread() override {
    enable_redesign_.reset();
    ToolbarActionsBar::disable_animations_for_testing_ = false;
  }

 private:
  scoped_ptr<extensions::FeatureSwitch::ScopedOverride> enable_redesign_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionButtonUiTest);
};

// Simulates a clicks on the action button in the overflow menu, and runs
// |closure| upon completion.
void ClickOnOverflowedAction(ToolbarController* toolbarController,
                             const base::Closure& closure) {
  WrenchMenuController* wrenchMenuController =
      [toolbarController wrenchMenuController];
  // The wrench menu should start as open (since that's where the overflowed
  // actions are).
  EXPECT_TRUE([wrenchMenuController isMenuOpen]);
  BrowserActionsController* overflowController =
      [wrenchMenuController browserActionsController];

  ASSERT_TRUE(overflowController);
  BrowserActionButton* actionButton =
      [overflowController buttonWithIndex:0];
  // The action should be attached to a superview.
  EXPECT_TRUE([actionButton superview]);

  // ui_controls:: methods don't play nice when there is an open menu (like the
  // wrench menu). Instead, simulate a right click by feeding the event directly
  // to the button.
  NSPoint centerPoint = GetCenterPoint(actionButton);
  NSEvent* mouseEvent = cocoa_test_event_utils::RightMouseDownAtPointInWindow(
      centerPoint, [actionButton window]);
  [actionButton rightMouseDown:mouseEvent];

  // This should close the wrench menu.
  EXPECT_FALSE([wrenchMenuController isMenuOpen]);
  base::MessageLoop::current()->PostTask(FROM_HERE, closure);
}

// Test that opening a context menu works for both actions on the main bar and
// actions in the overflow menu.
IN_PROC_BROWSER_TEST_F(BrowserActionButtonUiTest,
                       ContextMenusOnMainAndOverflow) {
  // Add an extension with a browser action.
  scoped_refptr<const extensions::Extension> extension =
      extensions::extension_action_test_util::CreateActionExtension(
          "browser_action",
          extensions::extension_action_test_util::BROWSER_ACTION);
  extension_service()->AddExtension(extension.get());
  extensions::ExtensionToolbarModel* model =
      extensions::ExtensionToolbarModel::Get(profile());
  ASSERT_EQ(1u, model->toolbar_items().size());

  ToolbarController* toolbarController =
      [[BrowserWindowController
          browserWindowControllerForWindow:browser()->
              window()->GetNativeWindow()]
          toolbarController];
  ASSERT_TRUE(toolbarController);

  BrowserActionsController* actionsController =
      [toolbarController browserActionsController];
  BrowserActionButton* actionButton = [actionsController buttonWithIndex:0];
  ASSERT_TRUE(actionButton);

  // Stub out the action button's normal context menu with a fake one so we
  // can track when it opens.
  base::scoped_nsobject<NSMenu> testContextMenu(
        [[NSMenu alloc] initWithTitle:@""]);
  base::scoped_nsobject<MenuHelper> menuHelper([[MenuHelper alloc] init]);
  [testContextMenu setDelegate:menuHelper.get()];
  [actionButton setTestContextMenu:testContextMenu.get()];

  // Right now, the action button should be visible (attached to a superview).
  EXPECT_TRUE([actionButton superview]);

  // Move the mouse to the center of the action button, preparing to click.
  MoveMouseToCenter(actionButton);

  {
    // No menu should yet be open.
    EXPECT_FALSE([menuHelper menuOpened]);
    base::RunLoop runLoop;
    ui_controls::SendMouseEventsNotifyWhenDone(
        ui_controls::RIGHT,
        ui_controls::DOWN | ui_controls::UP,
        runLoop.QuitClosure());
    runLoop.Run();
    // The menu should have opened from the click.
    EXPECT_TRUE([menuHelper menuOpened]);
  }

  // Reset the menu helper so we can use it again.
  [menuHelper setMenuOpened:NO];

  // Shrink the visible count to be 0. This should hide the action button.
  model->SetVisibleIconCount(0);
  EXPECT_EQ(nil, [actionButton superview]);

  // Move the mouse over the wrench button.
  NSView* wrenchButton = [toolbarController wrenchButton];
  ASSERT_TRUE(wrenchButton);
  MoveMouseToCenter(wrenchButton);

  {
    // No menu yet (on the browser action).
    EXPECT_FALSE([menuHelper menuOpened]);
    base::RunLoop runLoop;
    // Click on the wrench menu, and pass in a callback to continue the test
    // in ClickOnOverflowedAction (Due to the blocking nature of Cocoa menus,
    // passing in runLoop.QuitClosure() is not sufficient here.)
    ui_controls::SendMouseEventsNotifyWhenDone(
        ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        base::Bind(&ClickOnOverflowedAction,
                   base::Unretained(toolbarController),
                   runLoop.QuitClosure()));
    runLoop.Run();
    // The menu should have opened. Note that the menu opened on the main bar's
    // action button, not the overflow's. Since Cocoa doesn't support running
    // a menu-within-a-menu, this is what has to happen.
    EXPECT_TRUE([menuHelper menuOpened]);
  }
}
