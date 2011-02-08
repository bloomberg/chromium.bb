// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares the C++ side of PyAuto, the python interface to
// Chromium automation.  It access Chromium's internals using Automation Proxy.

#ifndef CHROME_TEST_PYAUTOLIB_PYAUTOLIB_H_
#define CHROME_TEST_PYAUTOLIB_PYAUTOLIB_H_
#pragma once

#include "base/message_loop.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/test/test_timeouts.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/test/ui/ui_test_suite.h"

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
  ~PyUITestSuiteBase();

  void Initialize(const FilePath& browser_dir);

  void SetCrSourceRoot(const FilePath& path);

 private:
  base::mac::ScopedNSAutoreleasePool pool_;
};

// The primary class that interfaces with Automation Proxy.
// This class is accessed from python using swig.
class PyUITestBase : public UITestBase {
 public:
  // Only public methods are accessible from swig.

  // Constructor. Lookup pyauto.py for doc on these args.
  PyUITestBase(bool clear_profile, std::wstring homepage);
  ~PyUITestBase();

  // Initialize the setup. Should be called before launching the browser.
  // |browser_dir| is the path to dir containing chromium binaries.
  void Initialize(const FilePath& browser_dir);

  // SetUp,TearDown is redeclared as public to make it accessible from swig.
  virtual void SetUp();
  virtual void TearDown();

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
  // browser window.
  bool ActivateTab(int tab_index, int window_index = 0);

  // Apply the accelerator with given id (IDC_BACK, IDC_NEWTAB ...) to the
  // browser window at the given or first index.
  // The list can be found at chrome/app/chrome_command_ids.h
  // Returns true if the call was successful.
  bool ApplyAccelerator(int id, int window_index = 0);

  // Like ApplyAccelerator except that it waits for the command to execute.
  bool RunCommand(int browser_command, int window_index = 0);

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

  // Installs the extension crx. Returns true only if extension was installed
  // and loaded successfully. Overinstalls will fail.
  bool InstallExtension(const FilePath& crx_file, bool with_ui);

  // Returns bookmark bar visibility state.
  bool GetBookmarkBarVisibility();

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

  // Meta-method.  Experimental pattern of passing args and response as
  // JSON dict to avoid future use of the SWIG interface and
  // automation proxy additions.  Returns response as JSON dict.
  std::string _SendJSONRequest(int window_index, std::string& request);

  // Execute javascript in a given tab, and return the response. This is
  // a low-level method intended for use mostly by GetDOMValue(). Note that
  // any complicated manipulation of the page should be done by something
  // like WebDriver, not PyAuto. Also note that in order for the script to
  // return a value to the calling code, it invoke
  // window.domAutomationController.send(), passing in the intended return
  // value.
  std::wstring ExecuteJavascript(const std::wstring& script,
                                 int window_index = 0,
                                 int tab_index = 0,
                                 const std::wstring& frame_xpath = L"");

  // Evaluate a Javascript expression and return the result as a string. This
  // method is intended largely to read values out of the frame DOM.
  std::wstring GetDOMValue(const std::wstring& expr,
                           int window_index = 0,
                           int tab_index = 0,
                           const std::wstring& frame_xpath = L"");

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

  int command_execution_timeout_ms() const {
    return TestTimeouts::command_execution_timeout_ms();
  }

 private:
  // Enables PostTask to main thread.
  // Should be shared across multiple instances of PyUITestBase so that this
  // class is re-entrant and multiple instances can be created.
  // This is necessary since python's unittest module creates instances of
  // TestCase at load time itself.
  static MessageLoop* GetSharedMessageLoop(MessageLoop::Type msg_loop_type);
  static MessageLoop* message_loop_;
};

#endif  // CHROME_TEST_PYAUTOLIB_PYAUTOLIB_H_
