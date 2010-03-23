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

%include "std_string.i"
%include "std_wstring.i"

%{
#include "chrome/test/pyautolib/pyautolib.h"
%}

// Make functions using (int argc, char** argv) usable as (sys.argv) from python
%typemap(in) (int argc, char **argv) {
  int i;
  if (!PyList_Check($input)) {
    PyErr_SetString(PyExc_ValueError, "Expecting a list");
    return NULL;
  }
  $1 = PyList_Size($input);
  $2 = (char **) malloc(($1+1)*sizeof(char *));
  for (i = 0; i < $1; i++) {
    PyObject *s = PyList_GetItem($input,i);
    if (!PyString_Check(s)) {
        free($2);
        PyErr_SetString(PyExc_ValueError, "List items must be strings");
        return NULL;
    }
    $2[i] = PyString_AsString(s);
  }
  $2[i] = 0;
}

%typemap(freearg) (int argc, char **argv) {
   if ($2) free($2);
}

%include "chrome/app/chrome_dll_resource.h"

%feature("docstring", "Represent a URL. Call spec() to get the string.") GURL;
class GURL {
 public:
  GURL();
  explicit GURL(const std::string& url_string);
  %feature("docstring", "Get the string representation.") spec;
  const std::string& spec() const;
};

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
  %feature("docstring", "Construct an empty FilePath or from a string.")
      FilePath;
  FilePath();
  explicit FilePath(const StringType& path);
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

  // Navigation Methods
  %feature("docstring", "Navigate to the given url in the given tab and given "
           "window (or active tab in first window if indexes not given). "
           "Note that this method also activates the corresponding tab/window "
           "if it's not active already. Blocks until page has loaded.")
      NavigateToURL;
  void NavigateToURL(const char* url_string);
  void NavigateToURL(const char* url_string, int window_index, int tab_index);

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

  // Misc methods
  %feature("docstring", "Determine if the browser is running. "
           "Returns False if user closed the window or if the browser died")
      IsBrowserRunning;
  bool IsBrowserRunning();

  %feature("docstring", "Install an extension from the given file. Returns "
           "True if successfully installed and loaded.") InstallExtension;
  bool InstallExtension(const FilePath& crx_file);
};

