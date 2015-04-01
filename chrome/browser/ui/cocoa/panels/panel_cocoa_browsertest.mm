// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/panels/base_panel_browser_test.h"
#include "chrome/browser/ui/panels/panel.h"
#include "content/public/test/test_utils.h"

// Class that spins a run loop until an NSWindow gains key status.
@interface CocoaActivationWaiter : NSObject {
 @private
  base::RunLoop* runLoop_;
  BOOL observed_;
}

- (id)initWithWindow:(NSWindow*)window;
- (void)windowDidBecomeKey:(NSNotification*)notification;
- (BOOL)waitUntilActive;

@end

@implementation CocoaActivationWaiter

- (id)initWithWindow:(NSWindow*)window {
  EXPECT_FALSE([window isKeyWindow]);
  if ((self = [super init])) {
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(windowDidBecomeKey:)
               name:NSWindowDidBecomeKeyNotification
             object:window];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
  observed_ = YES;
  if (runLoop_)
    runLoop_->Quit();
}

- (BOOL)waitUntilActive {
  if (observed_)
    return YES;

  base::RunLoop runLoop;
  base::MessageLoop::current()->task_runner()->PostDelayedTask(
      FROM_HERE, runLoop.QuitClosure(), TestTimeouts::action_timeout());
  runLoop_ = &runLoop;
  runLoop.Run();
  runLoop_ = nullptr;
  return observed_;
}

@end

typedef BasePanelBrowserTest PanelCocoaBrowserTest;

IN_PROC_BROWSER_TEST_F(PanelCocoaBrowserTest, MenuItems) {
  Panel* panel = CreatePanel("Panel");

  // Close main tabbed window.
  content::WindowedNotificationObserver signal(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::Source<Browser>(browser()));
  chrome::CloseWindow(browser());
  signal.Wait();

  // There should be no browser windows.
  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());

  // There should be one panel.
  EXPECT_EQ(1, PanelManager::GetInstance()->num_panels());

  NSMenu* mainMenu = [NSApp mainMenu];
  EXPECT_TRUE(mainMenu);

  // Chrome(0) File(1) ....
  // Get File submenu. It doesn't have a command id, fetch it by index.
  NSMenu* fileSubmenu = [[[mainMenu itemArray] objectAtIndex:1] submenu];
  EXPECT_TRUE(fileSubmenu);
  [fileSubmenu update];

  // Verify the items normally enabled for "all windows closed" case are
  // also enabled when there is a panel but no browser windows on the screen.
  NSMenuItem* item = [fileSubmenu itemWithTag:IDC_NEW_TAB];
  EXPECT_TRUE(item);
  EXPECT_TRUE([item isEnabled]);

  item = [fileSubmenu itemWithTag:IDC_NEW_WINDOW];
  EXPECT_TRUE(item);
  EXPECT_TRUE([item isEnabled]);

  item = [fileSubmenu itemWithTag:IDC_NEW_INCOGNITO_WINDOW];
  EXPECT_TRUE(item);
  EXPECT_TRUE([item isEnabled]);

  NSMenu* historySubmenu = [[mainMenu itemWithTag:IDC_HISTORY_MENU] submenu];
  EXPECT_TRUE(historySubmenu);
  [historySubmenu update];

  // These should be disabled.
  item = [historySubmenu itemWithTag:IDC_HOME];
  EXPECT_TRUE(item);
  EXPECT_FALSE([item isEnabled]);

  item = [historySubmenu itemWithTag:IDC_BACK];
  EXPECT_TRUE(item);
  EXPECT_FALSE([item isEnabled]);

  item = [historySubmenu itemWithTag:IDC_FORWARD];
  EXPECT_TRUE(item);
  EXPECT_FALSE([item isEnabled]);

  // 'Close Window' should be enabled because the remaining Panel is a Responder
  // which implements performClose:, the 'action' of 'Close Window'.
  for (NSMenuItem *i in [fileSubmenu itemArray]) {
    if ([i action] == @selector(performClose:)) {
      item = i;
      break;
    }
  }
  EXPECT_TRUE(item);
  EXPECT_TRUE([item isEnabled]);

  panel->Close();
}

// Test that panels do not become active when closing a window, even when a
// panel is otherwise the topmost window.
IN_PROC_BROWSER_TEST_F(PanelCocoaBrowserTest, InactivePanelsNotActivated) {
  // Note CreateDockedPanel() sets wait_for_fully_created and SHOW_AS_ACTIVE,
  // so the following spins a run loop until the respective panel is activated.
  Panel* docked_panel_1 = CreateDockedPanel("Panel1", gfx::Rect());
  EXPECT_TRUE([docked_panel_1->GetNativeWindow() isKeyWindow]);

  // Activate the browser. Otherwise closing the second panel will correctly
  // raise the first panel since panels are allowed to become key if they were
  // actually the most recently focused window (as opposed to being merely the
  // topmost window on the z-order stack).
  NSWindow* browser_window = browser()->window()->GetNativeWindow();
  base::scoped_nsobject<CocoaActivationWaiter> waiter(
      [[CocoaActivationWaiter alloc] initWithWindow:browser_window]);
  browser()->window()->Activate();
  EXPECT_TRUE([waiter waitUntilActive]);

  // Creating a second panel will activate it (and make it topmost).
  Panel* docked_panel_2 = CreateDockedPanel("Panel2", gfx::Rect());
  EXPECT_TRUE([docked_panel_2->GetNativeWindow() isKeyWindow]);

  // Verify the assumptions that the panels are actually topmost.
  EXPECT_EQ(docked_panel_2->GetNativeWindow(),
            [[NSApp orderedWindows] objectAtIndex:0]);
  EXPECT_EQ(docked_panel_1->GetNativeWindow(),
            [[NSApp orderedWindows] objectAtIndex:1]);

  // Close the second panel and wait for the browser to become active.
  waiter.reset([[CocoaActivationWaiter alloc] initWithWindow:browser_window]);
  docked_panel_2->Close();

  EXPECT_TRUE([waiter waitUntilActive]);
  EXPECT_TRUE([browser_window isKeyWindow]);
}
