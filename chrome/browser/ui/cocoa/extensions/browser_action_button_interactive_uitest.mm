// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/app_menu/app_menu_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/extensions/browser_action_button.h"
#import "chrome/browser/ui/cocoa/extensions/browser_actions_container_view.h"
#import "chrome/browser/ui/cocoa/extensions/browser_actions_controller.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "extensions/common/feature_switch.h"
#import "ui/events/test/cocoa_test_event_utils.h"

namespace {

const int kMenuPadding = 26;

// A simple error class that has a menu item.
class MenuError : public GlobalError {
 public:
  MenuError() {}
  ~MenuError() override {}

  bool HasMenuItem() override { return true; }
  int MenuItemCommandID() override {
    // An arbitrary high id so that it's not taken.
    return 65536;
  }
  base::string16 MenuItemLabel() override {
    const char kErrorMessage[] =
        "This is a really long error message that will cause the app menu "
        "to have increased width";
    return base::ASCIIToUTF16(kErrorMessage);
  }
  void ExecuteMenuItem(Browser* browser) override {}

  bool HasBubbleView() override { return false; }
  bool HasShownBubbleView() override { return false; }
  void ShowBubbleView(Browser* browser) override {}
  GlobalErrorBubbleViewBase* GetBubbleView() override { return nullptr; }

 private:
  DISALLOW_COPY_AND_ASSIGN(MenuError);
};

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

  // A function to be called to verify state while the menu is open.
  base::Closure verify_;
}

// Bare-bones init.
- (id)init;

@property(nonatomic, assign) BOOL menuOpened;
@property(nonatomic, assign) base::Closure verify;

@end

@implementation MenuHelper

- (void)menuWillOpen:(NSMenu*)menu {
  menuOpened_ = YES;
  if (!verify_.is_null())
    verify_.Run();
  [menu cancelTracking];
}

- (id)init {
  self = [super init];
  return self;
}

@synthesize menuOpened = menuOpened_;
@synthesize verify = verify_;

@end

class BrowserActionButtonUiTest : public ExtensionBrowserTest {
 protected:
  BrowserActionButtonUiTest() {}
  ~BrowserActionButtonUiTest() override {}

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    toolbarController_ =
        [[BrowserWindowController
            browserWindowControllerForWindow:browser()->
                window()->GetNativeWindow()]
            toolbarController];
    ASSERT_TRUE(toolbarController_);
    appMenuController_ = [toolbarController_ appMenuController];
    model_ = ToolbarActionsModel::Get(profile());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    enable_redesign_.reset(new extensions::FeatureSwitch::ScopedOverride(
        extensions::FeatureSwitch::extension_action_redesign(), true));
    ToolbarActionsBar::disable_animations_for_testing_ = true;
  }

  void TearDownOnMainThread() override {
    enable_redesign_.reset();
    ToolbarActionsBar::disable_animations_for_testing_ = false;
    ExtensionBrowserTest::TearDownOnMainThread();
  }

  ToolbarController* toolbarController() { return toolbarController_; }
  AppMenuController* appMenuController() { return appMenuController_; }
  ToolbarActionsModel* model() { return model_; }
  NSView* appMenuButton() { return [toolbarController_ appMenuButton]; }

 private:
  scoped_ptr<extensions::FeatureSwitch::ScopedOverride> enable_redesign_;

  ToolbarController* toolbarController_ = nil;
  AppMenuController* appMenuController_ = nil;
  ToolbarActionsModel* model_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionButtonUiTest);
};

// Simulates a clicks on the action button in the overflow menu, and runs
// |closure| upon completion.
void ClickOnOverflowedAction(ToolbarController* toolbarController,
                             const base::Closure& closure) {
  AppMenuController* appMenuController =
      [toolbarController appMenuController];
  // The app menu should start as open (since that's where the overflowed
  // actions are).
  EXPECT_TRUE([appMenuController isMenuOpen]);
  BrowserActionsController* overflowController =
      [appMenuController browserActionsController];

  ASSERT_TRUE(overflowController);
  BrowserActionButton* actionButton =
      [overflowController buttonWithIndex:0];
  // The action should be attached to a superview.
  EXPECT_TRUE([actionButton superview]);

  // ui_controls:: methods don't play nice when there is an open menu (like the
  // app menu). Instead, simulate a right click by feeding the event directly to
  // the button.
  NSPoint centerPoint = GetCenterPoint(actionButton);
  NSEvent* mouseEvent = cocoa_test_event_utils::RightMouseDownAtPointInWindow(
      centerPoint, [actionButton window]);
  [actionButton rightMouseDown:mouseEvent];

  // This should close the app menu.
  EXPECT_FALSE([appMenuController isMenuOpen]);
  base::MessageLoop::current()->PostTask(FROM_HERE, closure);
}

