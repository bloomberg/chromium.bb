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
#endif  // OS_WIN
  const StringType& value() const;
  %feature("docstring", "Construct an empty FilePath or from a string.")
      FilePath;
  FilePath();
  explicit FilePath(const StringType& path);
};

class PyUITestSuite {
 public:
  PyUITestSuite(int argc, char** argv);

  %feature("docstring",
           "Fires up the browser and opens a window.") SetUp;
  virtual void SetUp();
  %feature("docstring",
           "Closes all windows and destroys the browser.") TearDown;
  virtual void TearDown();

  %feature("docstring", "Navigate to the given url in the active tab. "
           "Blocks until page has loaded.") NavigateToURL;
  void NavigateToURL(const char* url_string);

  // BrowserProxy methods
  %feature("docstring", "Set download shelf visibility.") SetShelfVisible;
  void SetShelfVisible(bool is_visible);
  %feature("docstring", "Create a new tab at the end of given or first browser "
           "window and activate it. Blocks until the page is loaded. "
           "Returns True on success.") AppendTab;
  bool AppendTab(const GURL& tab_url, int window_index=0);
  %feature("docstring", "Activate the tab at the given zero-based index in "
           "the given or first window. Returns True on success.") ActivateTab;
  bool ActivateTab(int tab_index, int window_index=0);
  %feature("docstring", "Apply the accelerator with given id "
           "(IDC_BACK, IDC_NEWTAB ...) to the given or first window. "
           "The list can be found at chrome/app/chrome_dll_resource.h. "
           "Returns True on success.")
      ApplyAccelerator;
  bool ApplyAccelerator(int id, int window_index=0);

  // TabProxy methods
  %feature("docstring",
           "Determine if the download shelf is visible.") IsShelfVisible;
  bool IsShelfVisible();
  %feature("docstring", "Open the Find box.") OpenFindInPage;
  void OpenFindInPage();
  %feature("docstring",
           "Determine if the find box is visible.") IsFindInPageVisible;
  bool IsFindInPageVisible();
  %feature("docstring",
           "Get the path to download directory.") GetDownloadDirectory;
  std::string GetDownloadDirectory();

  // AutomationProxy methods
  %feature("docstring", "Open a new browser window.") OpenNewBrowserWindow;
  bool OpenNewBrowserWindow(bool show);
  %feature("docstring", "Install an extension from the given file. Returns "
           "True if successfully installed and loaded.") InstallExtension;
  bool InstallExtension(const FilePath& crx_file);

  // UITestBase methods
  %feature("docstring", "Path to download directory.") user_data_dir;
  FilePath user_data_dir() const;
  %feature("docstring", "Get the index of the active tab.") GetActiveTabIndex;
  int GetActiveTabIndex();
  %feature("docstring", "Get the title of the active tab.") GetActiveTabTitle;
  std::wstring GetActiveTabTitle();
  %feature("docstring", "Get the URL for the active tab. "
           "Returns an instance of GURL") GetActiveTabURL;
  GURL GetActiveTabURL();
  %feature("docstring", "Determine if the browser is running. "
           "Returns False if user closed the window or if the browser died")
      IsBrowserRunning;
  bool IsBrowserRunning();
  %feature("docstring",
           "Count of the number of tabs in the front window.") GetTabCount;
  int GetTabCount();
};

