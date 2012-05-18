// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares the C++ side of PyAuto, the python interface to
// Chromium automation.  It access Chromium's internals using Automation Proxy.

#ifndef CHROME_TEST_PYAUTOLIB_PYAUTOLIB_H_
#define CHROME_TEST_PYAUTOLIB_PYAUTOLIB_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/test/test_timeouts.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/test/ui/ui_test_suite.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

class AutomationProxy;

// The C++ style guide forbids using default arguments but I'm taking the
// liberty of allowing it in this file. The sole purpose of this (and the
// .cc) is to support the python interface, and default args are allowed in
// python. Strictly adhering to the guide here would mean having to re-define
// all methods in python just for the sake of providing default args. This
// seems cumbersome and unwanted.

// Test Suite for Pyauto tests. All one-time initializations go here.
class PyUITestSuiteBase : public UITestSuite {
 public:
  PyUITestSuiteBase(int argc, char** argv);
  virtual ~PyUITestSuiteBase();

  void InitializeWithPath(const FilePath& browser_dir);

  void SetCrSourceRoot(const FilePath& path);

 private:
#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool pool_;
#endif
};

// The primary class that interfaces with Automation Proxy.
// This class is accessed from python using swig.
class PyUITestBase : public UITestBase {
 public:
  // Only public methods are accessible from swig.

  // Constructor. Lookup pyauto.py for doc on these args.
  PyUITestBase(bool clear_profile, std::wstring homepage);
  virtual ~PyUITestBase();

  // Initialize the setup. Should be called before launching the browser.
  // |browser_dir| is the path to dir containing chromium binaries.
  void Initialize(const FilePath& browser_dir);

  void UseNamedChannelID(const std::string& named_channel_id) {
    named_channel_id_ = named_channel_id;
    launcher_.reset(CreateProxyLauncher());
  }

  virtual ProxyLauncher* CreateProxyLauncher() OVERRIDE;

  // SetUp,TearDown is redeclared as public to make it accessible from swig.
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // Navigate to the given URL in the active tab. Blocks until page loaded.
  void NavigateToURL(const char* url_string);

  // Navigate to the given URL in the active tab in the given window.
  void NavigateToURL(const char* url_string, int window_index);

  // Navigate to the given URL in given tab in the given window.
  // Blocks until page loaded.
  void NavigateToURL(const char* url_string, int window_index, int tab_index);

  // Reloads the active tab in the given window.
  // Blocks until page reloaded.
  void ReloadActiveTab(int window_index = 0);

  // Get the URL of the active tab.
  GURL GetActiveTabURL(int window_index = 0);

  int GetTabCount(int window_index = 0);

  // Appends a new tab with the given URL in the given or first browser window.
  bool AppendTab(const GURL& tab_url, int window_index = 0);

  // Activate the tab at the given zero-based index in the given or first
  // browser window.  Also brings the window to front.
  bool ActivateTab(int tab_index, int window_index = 0);

  // Apply the accelerator with given id (IDC_BACK, IDC_NEWTAB ...) to the
  // browser window at the given or first index.
  // The list can be found at chrome/app/chrome_command_ids.h
  // Returns true if the call was successful.
  bool ApplyAccelerator(int id, int window_index = 0);

  // Like ApplyAccelerator except that it waits for the command to execute.
  bool RunCommand(int browser_command, int window_index = 0);

  // Returns true if the given command id is enabled on the given window.
  bool IsMenuCommandEnabled(int browser_command, int window_index = 0);

  // Shows or hides the download shelf.
  void SetDownloadShelfVisible(bool is_visible, int window_index = 0);

  // Determines the visibility of the download shelf
  bool IsDownloadShelfVisible(int window_index = 0);

  // Open the Find box
  void OpenFindInPage(int window_index = 0);

  // Determines the visibility of the Find box
  bool IsFindInPageVisible(int window_index = 0);

  // Get the path to the downloads directory
  FilePath GetDownloadDirectory();

  // AutomationProxy methods

