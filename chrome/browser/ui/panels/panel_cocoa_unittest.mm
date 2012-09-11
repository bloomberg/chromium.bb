// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/memory/scoped_ptr.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"  // IDC_*
#import "chrome/browser/ui/cocoa/browser_window_utils.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#import "chrome/browser/ui/cocoa/run_loop_testing.h"
#include "chrome/browser/ui/panels/panel.h"
#import "chrome/browser/ui/panels/panel_cocoa.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#import "chrome/browser/ui/panels/panel_titlebar_view_cocoa.h"
#import "chrome/browser/ui/panels/panel_window_controller_cocoa.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

class PanelAnimatedBoundsObserver :
    public content::WindowedNotificationObserver {
 public:
  PanelAnimatedBoundsObserver(Panel* panel)
    : content::WindowedNotificationObserver(
        chrome::NOTIFICATION_PANEL_BOUNDS_ANIMATIONS_FINISHED,
        content::Source<Panel>(panel)) { }
  virtual ~PanelAnimatedBoundsObserver() { }
};

// Main test class.
class PanelCocoaTest : public CocoaProfileTest {
 public:
  virtual void SetUp() {
    CocoaProfileTest::SetUp();
  }

  Panel* CreateTestPanel(const std::string& panel_name) {
    // Opening panels on a Mac causes NSWindowController of the Panel window
    // to be autoreleased. We need a pool drained after it's done so the test
    // can close correctly.
    base::mac::ScopedNSAutoreleasePool autorelease_pool;

    PanelManager* manager = PanelManager::GetInstance();
    int panels_count = manager->num_panels();

    Panel* panel = manager->CreatePanel(panel_name, profile(),
                                        GURL(), gfx::Rect(),
                                        PanelManager::CREATE_AS_DOCKED);
    EXPECT_EQ(panels_count + 1, manager->num_panels());

    EXPECT_TRUE(panel);
    EXPECT_TRUE(panel->native_panel());  // Native panel is created right away.
    PanelCocoa* native_window =
        static_cast<PanelCocoa*>(panel->native_panel());
    EXPECT_EQ(panel, native_window->panel_);  // Back pointer initialized.

    PanelAnimatedBoundsObserver bounds_observer(panel);

    // Window should not load before Show().
    // Note: Loading the wnidow causes Cocoa to autorelease a few objects.
    // This is the reason we do this within the scope of the
    // ScopedNSAutoreleasePool.
    EXPECT_FALSE([native_window->controller_ isWindowLoaded]);
    panel->Show();
    EXPECT_TRUE([native_window->controller_ isWindowLoaded]);
    EXPECT_TRUE([native_window->controller_ window]);

    // Wait until bounds animate to their specified values.
    bounds_observer.Wait();

    return panel;
  }

  void VerifyTitlebarLocation(NSView* contentView, NSView* titlebar) {
    NSRect content_frame = [contentView frame];
    NSRect titlebar_frame = [titlebar frame];
    // Since contentView and titlebar are both children of window's root view,
    // we can compare their frames since they are in the same coordinate system.
    EXPECT_EQ(NSMinX(content_frame), NSMinX(titlebar_frame));
    EXPECT_EQ(NSWidth(content_frame), NSWidth(titlebar_frame));
    EXPECT_EQ(NSHeight([[titlebar superview] bounds]), NSMaxY(titlebar_frame));
  }

  void ClosePanelAndWait(Panel* panel) {
    EXPECT_TRUE(panel);
    // Closing a panel may involve several async tasks. Need to use
    // message pump and wait for the notification.
    PanelManager* manager = PanelManager::GetInstance();
    int panel_count = manager->num_panels();
    content::WindowedNotificationObserver signal(
        chrome::NOTIFICATION_PANEL_CLOSED,
        content::Source<Panel>(panel));
    panel->Close();
    signal.Wait();
    // Now we have one less panel.
    EXPECT_EQ(panel_count - 1, manager->num_panels());
  }

  NSMenuItem* CreateMenuItem(NSMenu* menu, int command_id) {
    NSMenuItem* item =
      [menu addItemWithTitle:@""
                      action:@selector(commandDispatch:)
               keyEquivalent:@""];
    [item setTag:command_id];
    return item;
  }
};

