// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Swig Interface for PyAuto.
// PyAuto makes the Automation Proxy interface available in Python
//
// Running swig as:
//   swig -python -c++ chrome/test/pyautolib/pyautolib.i
// would generate pyautolib.py, pyautolib_wrap.cxx

// When adding a new class or method, make sure you specify the doc string using
// %feature("docstring", "doc string goes here") NODENAME;
// and attach it to your node (class or method). This doc string will be
// copied over in the generated python classes/methods.

%module(docstring="Python interface to Automtion Proxy.") pyautolib
%feature("autodoc", "1");

%include <std_wstring.i>
%include <std_string.i>

%include "chrome/test/pyautolib/argc_argv.i"

// NOTE: All files included in this file should also be listed under
//       pyautolib_sources in chrome_tests.gypi.

// Headers that can be swigged directly.
%include "chrome/app/chrome_command_ids.h"
%include "chrome/app/chrome_dll_resource.h"
%include "chrome/common/automation_constants.h"
%include "chrome/common/pref_names.h"

%{
#include "chrome/common/automation_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/pyautolib/pyautolib.h"
%}

// Handle type uint32 conversions as int
%apply int { uint32 };

// scoped_refptr
template <class T>
class scoped_refptr {
 public:
  scoped_refptr();
  scoped_refptr(T* p);
  ~scoped_refptr();

  T* get() const;
  T* operator->() const;
};
%template(scoped_refptr__BrowserProxy) scoped_refptr<BrowserProxy>;
%template(scoped_refptr__TabProxy) scoped_refptr<TabProxy>;

// GURL
%feature("docstring", "Represent a URL. Call spec() to get the string.") GURL;
class GURL {
 public:
  GURL();
  explicit GURL(const std::string& url_string);
  %feature("docstring", "Get the string representation.") spec;
  const std::string& spec() const;
};

// FilePath
%feature("docstring",
         "Represent a file path. Call value() to get the string.") FilePath;
class FilePath {
 public:
  %feature("docstring", "Get the string representation.") value;
#ifdef SWIGWIN
  typedef std::wstring StringType;
#else
  typedef std::string StringType;
#endif  // SWIGWIN
  const StringType& value() const;
  %feature("docstring", "Construct an empty FilePath from a string.")
      FilePath;
  FilePath();
  explicit FilePath(const StringType& path);
};

// BrowserProxy
%feature("docstring", "Proxy handle to a browser window.") BrowserProxy;
%nodefaultctor BrowserProxy;
%nodefaultdtor BrowserProxy;
class BrowserProxy {
 public:
  %feature("docstring", "Activate the tab at the given zero-based index")
      ActivateTab;
  bool ActivateTab(int tab_index);

  %feature("docstring", "Activate the browser's window and bring it to front.")
      BringToFront;
  bool BringToFront();

  %feature("docstring", "Get proxy to the tab at the given zero-based index")
      GetTab;
  scoped_refptr<TabProxy> GetTab(int tab_index) const;
};

// TabProxy
%feature("docstring", "Proxy handle to a tab.") TabProxy;
%nodefaultctor TabProxy;
%nodefaultdtor TabProxy;
class TabProxy {
 public:
  // Navigation
  %feature("docstring", "Navigates to a given GURL. "
           "Blocks until the navigation completes. ") NavigateToURL;
  AutomationMsg_NavigationResponseValues NavigateToURL(const GURL& url);
  %feature("docstring", "Navigates to a given GURL. Blocks until the given "
           "number of navigations complete.")
      NavigateToURLBlockUntilNavigationsComplete;
  AutomationMsg_NavigationResponseValues
      NavigateToURLBlockUntilNavigationsComplete(
          const GURL& url, int number_of_navigations);
  %feature("docstring", "Equivalent to hitting the Back button. "
           "Blocks until navigation completes.") GoBack;
  AutomationMsg_NavigationResponseValues GoBack();
  %feature("docstring", "Equivalent to hitting the Forward button. "
           "Blocks until navigation completes.") GoForward;
  AutomationMsg_NavigationResponseValues GoForward();
  %feature("docstring", "Equivalent to hitting the Reload button. "
           "Blocks until navigation completes.") Reload;
  AutomationMsg_NavigationResponseValues Reload();
  %feature("docstring", "Closes the tab. Supply True to wait "
           "until the tab has closed, else it'll wait only until the call "
           "has been initiated. Be careful while closing the last tab "
           "since it might close the browser.") Close;
  bool Close();
  bool Close(bool wait_until_closed);
  %feature("docstring", "Blocks until tab is completely restored.")
      WaitForTabToBeRestored;
  bool WaitForTabToBeRestored(uint32 timeout_ms);

  // HTTP Auth
  %feature("docstring",
      "Checks if this tab has a login prompt waiting for auth.  This will be "
      "true if a navigation results in a login prompt, and if an attempted "
      "login fails. "
      "Note that this is only valid if you've done a navigation on this same "
      "object; different TabProxy objects can refer to the same Tab.  Calls "
      "that can set this are NavigateToURL, GoBack, and GoForward. ")
      NeedsAuth;
  bool NeedsAuth() const;
  %feature("docstring", "Supply authentication to a login prompt. "
           "Blocks until navigation completes or another login prompt appears "
           "in the case of failed auth.") SetAuth;
  bool SetAuth(const std::wstring& username, const std::wstring& password);
  %feature("docstring", "Cancel authentication to a login prompt. ")
      CancelAuth;
  bool CancelAuth();

};