  // Open a new browser window. Returns false on failure.
  bool OpenNewBrowserWindow(bool show);

  // Close a browser window. Returns false on failure.
  bool CloseBrowserWindow(int window_index);

  // Fetch the number of browser windows. Includes popups.
  int GetBrowserWindowCount();

  // Returns bookmark bar visibility state.
  bool GetBookmarkBarVisibility();

  // Returns true if the bookmark bar is visible in the detached state.
  bool IsBookmarkBarDetached();

  // Returns bookmark bar animation state.  Warning: timing issues may
  // change this return value unexpectedly.
  bool IsBookmarkBarAnimating();

  // Wait for the bookmark bar animation to complete.
  // If |wait_for_open| is true, wait for it to open.
  // If |wait_for_open| is false, wait for it to close.
  bool WaitForBookmarkBarVisibilityChange(bool wait_for_open);

  // Get the bookmarks as a JSON string.  Internal method.
  std::string _GetBookmarksAsJSON();

  // Editing of the bookmark model.  Bookmarks are referenced by id.
  // The id is a std::wstring, not an int64, for convenience, since
  // the python side gets IDs converted from a JSON representation
  // (which "extracts" into a string, not an int).  Since IDs are
  // grabbed from the current model (and not generated), a conversion
  // is unnecessary.  URLs are strings and not GURLs for a similar reason.
  // Bookmark or group (folder) creation:
  bool AddBookmarkGroup(std::wstring& parent_id, int index,
                        std::wstring& title);
  bool AddBookmarkURL(std::wstring& parent_id, int index,
                      std::wstring& title, std::wstring& url);
  // Bookmark editing:
  bool ReparentBookmark(std::wstring& id, std::wstring& new_parent_id,
                        int index);
  bool SetBookmarkTitle(std::wstring& id, std::wstring& title);
  bool SetBookmarkURL(std::wstring& id, std::wstring& url);
  // Finally, bookmark deletion:
  bool RemoveBookmark(std::wstring& id);

  // Get a handle to browser window at the given index, or NULL on failure.
  scoped_refptr<BrowserProxy> GetBrowserWindow(int window_index);

  // Meta-methods.  Generic pattern of passing args and response as
  // JSON dict to avoid future use of the SWIG interface and
  // automation proxy additions.  Returns response as JSON dict.
  // Use -ve window_index for automation calls not targetted at a browser
  // window.  Example: Login call for chromeos.
  std::string _SendJSONRequest(int window_index,
                               const std::string& request,
                               int timeout);

  // Resets to the default theme. Returns true on success.
  bool ResetToDefaultTheme();

  // Sets a cookie value for a url. Returns true on success.
  bool SetCookie(const GURL& cookie_url, const std::string& value,
                 int window_index = 0, int tab_index = 0);
  // Gets a cookie value for the given url.
  std::string GetCookie(const GURL& cookie_url, int window_index = 0,
                        int tab_index = 0);

  int action_max_timeout_ms() const {
    return TestTimeouts::action_max_timeout_ms();
  }

  int large_test_timeout_ms() const {
    return TestTimeouts::large_test_timeout_ms();
  }

 protected:
  // Gets the automation proxy and checks that it exists.
  virtual AutomationProxy* automation() const OVERRIDE;

  virtual void SetLaunchSwitches() OVERRIDE;

 private:
  // Gets the current state of the bookmark bar. Returns false if it failed.
  bool GetBookmarkBarState(bool* visible, bool* detached);

  // Enables PostTask to main thread.
  // Should be shared across multiple instances of PyUITestBase so that this
  // class is re-entrant and multiple instances can be created.
  // This is necessary since python's unittest module creates instances of
  // TestCase at load time itself.
  static MessageLoop* GetSharedMessageLoop(MessageLoop::Type msg_loop_type);
  static MessageLoop* message_loop_;

  // Path to named channel id.
  std::string named_channel_id_;
};

#endif  // CHROME_TEST_PYAUTOLIB_PYAUTOLIB_H_