TEST_F(PanelCocoaTest, CreateClose) {
  PanelManager* manager = PanelManager::GetInstance();
  EXPECT_EQ(0, manager->num_panels());  // No panels initially.

  Panel* panel = CreateTestPanel("Test Panel");
  ASSERT_TRUE(panel);

  gfx::Rect bounds = panel->GetBounds();
  EXPECT_TRUE(bounds.width() > 0);
  EXPECT_TRUE(bounds.height() > 0);

  PanelCocoa* native_window = static_cast<PanelCocoa*>(panel->native_panel());
  ASSERT_TRUE(native_window);
  // NSWindows created by NSWindowControllers don't have this bit even if
  // their NIB has it. The controller's lifetime is the window's lifetime.
  EXPECT_EQ(NO, [[native_window->controller_ window] isReleasedWhenClosed]);

  ClosePanelAndWait(panel);
  EXPECT_EQ(0, manager->num_panels());
}

TEST_F(PanelCocoaTest, AssignedBounds) {
  Panel* panel1 = CreateTestPanel("Test Panel 1");
  Panel* panel2 = CreateTestPanel("Test Panel 2");
  Panel* panel3 = CreateTestPanel("Test Panel 3");

  gfx::Rect bounds1 = panel1->GetBounds();
  gfx::Rect bounds2 = panel2->GetBounds();
  gfx::Rect bounds3 = panel3->GetBounds();

  // This checks panelManager calculating and assigning bounds right.
  // Panels should stack on the bottom right to left.
  EXPECT_LT(bounds3.x() + bounds3.width(), bounds2.x());
  EXPECT_LT(bounds2.x() + bounds2.width(), bounds1.x());
  EXPECT_EQ(bounds1.y(), bounds2.y());
  EXPECT_EQ(bounds2.y(), bounds3.y());

  // After panel2 is closed, panel3 should take its place.
  ClosePanelAndWait(panel2);
  bounds3 = panel3->GetBounds();
  EXPECT_EQ(bounds2, bounds3);

  // After panel1 is closed, panel3 should take its place.
  ClosePanelAndWait(panel1);
  EXPECT_EQ(bounds1, panel3->GetBounds());

  ClosePanelAndWait(panel3);
}

// Same test as AssignedBounds, but checks actual bounds on native OS windows.
TEST_F(PanelCocoaTest, NativeBounds) {
  Panel* panel1 = CreateTestPanel("Test Panel 1");
  Panel* panel2 = CreateTestPanel("Test Panel 2");
  Panel* panel3 = CreateTestPanel("Test Panel 3");

  PanelCocoa* native_window1 = static_cast<PanelCocoa*>(panel1->native_panel());
  PanelCocoa* native_window2 = static_cast<PanelCocoa*>(panel2->native_panel());
  PanelCocoa* native_window3 = static_cast<PanelCocoa*>(panel3->native_panel());

  NSRect bounds1 = [[native_window1->controller_ window] frame];
  NSRect bounds2 = [[native_window2->controller_ window] frame];
  NSRect bounds3 = [[native_window3->controller_ window] frame];

  EXPECT_LT(bounds3.origin.x + bounds3.size.width, bounds2.origin.x);
  EXPECT_LT(bounds2.origin.x + bounds2.size.width, bounds1.origin.x);
  EXPECT_EQ(bounds1.origin.y, bounds2.origin.y);
  EXPECT_EQ(bounds2.origin.y, bounds3.origin.y);

  {
    // After panel2 is closed, panel3 should take its place.
    PanelAnimatedBoundsObserver bounds_observer(panel3);
    ClosePanelAndWait(panel2);
    bounds_observer.Wait();
    bounds3 = [[native_window3->controller_ window] frame];
    EXPECT_EQ(bounds2.origin.x, bounds3.origin.x);
    EXPECT_EQ(bounds2.origin.y, bounds3.origin.y);
    EXPECT_EQ(bounds2.size.width, bounds3.size.width);
    EXPECT_EQ(bounds2.size.height, bounds3.size.height);
  }

  {
    // After panel1 is closed, panel3 should take its place.
    PanelAnimatedBoundsObserver bounds_observer(panel3);
    ClosePanelAndWait(panel1);
    bounds_observer.Wait();
    bounds3 = [[native_window3->controller_ window] frame];
    EXPECT_EQ(bounds1.origin.x, bounds3.origin.x);
    EXPECT_EQ(bounds1.origin.y, bounds3.origin.y);
    EXPECT_EQ(bounds1.size.width, bounds3.size.width);
    EXPECT_EQ(bounds1.size.height, bounds3.size.height);
  }

  ClosePanelAndWait(panel3);
}