class PyUITestSuiteBase {
 public:
  %feature("docstring", "Create the suite.") PyUITestSuiteBase;
  PyUITestSuiteBase(int argc, char** argv);
  ~PyUITestSuiteBase();

  %feature("docstring", "Initialize from the path to browser dir.") Initialize;
  void Initialize(const FilePath& browser_dir);
};

class PyUITestBase {
 public:
  PyUITestBase(bool clear_profile, std::wstring homepage);

  %feature("docstring", "Initialize the entire setup. Should be called "
           "before launching the browser. For internal use.") Initialize;
  void Initialize(const FilePath& browser_dir);

  %feature("docstring",
           "Fires up the browser and opens a window.") SetUp;
  virtual void SetUp();
  %feature("docstring",
           "Closes all windows and destroys the browser.") TearDown;
  virtual void TearDown();

  %feature("docstring", "Timeout (in milli secs) for an action. "
           "This value is also used as default for the WaitUntil() method.")
      action_max_timeout_ms;
  int action_max_timeout_ms() const;

  %feature("docstring", "Get the timeout (in milli secs) for an automation "
           "call") command_execution_timeout_ms;
  int command_execution_timeout_ms() const;
  %feature("docstring", "Set the timeout (in milli secs) for an automation "
           "call.  This is an internal method.  Do not use this directly.  "
           "Use CmdExecutionTimeoutChanger instead")
      set_command_execution_timeout_ms;
  void set_command_execution_timeout_ms(int timeout);

  %feature("docstring", "Launches the browser and IPC testing server.")
      LaunchBrowserAndServer;
  void LaunchBrowserAndServer();
  %feature("docstring", "Closes the browser and IPC testing server.")
      CloseBrowserAndServer;
  void CloseBrowserAndServer();

  %feature("docstring", "Determine if the profile is set to be cleared on "
                        "next startup.") get_clear_profile;
  bool get_clear_profile() const;
  %feature("docstring", "If False, sets the flag so that the profile is "
           "not cleared on next startup. Useful for persisting profile "
           "across restarts. By default the state is True, to clear profile.")
      set_clear_profile;
  void set_clear_profile(bool clear_profile);

  // Navigation Methods
  %feature("docstring", "Navigate to the given url in the given tab and given "
           "window (or active tab in first window if indexes not given). "
           "Note that this method also activates the corresponding tab/window "
           "if it's not active already. Blocks until page has loaded.")
      NavigateToURL;
  void NavigateToURL(const char* url_string);
  void NavigateToURL(const char* url_string, int window_index);
  void NavigateToURL(const char* url_string, int window_index, int tab_index);

  %feature("docstring", "Reload the active tab in the given window (or first "
           "window if index not given). Blocks until page has reloaded.")
      ReloadActiveTab;
  void ReloadActiveTab(int window_index = 0);

  // BrowserProxy methods
  %feature("docstring", "Apply the accelerator with given id "
           "(IDC_BACK, IDC_NEWTAB ...) to the given or first window. "
           "The list can be found at chrome/app/chrome_dll_resource.h. "
           "Note that this method just schedules the accelerator, but does "
           "not wait for it to actually finish doing anything."
           "Returns True on success.")
      ApplyAccelerator;
  bool ApplyAccelerator(int id, int window_index=0);
  %feature("docstring", "Like ApplyAccelerator, except that it waits for "
           "the command to execute.") RunCommand;
  bool RunCommand(int browser_command, int window_index = 0);

  // Get/fetch properties
  %feature("docstring",
           "Get the path to download directory.") GetDownloadDirectory;
  FilePath GetDownloadDirectory();

  %feature("docstring", "Get the path to profile directory.") user_data_dir;
  FilePath user_data_dir() const;

  %feature("docstring", "Set download shelf visibility for the given or "
           "first browser window.") SetDownloadShelfVisible;
  void SetDownloadShelfVisible(bool is_visible, int window_index=0);

  %feature("docstring", "Determine if the download shelf is visible in the "
           "given or first browser window.") IsDownloadShelfVisible;
  bool IsDownloadShelfVisible(int window_index=0);

  %feature("docstring", "Determine if the bookmark bar is visible. "
           "If the NTP is visible, only return true if attached "
           "(to the chrome).") GetBookmarkBarVisibility;
  bool GetBookmarkBarVisibility();

  %feature("docstring", "Wait for the bookmark bar animation to complete. "
           "|wait_for_open| specifies which kind of change we wait for.")
      WaitForBookmarkBarVisibilityChange;
  bool WaitForBookmarkBarVisibilityChange(bool wait_for_open);

  %feature("docstring", "Get the bookmarks as a JSON string.  Internal method.")
      _GetBookmarksAsJSON;
  std::string _GetBookmarksAsJSON();

