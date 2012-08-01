// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

%module(docstring="Python interface to Automation Proxy.") pyautolib
%feature("autodoc", "1");

%include <cpointer.i>
%include <std_string.i>
%include <std_wstring.i>

%include "chrome/test/pyautolib/argc_argv.i"

// NOTE: All files included in this file should also be listed under
//       pyautolib_sources in chrome_tests.gypi.

// Headers that can be swigged directly.
%include "chrome/app/chrome_command_ids.h"
%include "chrome/app/chrome_dll_resource.h"
%include "chrome/common/automation_constants.h"
%include "chrome/common/pref_names.h"
%include "content/public/common/page_type.h"
%include "content/public/common/security_style.h"
// Must come before cert_status_flags.h
%include "net/base/net_export.h"
%ignore net::MapNetErrorToCertStatus(int);
%include "net/base/cert_status_flags.h"

%{
#include "chrome/common/automation_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/pyautolib/pyautolib.h"
#include "net/test/test_server.h"
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
  %feature("docstring", "Navigates to a given GURL asynchronously. "
           "Does not wait for the navigation to complete, or even begin; "
           "Use NavigateToURL() if you want to wait.") NavigateToURLAsync;
  bool TabProxy::NavigateToURLAsync(const GURL& url);
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
  %feature("docstring", "Simulates user action on the SSL blocking page."
          "if |proceed| is true, this is equivalent to clicking the 'Proceed' "
          "button, if false to 'Take me out of there' button.")
      TakeActionOnSSLBlockingPage;
  bool TakeActionOnSSLBlockingPage(bool proceed);
  %extend {
    %feature("docstring", "Retrieves the different security states for the "
             "current tab.")
        GetSecurityState;
    PyObject* GetSecurityState() {
      content::SecurityStyle security_style;
      net::CertStatus ssl_cert_status;
      int insecure_content_status;
      PyObject* result_dict = PyDict_New();
      if ($self->GetSecurityState(
          &security_style, &ssl_cert_status, &insecure_content_status)) {
        PyDict_SetItem(result_dict, PyString_FromString("security_style"),
                       PyInt_FromLong(security_style));
        PyDict_SetItem(result_dict, PyString_FromString("ssl_cert_status"),
                       PyInt_FromLong(ssl_cert_status));
        PyDict_SetItem(result_dict,
                       PyString_FromString("insecure_content_status"),
                       PyInt_FromLong(insecure_content_status));
      }
      return result_dict;
    }
  };
  %extend {
    %feature("docstring", "Returns the type of page currently showing "
             "(normal, interstitial, error.")
        GetPageType;
    PyObject* GetPageType() {
      content::PageType page_type;
      PyObject* result_dict = PyDict_New();
      if ($self->GetPageType(&page_type)) {
        PyDict_SetItem(result_dict, PyString_FromString("page_type"),
                       PyInt_FromLong(page_type));
      }
      return result_dict;
    }
  };

};

class PyUITestSuiteBase {
 public:
  %feature("docstring", "Create the suite.") PyUITestSuiteBase;
  PyUITestSuiteBase(int argc, char** argv);
  virtual ~PyUITestSuiteBase();

  %feature("docstring", "Initialize from the path to browser dir.")
      InitializeWithPath;
  void InitializeWithPath(const FilePath& browser_dir);
  %feature("docstring", "Set chrome source root path, used in some tests")
      SetCrSourceRoot;
  void SetCrSourceRoot(const FilePath& path);
};

class PyUITestBase {
 public:
  PyUITestBase(bool clear_profile, std::wstring homepage);

  %feature("docstring", "Initialize the entire setup. Should be called "
           "before launching the browser. For internal use.") Initialize;
  void Initialize(const FilePath& browser_dir);

  %feature("docstring", "Appends a command-line switch (with associated value "
           "if given) to the list of switches to be passed to the browser "
           "upon launch. Should be called before launching the browser. "
           "For internal use only.")
      AppendBrowserLaunchSwitch;
  void AppendBrowserLaunchSwitch(const char* name);
  void AppendBrowserLaunchSwitch(const char* name, const char* value);

  %feature("docstring", "Begins tracing with the given category string.")
      BeginTracing;
  bool BeginTracing(const std::string& categories);

  %feature("docstring", "Ends tracing and returns the collected events.")
      EndTracing;
  std::string EndTracing();