// Verify the titlebar is being created.
TEST_F(PanelCocoaTest, TitlebarViewCreate) {
  Panel* panel = CreateTestPanel("Test Panel");

  PanelCocoa* native_window = static_cast<PanelCocoa*>(panel->native_panel());

  PanelTitlebarViewCocoa* titlebar = [native_window->controller_ titlebarView];
  EXPECT_TRUE(titlebar);
  EXPECT_EQ(native_window->controller_, [titlebar controller]);

  ClosePanelAndWait(panel);
}

// Verify the sizing of titlebar - should be affixed on top of regular titlebar.
TEST_F(PanelCocoaTest, TitlebarViewSizing) {
  Panel* panel = CreateTestPanel("Test Panel");

  PanelCocoa* native_window = static_cast<PanelCocoa*>(panel->native_panel());
  PanelTitlebarViewCocoa* titlebar = [native_window->controller_ titlebarView];

  NSView* contentView = [[native_window->controller_ window] contentView];
  VerifyTitlebarLocation(contentView, titlebar);

  // In local coordinate system, width of titlebar should match width of
  // content view of the window. They both use the same scale factor.
  EXPECT_EQ(NSWidth([contentView bounds]), NSWidth([titlebar bounds]));

  NSRect oldTitleFrame = [[titlebar title] frame];
  NSRect oldIconFrame = [[titlebar icon] frame];

  // Now resize the Panel, see that titlebar follows.
  const int kDelta = 153;  // random number
  gfx::Rect bounds = panel->GetBounds();
  // Grow panel in a way so that its titlebar moves and grows.
  bounds.set_x(bounds.x() - kDelta);
  bounds.set_y(bounds.y() - kDelta);
  bounds.set_width(bounds.width() + kDelta);
  bounds.set_height(bounds.height() + kDelta);

  PanelAnimatedBoundsObserver bounds_observer(panel);
  native_window->SetPanelBounds(bounds);
  bounds_observer.Wait();

  // Verify the panel resized.
  NSRect window_frame = [[native_window->controller_ window] frame];
  EXPECT_EQ(NSWidth(window_frame), bounds.width());
  EXPECT_EQ(NSHeight(window_frame), bounds.height());

  // Verify the titlebar is still on top of regular titlebar.
  VerifyTitlebarLocation(contentView, titlebar);

  // Verify that the title/icon frames were updated.
  NSRect newTitleFrame = [[titlebar title] frame];
  NSRect newIconFrame = [[titlebar icon] frame];

  EXPECT_EQ(newTitleFrame.origin.x - newIconFrame.origin.x,
            oldTitleFrame.origin.x - oldIconFrame.origin.x);
  // Icon and Text should remain at the same left-aligned position.
  EXPECT_EQ(newTitleFrame.origin.x, oldTitleFrame.origin.x);
  EXPECT_EQ(newIconFrame.origin.x, oldIconFrame.origin.x);

  ClosePanelAndWait(panel);
}

// Verify closing behavior of titlebar close button.
TEST_F(PanelCocoaTest, TitlebarViewClose) {
  Panel* panel = CreateTestPanel("Test Panel");
  PanelCocoa* native_window = static_cast<PanelCocoa*>(panel->native_panel());

  PanelTitlebarViewCocoa* titlebar = [native_window->controller_ titlebarView];
  EXPECT_TRUE(titlebar);

  PanelManager* manager = PanelManager::GetInstance();
  EXPECT_EQ(1, manager->num_panels());
  // Simulate clicking Close Button and wait until the Panel closes.
  content::WindowedNotificationObserver signal(
      chrome::NOTIFICATION_PANEL_CLOSED,
      content::Source<Panel>(panel));
  [titlebar simulateCloseButtonClick];
  signal.Wait();
  EXPECT_EQ(0, manager->num_panels());
}

