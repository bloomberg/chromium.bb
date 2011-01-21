// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/testing_automation_provider.h"

#import <Cocoa/Cocoa.h>

#include "ui/base/l10n/l10n_util.h"
#include "base/logging.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/automation/automation_browser_tracker.h"
#include "chrome/browser/automation/automation_window_tracker.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/tabs/tab_window_controller.h"
#include "chrome/browser/ui/view_ids.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

void TestingAutomationProvider::ActivateWindow(int handle) {
  NOTIMPLEMENTED();
}

void TestingAutomationProvider::IsWindowMaximized(int handle,
                                                  bool* is_maximized,
                                                  bool* success) {
  *success = false;
  NSWindow* window = window_tracker_->GetResource(handle);
  if (!window)
    return;
  *is_maximized = [window isZoomed];
  *success = true;
}

void TestingAutomationProvider::TerminateSession(int handle, bool* success) {
  *success = false;
  NOTIMPLEMENTED();
}

void TestingAutomationProvider::WindowGetViewBounds(int handle,
                                                    int view_id,
                                                    bool screen_coordinates,
                                                    bool* success,
                                                    gfx::Rect* bounds) {
  *success = false;

  // At the moment we hard code the view ID used by WebDriver and do
  // not support arbitrary view IDs.  suzhe is working on general view
  // ID support for the Mac.
  if (view_id != VIEW_ID_TAB_CONTAINER) {
    NOTIMPLEMENTED();
    return;
  }

  NSWindow* window = window_tracker_->GetResource(handle);
  if (!window)
    return;

  BrowserWindowController* controller = [window windowController];
  DCHECK([controller isKindOfClass:[BrowserWindowController class]]);
  if (![controller isKindOfClass:[BrowserWindowController class]])
    return;
  NSView* tab =
      [[[controller tabStripController] activeTabContentsController] view];
  if (!tab)
    return;

  NSRect rect = [tab convertRectToBase:[tab bounds]];
  // The origin of the bounding box should be the top left of the tab contents,
  // not bottom left, to match other platforms.
  rect.origin.y += rect.size.height;
  CGFloat coord_sys_height;
  if (screen_coordinates && [[NSScreen screens] count] > 0) {
    rect.origin = [window convertBaseToScreen:rect.origin];
    coord_sys_height =
        [[[NSScreen screens] objectAtIndex:0] frame].size.height;
  } else {
    coord_sys_height = [window frame].size.height;
  }
  // Flip coordinate system.
  rect.origin.y = coord_sys_height - rect.origin.y;
  *bounds = gfx::Rect(NSRectToCGRect(rect));
  *success = true;
}

void TestingAutomationProvider::GetWindowBounds(int handle,
                                                gfx::Rect* bounds,
                                                bool* success) {
  *success = false;
  NSWindow* window = window_tracker_->GetResource(handle);
  if (window) {
    // Move rect to reflect flipped coordinate system.
    if ([[NSScreen screens] count] > 0) {
      NSRect rect = [window frame];
      rect.origin.y =
          [[[NSScreen screens] objectAtIndex:0] frame].size.height -
              rect.origin.y - rect.size.height;
      *bounds = gfx::Rect(NSRectToCGRect(rect));
      *success = true;
    }
  }
}

void TestingAutomationProvider::SetWindowBounds(int handle,
                                                const gfx::Rect& bounds,
                                                bool* success) {
  *success = false;
  NSWindow* window = window_tracker_->GetResource(handle);
  if (window) {
    NSRect new_bounds = NSRectFromCGRect(bounds.ToCGRect());

    if ([[NSScreen screens] count] > 0) {
      new_bounds.origin.y =
          [[[NSScreen screens] objectAtIndex:0] frame].size.height -
          new_bounds.origin.y - new_bounds.size.height;
    }

    [window setFrame:new_bounds display:NO];
    *success = true;
  }
}

void TestingAutomationProvider::SetWindowVisible(int handle,
                                                 bool visible,
                                                 bool* result) {
  *result = false;
  NSWindow* window = window_tracker_->GetResource(handle);
  if (window) {
    if (visible) {
      [window orderFront:nil];
    } else {
      [window orderOut:nil];
    }
    *result = true;
  }
}

void TestingAutomationProvider::GetWindowTitle(int handle, string16* text) {
  gfx::NativeWindow window = window_tracker_->GetResource(handle);
  NSString* title = nil;
  if ([[window delegate] isKindOfClass:[TabWindowController class]]) {
    TabWindowController* delegate =
        reinterpret_cast<TabWindowController*>([window delegate]);
    title = [delegate selectedTabTitle];
  } else {
    title = [window title];
  }
  // If we don't yet have a title, use "Untitled".
  if (![title length]) {
    text->assign(l10n_util::GetStringUTF16(
        IDS_BROWSER_WINDOW_MAC_TAB_UNTITLED));
    return;
  }

  text->assign(base::SysNSStringToUTF16(title));
}
