// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/testing_automation_provider.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "chrome/browser/automation/automation_browser_tracker.h"
#include "chrome/browser/automation/automation_window_tracker.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#include "chrome/browser/cocoa/tab_window_controller.h"
#include "chrome/browser/view_ids.h"

void TestingAutomationProvider::ActivateWindow(int handle) {
  NOTIMPLEMENTED();
}

void TestingAutomationProvider::IsWindowMaximized(int handle,
                                                  bool* is_maximized,
                                                  bool* success) {
  *success = false;
  NOTIMPLEMENTED();
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
  NSView* tab = [controller selectedTabView];
  if (!tab)
    return;

  NSPoint coords = NSZeroPoint;
  if (screen_coordinates) {
    coords = [window convertBaseToScreen:[tab convertPoint:NSZeroPoint
                                                    toView:nil]];
  } else {
    coords = [tab convertPoint:NSZeroPoint toView:[window contentView]];
  }
  // Flip coordinate system
  coords.y = [[window screen] frame].size.height - coords.y;
  *success = true;
}

void TestingAutomationProvider::GetWindowBounds(int handle,
                                                gfx::Rect* bounds,
                                                bool* result) {
  *result = false;
  NOTIMPLEMENTED();
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