// Verifies that the action is "popped out" of overflow; that is, it is visible
// on the main bar, and is set as the popped out action on the controlling
// ToolbarActionsBar.
void CheckActionIsPoppedOut(BrowserActionsController* actionsController,
                            BrowserActionButton* actionButton) {
  EXPECT_EQ([actionsController containerView], [actionButton superview]);
  EXPECT_EQ([actionButton viewController],
            [actionsController toolbarActionsBar]->popped_out_action());
}

// Test that opening a context menu works for both actions on the main bar and
// actions in the overflow menu.
// Flaky: http://crbug.com/561461
IN_PROC_BROWSER_TEST_F(BrowserActionButtonUiTest,
                       DISABLED_ContextMenusOnMainAndOverflow) {
  // Add an extension with a browser action.
  scoped_refptr<const extensions::Extension> extension =
      extensions::extension_action_test_util::CreateActionExtension(
          "browser_action",
          extensions::extension_action_test_util::BROWSER_ACTION);
  extension_service()->AddExtension(extension.get());
  ASSERT_EQ(1u, model()->toolbar_items().size());

  BrowserActionsController* actionsController =
      [toolbarController() browserActionsController];
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
  [menuHelper setVerify:base::Bind(
      CheckActionIsPoppedOut, actionsController, actionButton)];

  // Shrink the visible count to be 0. This should hide the action button.
  model()->SetVisibleIconCount(0);
  EXPECT_EQ(nil, [actionButton superview]);

  // Move the mouse over the app menu button.
  MoveMouseToCenter(appMenuButton());

  {
    // No menu yet (on the browser action).
    EXPECT_FALSE([menuHelper menuOpened]);
    base::RunLoop runLoop;
    // Click on the app menu, and pass in a callback to continue the test in
    // ClickOnOverflowedAction (Due to the blocking nature of Cocoa menus,
    // passing in runLoop.QuitClosure() is not sufficient here.)
    ui_controls::SendMouseEventsNotifyWhenDone(
        ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        base::Bind(&ClickOnOverflowedAction,
                   base::Unretained(toolbarController()),
                   runLoop.QuitClosure()));
    runLoop.Run();
    // The menu should have opened. Note that the menu opened on the main bar's
    // action button, not the overflow's. Since Cocoa doesn't support running
    // a menu-within-a-menu, this is what has to happen.
    EXPECT_TRUE([menuHelper menuOpened]);
  }
}

// Checks the layout of the overflow bar in the app menu.
void CheckAppMenuLayout(ToolbarController* toolbarController,
                        int overflowStartIndex,
                        const std::string& error_message,
                        const base::Closure& closure) {
  AppMenuController* appMenuController =
      [toolbarController appMenuController];
  // The app menu should start as open (since that's where the overflowed
  // actions are).
  EXPECT_TRUE([appMenuController isMenuOpen]) << error_message;
  BrowserActionsController* overflowController =
      [appMenuController browserActionsController];
  ASSERT_TRUE(overflowController) << error_message;

  ToolbarActionsBar* overflowBar = [overflowController toolbarActionsBar];
  BrowserActionsContainerView* overflowContainer =
      [overflowController containerView];
  NSMenu* menu = [appMenuController menu];

  // The overflow container should be within the bounds of the app menu, as
  // should its parents.
  int menu_width = [menu size].width;
  NSRect frame = [overflowContainer frame];
  // The container itself should be indented in the menu.
  EXPECT_GT(NSMinX(frame), 0) << error_message;
  // Hierarchy: The overflow container is owned by two different views in the
  // app menu. Each superview should start at 0 in the x-axis.
  EXPECT_EQ(0, NSMinX([[overflowContainer superview] frame])) << error_message;
  EXPECT_EQ(0, NSMinX([[[overflowContainer superview] superview] frame])) <<
      error_message;
  // The overflow container should fully fit in the app menu, including the
  // space taken away for padding, and should have its desired size.
  EXPECT_LE(NSWidth(frame), menu_width - kMenuPadding) << error_message;
  EXPECT_EQ(NSWidth(frame), overflowBar->GetPreferredSize().width()) <<
      error_message;

  // Every button that has an index lower than the overflow start index (i.e.,
  // every button on the main toolbar) should not be attached to a view.
  for (int i = 0; i < overflowStartIndex; ++i) {
    BrowserActionButton* button = [overflowController buttonWithIndex:i];
    EXPECT_FALSE([button superview]) << error_message;
    EXPECT_GE(0, NSMaxX([button frame])) << error_message;
  }

  // Every other button should be attached to a view, and should be at the
  // proper bounds. Calculating each button's proper bounds here would just be
  // a duplication of the logic in the method, but we can test that each button
  // a) is within the overflow container's bounds, and
  // b) doesn't intersect with another button.
  // If both of those are true, then we're probably good.
  for (NSUInteger i = overflowStartIndex;
       i < [overflowController buttonCount]; ++i) {
    BrowserActionButton* button = [overflowController buttonWithIndex:i];
    EXPECT_TRUE([button superview]) << error_message;
    EXPECT_TRUE(NSContainsRect([overflowContainer bounds], [button frame])) <<
        error_message;
    for (NSUInteger j = 0; j < i; ++j) {
      EXPECT_FALSE(
          NSContainsRect([[overflowController buttonWithIndex:j] frame],
                         [button frame])) << error_message;
    }
  }

  // Close the app menu.
  [appMenuController cancel];
  EXPECT_FALSE([appMenuController isMenuOpen]) << error_message;
  base::MessageLoop::current()->PostTask(FROM_HERE, closure);
}

