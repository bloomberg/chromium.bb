// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_WINDOW_PROXY_H__
#define CHROME_TEST_AUTOMATION_WINDOW_PROXY_H__
#pragma once

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/string16.h"
#include "base/threading/thread.h"
#include "chrome/test/automation/automation_handle_tracker.h"
#include "ui/base/keycodes/keyboard_codes.h"

class BrowserProxy;
class WindowProxy;

namespace gfx {
  class Point;
  class Rect;
}

// This class presents the interface to actions that can be performed on a given
// window.  Note that this object can be invalidated at any time if the
// corresponding window in the app is closed.  In that case, any subsequent
// calls will return false immediately.
class WindowProxy : public AutomationResourceProxy {
 public:
  WindowProxy(AutomationMessageSender* sender,
              AutomationHandleTracker* tracker,
              int handle)
    : AutomationResourceProxy(tracker, sender, handle) {}

  // Simulates a click at the OS level. |click| is in the window's coordinates
  // and |flags| specifies which buttons are pressed (as defined in
  // chrome/views/event.h).  Note that this is equivalent to the user moving
  // the mouse and pressing the button.  So if there is a window on top of this
  // window, the top window is clicked.
  bool SimulateOSClick(const gfx::Point& click, int flags);

  // Moves the mouse pointer this location at the OS level.  |location| is
  // in the window's coordinates.
  bool SimulateOSMouseMove(const gfx::Point& location);

  // Get the title of the top level window.
  bool GetWindowTitle(string16* text);

  // Simulates a key press at the OS level. |key| is the virtual key code of the
  // key pressed and |flags| specifies which modifiers keys are also pressed (as
  // defined in chrome/views/event.h).  Note that this actually sends the event
  // to the window that has focus.
  bool SimulateOSKeyPress(ui::KeyboardCode key, int flags);

  // Shows/hides the window and as a result makes it active/inactive.
  // Returns true if the call was successful.
  bool SetVisible(bool visible);

  // Sets |active| to true if this view is currently the active window.
  // Returns true if the call was successful.
  bool IsActive(bool* active);

  // Make this window the active window.
  // Returns true if the call was successful.
  bool Activate();

  // Sets |maximized| to true if this window is maximized.
  bool IsMaximized(bool* maximized);

  // Gets the bounds (in window coordinates) that correspond to the view with
  // the given ID in this window.  Returns true if bounds could be obtained.
  // If |screen_coordinates| is true, the bounds are returned in the coordinates
  // of the screen, if false in the coordinates of the browser.
  bool GetViewBounds(int view_id, gfx::Rect* bounds, bool screen_coordinates);

  // Gets the position and size of the window. Returns true if setting the
  // bounds was successful.
  bool GetBounds(gfx::Rect* bounds);

  // Sets the position and size of the window. Returns true if setting the
  // bounds was successful.
  bool SetBounds(const gfx::Rect& bounds);

  // Returns the browser this window corresponds to, or NULL if this window
  // is not a browser.  The caller owns the returned BrowserProxy.
  scoped_refptr<BrowserProxy> GetBrowser();

  // Same as GetWindow except return NULL if response isn't received
  // before the specified timeout.
  scoped_refptr<BrowserProxy> GetBrowserWithTimeout(uint32 timeout_ms,
      bool* is_timeout);
 protected:
  virtual ~WindowProxy() {}
 private:
  DISALLOW_COPY_AND_ASSIGN(WindowProxy);
};

#endif  // CHROME_TEST_AUTOMATION_WINDOW_PROXY_H__