  %feature("docstring", "Add a bookmark folder with the given index in the parent."
                        "  |title| is the title/name of the folder.") AddBookmarkGroup;
  bool AddBookmarkGroup(std::wstring parent_id, int index, std::wstring title);

  %feature("docstring", "Add a bookmark with the given title and URL.") AddBookmarkURL;
  bool AddBookmarkURL(std::wstring parent_id, int index,
                      std::wstring title, const std::wstring url);

  %feature("docstring", "Move a bookmark to a new parent.") ReparentBookmark;
  bool ReparentBookmark(std::wstring id, std::wstring new_parent_id, int index);

  %feature("docstring", "Set the title of a bookmark.") SetBookmarkTitle;
  bool SetBookmarkTitle(std::wstring id, std::wstring title);

  %feature("docstring", "Set the URL of a bookmark.") SetBookmarkURL;
  bool SetBookmarkURL(std::wstring id, const std::wstring url);

  %feature("docstring", "Remove (delete) a bookmark.") RemoveBookmark;
  bool RemoveBookmark(std::wstring id);

  %feature("docstring", "Open the Find box in the given or first browser "
           "window.") OpenFindInPage;
  void OpenFindInPage(int window_index=0);

  %feature("docstring", "Determine if the find box is visible in the "
           "given or first browser window.") IsFindInPageVisible;
  bool IsFindInPageVisible(int window_index=0);

  // Tabs and windows methods
  %feature("docstring", "Open a new browser window.") OpenNewBrowserWindow;
  bool OpenNewBrowserWindow(bool show);

  %feature("docstring", "Close a browser window.") CloseBrowserWindow;
  bool CloseBrowserWindow(int window_index);

  %feature("docstring", "Fetch the number of browser windows. Includes popups.")
      GetBrowserWindowCount;
  int GetBrowserWindowCount();

  %feature("docstring", "Get the index of the active tab in the given or "
           "first window. Indexes are zero-based.") GetActiveTabIndex;
  int GetActiveTabIndex(int window_index=0);
  %feature("docstring", "Activate the tab at the given zero-based index in "
           "the given or first window. Returns True on success.") ActivateTab;
  bool ActivateTab(int tab_index, int window_index=0);

  %feature("docstring", "Get the title of the active tab for the given or "
           "first window.") GetActiveTabTitle;
  std::wstring GetActiveTabTitle(int window_index=0);

  %feature("docstring", "Get the URL for the active tab. for the given or "
           "first window. Returns an instance of GURL") GetActiveTabURL;
  GURL GetActiveTabURL(int window_index=0);

  %feature("docstring", "Count of the number of tabs in the given or "
           "first window.") GetTabCount;
  int GetTabCount(int window_index=0);
  %feature("docstring", "Create a new tab at the end of given or first browser "
           "window and activate it. Blocks until the page is loaded. "
           "Returns True on success.") AppendTab;
  bool AppendTab(const GURL& tab_url, int window_index=0);

  %feature("docstring", "Set the value of the cookie at cookie_url to value "
           "for the given window index and tab index. "
           "Returns True on success.") SetCookie;
  bool SetCookie(const GURL& cookie_url, const std::string& value,
                 int window_index=0, int tab_index=0);

  %feature("docstring", "Get the value of the cokie at cookie_url for the "
           "given window index and tab index. "
           "Returns empty string on error or if there is no value for the "
           "cookie.") GetCookieVal;
  std::string GetCookie(const GURL& cookie_url, int window_index=0,
                        int tab_index=0);

  // Misc methods
  %feature("docstring", "Determine if the browser is running. "
           "Returns False if user closed the window or if the browser died")
      IsBrowserRunning;
  bool IsBrowserRunning();

  %feature("docstring", "Install an extension from the given file. Returns "
           "True if successfully installed and loaded.") InstallExtension;
  bool InstallExtension(const FilePath& crx_file, bool with_ui);

  %feature("docstring", "Get a proxy to the browser window at the given "
                        "zero-based index.") GetBrowserWindow;
  scoped_refptr<BrowserProxy> GetBrowserWindow(int window_index);

  // Meta-method
  %feature("docstring", "Send a sync JSON request to Chrome.  "
                        "Returns a JSON dict as a response.  "
                        "Internal method.")
      _SendJSONRequest;
  std::string _SendJSONRequest(int window_index, std::string request);

  %feature("docstring", "Execute a string of javascript in the specified "
           "(window, tab, frame) and return a string.") ExecuteJavascript;
  std::wstring ExecuteJavascript(const std::wstring& script,
                                 int window_index=0,
                                 int tab_index=0,
                                 const std::wstring& frame_xpath="");

  %feature("docstring", "Evaluate a javascript expression in the specified "
           "(window, tab, frame) and return the specified DOM value "
           "as a string. This is a wrapper around "
           "window.domAutomationController.send().") GetDOMValue;
  std::wstring GetDOMValue(const std::wstring& expr,
                           int window_index=0,
                           int tab_index=0,
                           const std::wstring& frame_xpath="");

  %feature("docstring", "Resets to the default theme. "
           "Returns true on success.") ResetToDefaultTheme;
  bool ResetToDefaultTheme();

};