  void UseNamedChannelID(const std::string& named_channel_id);

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
           "call") action_timeout_ms;
  int action_timeout_ms();

  %feature("docstring", "Set the timeout (in milli secs) for an automation "
           "call.  This is an internal method.  Do not use this directly.  "
           "Use ActionTimeoutChanger instead")
      set_action_timeout_ms;
  void set_action_timeout_ms(int timeout);

  %feature("docstring", "Timeout (in milli secs) for large tests.")
      large_test_timeout_ms;
  int large_test_timeout_ms() const;

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

  %feature("docstring", "Get the path to profile directory.") user_data_dir;
  FilePath user_data_dir() const;

  %feature("docstring", "Determine if the bookmark bar is visible. "
           "If the NTP is visible, only return true if attached "
           "(to the chrome).") GetBookmarkBarVisibility;
  bool GetBookmarkBarVisibility();

  %feature("docstring", "Determine if the bookmark bar is detached. "
           "This usually is only true on the NTP.") IsBookmarkBarDetached;
  bool IsBookmarkBarDetached();

  %feature("docstring", "Wait for the bookmark bar animation to complete. "
           "|wait_for_open| specifies which kind of change we wait for.")
      WaitForBookmarkBarVisibilityChange;
  bool WaitForBookmarkBarVisibilityChange(bool wait_for_open,
                                          int window_index=0);

  %feature("docstring", "Get the bookmarks as a JSON string. Internal method.")
      _GetBookmarksAsJSON;
  std::string _GetBookmarksAsJSON(int window_index=0);

  %feature("docstring", "Add a bookmark folder with the given index in the "
                        " parent. |title| is the title/name of the folder.")
      AddBookmarkGroup;
  bool AddBookmarkGroup(std::wstring parent_id,
                        int index, std::wstring title,
                        int window_index=0);

  %feature("docstring", "Add a bookmark with the given title and URL.")
      AddBookmarkURL;
  bool AddBookmarkURL(std::wstring parent_id,
                      int index,
                      std::wstring title,
                      const std::wstring url,
                      int window_index=0);

  %feature("docstring", "Move a bookmark to a new parent.") ReparentBookmark;
  bool ReparentBookmark(std::wstring id,
                        std::wstring new_parent_id,
                        int index,
                        int window_index=0);

  %feature("docstring", "Set the title of a bookmark.") SetBookmarkTitle;
  bool SetBookmarkTitle(std::wstring id,
                        std::wstring title,
                        int window_index=0);

  %feature("docstring", "Set the URL of a bookmark.") SetBookmarkURL;
  bool SetBookmarkURL(std::wstring id,
                      const std::wstring url,
                      int window_index=0);

  %feature("docstring", "Remove (delete) a bookmark.") RemoveBookmark;
  bool RemoveBookmark(std::wstring id, int window_index=0);

  // Misc methods
  %feature("docstring", "Get a proxy to the browser window at the given "
                        "zero-based index.") GetBrowserWindow;
  scoped_refptr<BrowserProxy> GetBrowserWindow(int window_index);

  // Meta-method
  %feature("docstring", "Send a sync JSON request to Chrome.  "
                        "Returns a JSON dict as a response.  "
                        "Given timeout in milliseconds."
                        "Internal method.")
      _SendJSONRequest;
  std::string _SendJSONRequest(int window_index,
                               const std::string& request,
                               int timeout);

  %feature("docstring",
           "Returns empty string if there were no unexpected Chrome asserts or "
           "crashes, a string describing the failures otherwise. As a side "
           "effect, it will fail with EXPECT_EQ macros if this code runs "
           "within a gtest harness.") GetErrorsAndCrashes;
  std::string CheckErrorsAndCrashes() const;
};

namespace net {
// TestServer
%feature("docstring",
         "TestServer. Serves files in data dir over a local http server")
    TestServer;
class TestServer {
 public:
  enum Type {
    TYPE_FTP,
    TYPE_HTTP,
    TYPE_HTTPS,
    TYPE_SYNC,
  };

  // Initialize a TestServer listening on the specified host (IP or hostname).
  TestServer(Type type, const std::string& host, const FilePath& document_root);
  // Initialize a HTTPS TestServer with a specific set of HTTPSOptions.
  TestServer(const HTTPSOptions& https_options,
             const FilePath& document_root);

  %feature("docstring", "Start TestServer over an ephemeral port") Start;
  bool Start();

  %feature("docstring", "Stop TestServer") Stop;
  bool Stop();

  %feature("docstring", "Get FilePath to the document root") document_root;
  const FilePath& document_root() const;

  std::string GetScheme() const;

  %feature("docstring", "Get URL for a file path") GetURL;
  GURL GetURL(const std::string& path) const;
};

%extend TestServer {
  %feature("docstring", "Get port number.") GetPort;
  int GetPort() const {
    int val = 0;
    $self->server_data().GetInteger("port", &val);
    return val;
  }

  %feature("docstring", "Get xmpp port number in case of sync server.")
      GetSyncXmppPort;
  int GetSyncXmppPort() const {
    int val = 0;
    $self->server_data().GetInteger("xmpp_port", &val);
    return val;
  }
};

}
// HTTPSOptions
%feature("docstring",
         "HTTPSOptions. Sets one of three types of a cert")
    HTTPSOptions;
struct HTTPSOptions {
  enum ServerCertificate {
    CERT_OK,
    CERT_MISMATCHED_NAME,
    CERT_EXPIRED,
  };

  // Initialize a new HTTPSOptions that will use the specified certificate.
  explicit HTTPSOptions(ServerCertificate cert);
};

%{
typedef net::TestServer::HTTPSOptions HTTPSOptions;
%}

%pointer_class(int, int_ptr);
%pointer_class(uint32, uint32_ptr);
