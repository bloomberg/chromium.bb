// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_BROWSER_PROXY_H_
#define CHROME_TEST_AUTOMATION_BROWSER_PROXY_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/content_settings.h"
#include "chrome/test/automation/automation_handle_tracker.h"

class GURL;
class TabProxy;
class WindowProxy;

namespace gfx {
  class Point;
  class Rect;
}

// This class presents the interface to actions that can be performed on
// a given browser window.  Note that this object can be invalidated at any
// time if the corresponding browser window in the app is closed.  In that case,
// any subsequent calls will return false immediately.
class BrowserProxy : public AutomationResourceProxy {
 public:
  BrowserProxy(AutomationMessageSender* sender,
               AutomationHandleTracker* tracker,
               int handle)
    : AutomationResourceProxy(tracker, sender, handle) {}

  // Activates the tab corresponding to (zero-based) tab_index. Returns true if
  // successful.
  bool ActivateTab(int tab_index) WARN_UNUSED_RESULT;

  // Bring the browser window to the front, activating it. Returns true on
  // success.
  bool BringToFront() WARN_UNUSED_RESULT;

  // Append a new tab to the TabStrip.  The new tab is selected.
  // The new tab navigates to the given tab_url.
  // Returns true if successful.
  bool AppendTab(const GURL& tab_url) WARN_UNUSED_RESULT;

  // Gets the (zero-based) index of the currently active tab. Returns true if
  // successful.
  bool GetActiveTabIndex(int* active_tab_index) const WARN_UNUSED_RESULT;

  // Returns the number of tabs in the given window.  Returns true if
  // the call was successful.
  bool GetTabCount(int* num_tabs) const WARN_UNUSED_RESULT;

  // Returns the type of the given window. Returns true if the call was
  // successful.
  bool GetType(Browser::Type* type) const WARN_UNUSED_RESULT;

  // Returns the TabProxy for the tab at the given index, transferring
  // ownership of the pointer to the caller. On failure, returns NULL.
  //
  // Use GetTabCount to see how many windows you can ask for. Tab numbers
  // are 0-based.
  scoped_refptr<TabProxy> GetTab(int tab_index) const;

  // Returns the TabProxy for the currently active tab, transferring
  // ownership of the pointer to the caller. On failure, returns NULL.
  scoped_refptr<TabProxy> GetActiveTab() const;

  // Returns the WindowProxy for this browser's window. It can be used to
  // retreive view bounds, simulate clicks and key press events.  The caller
  // owns the returned WindowProxy.
  // On failure, returns NULL.
  scoped_refptr<WindowProxy> GetWindow() const;

  // Apply the accelerator with given id (IDC_BACK, IDC_NEWTAB ...)
  // The list can be found at chrome/app/chrome_command_ids.h
  // Returns true if the call was successful.
  //
  // The alternate way to test the accelerators is to use the Windows messaging
  // system to send the actual keyboard events (ui_controls.h) A precondition
  // to using this system is that the target window should have the keyboard
  // focus. This leads to a flaky test behavior in circumstances when the
  // desktop screen is locked or the test is being executed over a remote
  // desktop.
  bool ApplyAccelerator(int id) WARN_UNUSED_RESULT;

  // Block the thread until the tab count is |count|.
  // Returns true on success.
  bool WaitForTabCountToBecome(int count) WARN_UNUSED_RESULT;

  // Block the thread until the specified tab is the active tab.
  // |wait_timeout| is the timeout, in milliseconds, for waiting.
  // Returns false if the tab does not become active.
  bool WaitForTabToBecomeActive(
      int tab,
      base::TimeDelta wait_timeout) WARN_UNUSED_RESULT;

  // Returns whether the Find window is fully visible If animating, |is_visible|
  // will be false. Returns false on failure.
  bool IsFindWindowFullyVisible(bool* is_visible) WARN_UNUSED_RESULT;

  // Run the specified command in the browser
  // (see Browser::ExecuteCommandWithDisposition() for the list of supported
  // commands).  Returns true if the command was successfully dispatched,
  // false otherwise.
  bool RunCommandAsync(int browser_command) const WARN_UNUSED_RESULT;

  // Run the specified command in the browser
  // (see Browser::ExecuteCommandWithDisposition() for the list of supported
  // commands).  Returns true if the command was successfully dispatched and
  // executed, false otherwise.
  bool RunCommand(int browser_command) const WARN_UNUSED_RESULT;

  // Simulates a termination the browser session (as if the user logged off the
  // mahine).
  bool TerminateSession() WARN_UNUSED_RESULT;

  // Generic pattern for sending automation requests.
  bool SendJSONRequest(const std::string& request,
                       int timeout_ms,
                       std::string* response) WARN_UNUSED_RESULT;

  // Gets the load times for all tabs started from the command line.
  // Puts the time of the first tab to start loading into |min_start_time|,
  // the time when loading stopped into |max_stop_time| (should be similar to
  // the delay that WaitForInitialLoads waits for), and a list of all
  // finished timestamps into |stop_times|. Returns true on success.
  bool GetInitialLoadTimes(
      base::TimeDelta timeout,
      float* min_start_time,
      float* max_stop_time,
      std::vector<float>* stop_times);


 protected:
  virtual ~BrowserProxy() {}
 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserProxy);
};

#endif  // CHROME_TEST_AUTOMATION_BROWSER_PROXY_H_