// Tests the layout of the overflow container in the app menu.
IN_PROC_BROWSER_TEST_F(BrowserActionButtonUiTest, TestOverflowContainerLayout) {
  // Add a bunch of extensions - enough to trigger multiple rows in the overflow
  // menu.
  const int kNumExtensions = 12;
  for (int i = 0; i < kNumExtensions; ++i) {
    scoped_refptr<const extensions::Extension> extension =
        extensions::extension_action_test_util::CreateActionExtension(
            base::StringPrintf("extension%d", i),
            extensions::extension_action_test_util::BROWSER_ACTION);
    extension_service()->AddExtension(extension.get());
  }
  ASSERT_EQ(kNumExtensions, static_cast<int>(model()->toolbar_items().size()));

  // A helper function to open the app menu and call the check function.
  auto resizeAndActivateAppMenu = [this](int visible_count,
                                         const std::string& error_message) {
    model()->SetVisibleIconCount(kNumExtensions - visible_count);
    MoveMouseToCenter(appMenuButton());

    {
      base::RunLoop runLoop;
      // Click on the app menu, and pass in a callback to continue the test in
      // CheckAppMenuLayout (due to the blocking nature of Cocoa menus,
      // passing in runLoop.QuitClosure() is not sufficient here.)
      ui_controls::SendMouseEventsNotifyWhenDone(
          ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
          base::Bind(&CheckAppMenuLayout,
                     base::Unretained(toolbarController()),
                     kNumExtensions - visible_count,
                     error_message,
                     runLoop.QuitClosure()));
      runLoop.Run();
    }
  };

  // Test the layout with gradually more extensions hidden.
  for (int i = 1; i <= kNumExtensions; ++i)
    resizeAndActivateAppMenu(i, base::StringPrintf("Normal: %d", i));

  // Adding a global error adjusts the app menu size, and has been known to mess
  // up the overflow container's bounds (crbug.com/511326).
  GlobalErrorService* error_service =
      GlobalErrorServiceFactory::GetForProfile(profile());
  error_service->AddGlobalError(new MenuError());

  // It's probably excessive to test every level of the overflow here. Test
  // having all actions overflowed, some actions overflowed, and one action
  // overflowed.
  resizeAndActivateAppMenu(kNumExtensions, "GlobalError Full");
  resizeAndActivateAppMenu(kNumExtensions / 2, "GlobalError Half");
  resizeAndActivateAppMenu(1, "GlobalError One");
}

void AddExtensionWithMenuOpen(ToolbarController* toolbarController,
                              ExtensionService* extensionService,
                              const base::Closure& closure) {
  AppMenuController* appMenuController =
      [toolbarController appMenuController];

  scoped_refptr<const extensions::Extension> extension =
      extensions::extension_action_test_util::CreateActionExtension(
          "extension",
          extensions::extension_action_test_util::BROWSER_ACTION);
  extensionService->AddExtension(extension.get());

  base::RunLoop().RunUntilIdle();

  // Close the app menu.
  [appMenuController cancel];
  EXPECT_FALSE([appMenuController isMenuOpen]);
  base::MessageLoop::current()->PostTask(FROM_HERE, closure);
}

// Test adding an extension while the app menu is open. Regression test for
// crbug.com/561237.
// Flaky: http://crbug.com/564623
IN_PROC_BROWSER_TEST_F(BrowserActionButtonUiTest,
                       DISABLED_AddExtensionWithMenuOpen) {
  // Add an extension to ensure the overflow menu is present.
  scoped_refptr<const extensions::Extension> extension =
      extensions::extension_action_test_util::CreateActionExtension(
          "original extension",
          extensions::extension_action_test_util::BROWSER_ACTION);
  extension_service()->AddExtension(extension.get());
  ASSERT_EQ(1, static_cast<int>(model()->toolbar_items().size()));
  model()->SetVisibleIconCount(0);

  MoveMouseToCenter(appMenuButton());

  base::RunLoop runLoop;
  // Click on the app menu, and pass in a callback to continue the test in
  // AddExtensionWithMenuOpen (due to the blocking nature of Cocoa menus,
  // passing in runLoop.QuitClosure() is not sufficient here.)
  ui_controls::SendMouseEventsNotifyWhenDone(
      ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
      base::Bind(&AddExtensionWithMenuOpen,
                 base::Unretained(toolbarController()),
                 extension_service(),
                 runLoop.QuitClosure()));
  runLoop.Run();
}