// Verify some menu items being properly enabled/disabled for panels.
TEST_F(PanelCocoaTest, MenuItems) {
  Panel* panel = CreateTestPanel("Test Panel");

  scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@""]);
  NSMenuItem* close_tab_menu_item = CreateMenuItem(menu, IDC_CLOSE_TAB);
  NSMenuItem* close_window_menu_item = CreateMenuItem(menu, IDC_CLOSE_WINDOW);
  NSMenuItem* find_menu_item = CreateMenuItem(menu, IDC_FIND);
  NSMenuItem* find_previous_menu_item = CreateMenuItem(menu, IDC_FIND_PREVIOUS);
  NSMenuItem* find_next_menu_item = CreateMenuItem(menu, IDC_FIND_NEXT);
  NSMenuItem* fullscreen_menu_item = CreateMenuItem(menu, IDC_FULLSCREEN);
  NSMenuItem* presentation_menu_item =
      CreateMenuItem(menu, IDC_PRESENTATION_MODE);
  NSMenuItem* sync_menu_item = CreateMenuItem(menu, IDC_SHOW_SYNC_SETUP);

  PanelCocoa* native_window = static_cast<PanelCocoa*>(panel->native_panel());
  PanelWindowControllerCocoa* panel_controller = native_window->controller_;
  for (NSMenuItem *item in [menu itemArray])
    [item setTarget:panel_controller];

  [menu update];  // Trigger validation of menu items.
  EXPECT_FALSE([close_tab_menu_item isEnabled]);
  EXPECT_TRUE([close_window_menu_item isEnabled]);
  // No find support. Panels don't have a find bar.
  EXPECT_FALSE([find_menu_item isEnabled]);
  EXPECT_FALSE([find_previous_menu_item isEnabled]);
  EXPECT_FALSE([find_next_menu_item isEnabled]);
  EXPECT_FALSE([fullscreen_menu_item isEnabled]);
  EXPECT_FALSE([presentation_menu_item isEnabled]);
  EXPECT_FALSE([sync_menu_item isEnabled]);

  // Verify that commandDispatch on an invalid menu item does not crash.
  [NSApp sendAction:[sync_menu_item action]
                 to:[sync_menu_item target]
               from:sync_menu_item];

  ClosePanelAndWait(panel);
}

TEST_F(PanelCocoaTest, KeyEvent) {
  Panel* panel = CreateTestPanel("Test Panel");
  NSEvent* event = [NSEvent keyEventWithType:NSKeyDown
                                    location:NSZeroPoint
                               modifierFlags:NSControlKeyMask
                                   timestamp:0.0
                                windowNumber:0
                                     context:nil
                                  characters:@""
                 charactersIgnoringModifiers:@""
                                   isARepeat:NO
                                     keyCode:kVK_Tab];
  PanelCocoa* native_window = static_cast<PanelCocoa*>(panel->native_panel());
  [BrowserWindowUtils handleKeyboardEvent:event
                      inWindow:[native_window->controller_ window]];
  ClosePanelAndWait(panel);
}

// Verify that the theme provider is properly plumbed through.
TEST_F(PanelCocoaTest, ThemeProvider) {
  Panel* panel = CreateTestPanel("Test Panel");
  ASSERT_TRUE(panel);

  PanelCocoa* native_window = static_cast<PanelCocoa*>(panel->native_panel());
  ASSERT_TRUE(native_window);
  EXPECT_TRUE(NULL != [[native_window->controller_ window] themeProvider]);
  ClosePanelAndWait(panel);
}

TEST_F(PanelCocoaTest, SetTitle) {
  NSString *appName = @"Test Panel";
  Panel* panel = CreateTestPanel(base::SysNSStringToUTF8(appName));
  ASSERT_TRUE(panel);

  PanelCocoa* native_window = static_cast<PanelCocoa*>(panel->native_panel());
  ASSERT_TRUE(native_window);
  NSString* previousTitle = [[native_window->controller_ window] title];
  EXPECT_NSNE(appName, previousTitle);
  [native_window->controller_ updateTitleBar];
  chrome::testing::NSRunLoopRunAllPending();
  NSString* currentTitle = [[native_window->controller_ window] title];
  EXPECT_NSEQ(appName, currentTitle);
  EXPECT_NSNE(currentTitle, previousTitle);
  ClosePanelAndWait(panel);
}

TEST_F(PanelCocoaTest, ActivatePanel) {
  Panel* panel = CreateTestPanel("Test Panel");
  Panel* panel2 = CreateTestPanel("Test Panel 2");
  ASSERT_TRUE(panel);
  ASSERT_TRUE(panel2);

  PanelCocoa* native_window = static_cast<PanelCocoa*>(panel->native_panel());
  ASSERT_TRUE(native_window);
  PanelCocoa* native_window2 = static_cast<PanelCocoa*>(panel2->native_panel());
  ASSERT_TRUE(native_window2);

  // No one has a good answer why but apparently windows can't take keyboard
  // focus outside of interactive UI tests. BrowserWindowController uses the
  // same way of testing this.
  native_window->ActivatePanel();
  NSWindow* frontmostWindow = [[NSApp orderedWindows] objectAtIndex:0];
  EXPECT_NSEQ(frontmostWindow, [native_window->controller_ window]);

  native_window2->ActivatePanel();
  frontmostWindow = [[NSApp orderedWindows] objectAtIndex:0];
  EXPECT_NSEQ(frontmostWindow, [native_window2->controller_ window]);

  ClosePanelAndWait(panel);
  ClosePanelAndWait(panel2);
}
