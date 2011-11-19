// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_UI_TEST_UTILS_H_
#define CHROME_TEST_BASE_UI_TEST_UTILS_H_
#pragma once

#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/string16.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/test/automation/dom_element_proxy.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/gfx/native_widget_types.h"
#include "webkit/glue/window_open_disposition.h"

class AppModalDialog;
class BookmarkModel;
class Browser;
class CommandLine;
class ExtensionAction;
class FilePath;
class GURL;
class MessageLoop;
class Profile;
class RenderViewHost;
class RenderWidgetHost;
class ScopedTempDir;
class SkBitmap;
class TabContents;
class TabContentsWrapper;
class TemplateURLService;

namespace browser {
struct NavigateParams;
}

namespace gfx {
class Size;
}

// A collections of functions designed for use with InProcessBrowserTest.
namespace ui_test_utils {

// Flags to indicate what to wait for in a navigation test.
// They can be ORed together.
// The order in which the waits happen when more than one is selected, is:
//    Browser
//    Tab
//    Navigation
enum BrowserTestWaitFlags {
  BROWSER_TEST_NONE = 0,                      // Don't wait for anything.
  BROWSER_TEST_WAIT_FOR_BROWSER = 1 << 0,     // Wait for a new browser.
  BROWSER_TEST_WAIT_FOR_TAB = 1 << 1,         // Wait for a new tab.
  BROWSER_TEST_WAIT_FOR_NAVIGATION = 1 << 2,  // Wait for navigation to finish.

  BROWSER_TEST_MASK = BROWSER_TEST_WAIT_FOR_BROWSER |
                      BROWSER_TEST_WAIT_FOR_TAB |
                      BROWSER_TEST_WAIT_FOR_NAVIGATION
};

// Turns on nestable tasks, runs the message loop, then resets nestable tasks
// to what they were originally. Prefer this over MessageLoop::Run for in
// process browser tests that need to block until a condition is met.
void RunMessageLoop();

// Turns on nestable tasks, runs all pending tasks in the message loop,
// then resets nestable tasks to what they were originally. Prefer this
// over MessageLoop::RunAllPending for in process browser tests to run
// all pending tasks.
void RunAllPendingInMessageLoop();

// Puts the current tab title in |title|. Returns true on success.
bool GetCurrentTabTitle(const Browser* browser, string16* title);

// Waits for a new tab to be added to |browser|. TODO(gbillock): remove this
// race hazard. Use WindowedNotificationObserver instead.
void WaitForNewTab(Browser* browser);

// Waits for a |browser_action| to be updated. TODO(gbillock): remove this race
// hazard. Use WindowedNotificationObserver instead.
void WaitForBrowserActionUpdated(ExtensionAction* browser_action);

// Waits for a load stop for the specified |tab|'s controller, if the tab is
// currently loading.  Otherwise returns immediately.
void WaitForLoadStop(TabContents* tab);

// Waits for a new browser to be created, returning the browser.
Browser* WaitForNewBrowser();

// Opens |url| in an incognito browser window with the incognito profile of
// |profile|, blocking until the navigation finishes. This will create a new
// browser if a browser with the incognito profile does not exist.
void OpenURLOffTheRecord(Profile* profile, const GURL& url);

// Performs the provided navigation process, blocking until the navigation
// finishes. May change the params in some cases (i.e. if the navigation
// opens a new browser window). Uses browser::Navigate.
void NavigateToURL(browser::NavigateParams* params);

// Navigates the selected tab of |browser| to |url|, blocking until the
// navigation finishes. Uses Browser::OpenURL --> browser::Navigate.
void NavigateToURL(Browser* browser, const GURL& url);

// Navigates the specified tab of |browser| to |url|, blocking until the
// navigation finishes.
// |disposition| indicates what tab the navigation occurs in, and
// |browser_test_flags| controls what to wait for before continuing.
void NavigateToURLWithDisposition(Browser* browser,
                                  const GURL& url,
                                  WindowOpenDisposition disposition,
                                  int browser_test_flags);

// Navigates the selected tab of |browser| to |url|, blocking until the
// number of navigations specified complete.
void NavigateToURLBlockUntilNavigationsComplete(Browser* browser,
                                                const GURL& url,
                                                int number_of_navigations);

// Gets the DOMDocument for the active tab in |browser|.
// Returns a NULL reference on failure.
DOMElementProxyRef GetActiveDOMDocument(Browser* browser);

// Executes the passed |script| in the frame pointed to by |frame_xpath| (use
// empty string for main frame).  The |script| should not invoke
// domAutomationController.send(); otherwise, your test will hang or be flaky.
// If you want to extract a result, use one of the below functions.
// Returns true on success.
bool ExecuteJavaScript(RenderViewHost* render_view_host,
                       const std::wstring& frame_xpath,
                       const std::wstring& script) WARN_UNUSED_RESULT;

// The following methods executes the passed |script| in the frame pointed to by
// |frame_xpath| (use empty string for main frame) and sets |result| to the
// value returned by the script evaluation.
// They return true on success, false if the script evaluation failed or did not
// evaluate to the expected type.
// Note: In order for the domAutomationController to work, you must call
// EnableDOMAutomation() in your test first.
bool ExecuteJavaScriptAndExtractInt(RenderViewHost* render_view_host,
                                    const std::wstring& frame_xpath,
                                    const std::wstring& script,
                                    int* result) WARN_UNUSED_RESULT;
bool ExecuteJavaScriptAndExtractBool(RenderViewHost* render_view_host,
                                     const std::wstring& frame_xpath,
                                     const std::wstring& script,
                                     bool* result) WARN_UNUSED_RESULT;
bool ExecuteJavaScriptAndExtractString(RenderViewHost* render_view_host,
                                       const std::wstring& frame_xpath,
                                       const std::wstring& script,
                                       std::string* result) WARN_UNUSED_RESULT;

// Generate the file path for testing a particular test.
// The file for the tests is all located in
// test_root_directory/dir/<file>
// The returned path is FilePath format.
FilePath GetTestFilePath(const FilePath& dir, const FilePath& file);

// Generate the URL for testing a particular test.
// HTML for the tests is all located in
// test_root_directory/dir/<file>
// The returned path is GURL format.
GURL GetTestUrl(const FilePath& dir, const FilePath& file);

// Generate a URL for a file path including a query string.
GURL GetFileUrlWithQuery(const FilePath& path, const std::string& query_string);

// Blocks until an application modal dialog is showns and returns it.
AppModalDialog* WaitForAppModalDialog();

// Causes the specified tab to crash. Blocks until it is crashed.
void CrashTab(TabContents* tab);

// Waits for the focus to change in the specified tab.
void WaitForFocusChange(TabContents* tab_contents);

// Waits for the renderer to return focus to the browser (happens through tab
// traversal).
void WaitForFocusInBrowser(Browser* browser);

// Performs a find in the page of the specified tab. Returns the number of
// matches found.  |ordinal| is an optional parameter which is set to the index
// of the current match.
int FindInPage(TabContentsWrapper* tab,
               const string16& search_string,
               bool forward,
               bool case_sensitive,
               int* ordinal);

// Returns true if the View is focused.
bool IsViewFocused(const Browser* browser, ViewID vid);

// Simulates a mouse click on a View in the browser.
void ClickOnView(const Browser* browser, ViewID vid);

// Register |observer| for the given |type| and |source| and run
// the message loop until the observer posts a quit task.
void RegisterAndWait(content::NotificationObserver* observer,
                     int type,
                     const content::NotificationSource& source);

// Blocks until |model| finishes loading.
void WaitForBookmarkModelToLoad(BookmarkModel* model);

// Blocks until |service| finishes loading.
void WaitForTemplateURLServiceToLoad(TemplateURLService* service);

// Blocks until the |browser|'s history finishes loading.
void WaitForHistoryToLoad(Browser* browser);

// Puts the native window for |browser| in |native_window|. Returns true on
// success.
bool GetNativeWindow(const Browser* browser, gfx::NativeWindow* native_window)
    WARN_UNUSED_RESULT;

// Brings the native window for |browser| to the foreground. Returns true on
// success.
bool BringBrowserWindowToFront(const Browser* browser) WARN_UNUSED_RESULT;

// Gets the first browser that is not in the specified set.
Browser* GetBrowserNotInSet(std::set<Browser*> excluded_browsers);

// Sends a key press, blocking until the key press is received or the test times
// out. This uses ui_controls::SendKeyPress, see it for details. Returns true
// if the event was successfully sent and received.
bool SendKeyPressSync(const Browser* browser,
                      ui::KeyboardCode key,
                      bool control,
                      bool shift,
                      bool alt,
                      bool command) WARN_UNUSED_RESULT;

// Sends a key press, blocking until both the key press and a notification from
// |source| of type |type| are received, or until the test times out. This uses
// ui_controls::SendKeyPress, see it for details. Returns true if the event was
// successfully sent and both the event and notification were received.
bool SendKeyPressAndWait(const Browser* browser,
                         ui::KeyboardCode key,
                         bool control,
                         bool shift,
                         bool alt,
                         bool command,
                         int type,
                         const content::NotificationSource& source)
                             WARN_UNUSED_RESULT;

// Run a message loop only for the specified amount of time.
class TimedMessageLoopRunner {
 public:
  // Create new MessageLoopForUI and attach to it.
  TimedMessageLoopRunner();

  // Attach to an existing message loop.
  explicit TimedMessageLoopRunner(MessageLoop* loop)
      : loop_(loop), owned_(false), quit_loop_invoked_(false) {}

  ~TimedMessageLoopRunner();

  // Run the message loop for ms milliseconds.
  void RunFor(int ms);

  // Post Quit task to the message loop.
  void Quit();

  // Post delayed Quit task to the message loop.
  void QuitAfter(int ms);

  bool WasTimedOut() const {
    return !quit_loop_invoked_;
  }

 private:
  MessageLoop* loop_;
  bool owned_;
  bool quit_loop_invoked_;

  DISALLOW_COPY_AND_ASSIGN(TimedMessageLoopRunner);
};

// This is a utility class for running a python websocket server
// during tests. The server is started during the construction of the
// object, and is stopped when the destructor is called. Note that
// because of the underlying script that is used:
//
//    third_paty/WebKit/Tools/Scripts/new-run-webkit-websocketserver
//
// Only *_wsh.py handlers found under "http/tests/websocket/tests" from the
// |root_directory| will be found and active while running the test
// server.
class TestWebSocketServer {
 public:
  TestWebSocketServer();

  // Stops the python websocket server if it was already started.
  ~TestWebSocketServer();

  // Starts the python websocket server using |root_directory|. Returns whether
  // the server was successfully started.
  bool Start(const FilePath& root_directory);

 private:
  // Sets up PYTHONPATH to run websocket_server.py.
  void SetPythonPath();

  // Creates a CommandLine for invoking the python interpreter.
  CommandLine* CreatePythonCommandLine();

  // Creates a CommandLine for invoking the python websocker server.
  CommandLine* CreateWebSocketServerCommandLine();

  // Has the server been started?
  bool started_;

  // A Scoped temporary directory for holding the python pid file.
  ScopedTempDir temp_dir_;

  // Used to close the same python interpreter when server falls out
  // scope.
  FilePath websocket_pid_file_;

  DISALLOW_COPY_AND_ASSIGN(TestWebSocketServer);
};

// A notification observer which quits the message loop when a notification
// is received. It also records the source and details of the notification.
class TestNotificationObserver : public content::NotificationObserver {
 public:
  TestNotificationObserver();
  virtual ~TestNotificationObserver();

  const content::NotificationSource& source() const {
    return source_;
  }

  const content::NotificationDetails& details() const {
    return details_;
  }

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  content::NotificationSource source_;
  content::NotificationDetails details_;
};

// A WindowedNotificationObserver allows code to watch for a notification
// over a window of time. Typically testing code will need to do something
// like this:
//   PerformAction()
//   WaitForCompletionNotification()
// This leads to flakiness as there's a window between PerformAction returning
// and the observers getting registered, where a notification will be missed.
//
// Rather, one can do this:
//   WindowedNotificationObserver signal(...)
//   PerformAction()
//   signal.Wait()
class WindowedNotificationObserver : public content::NotificationObserver {
 public:
  // Register to listen for notifications of the given type from either a
  // specific source, or from all sources if |source| is
  // NotificationService::AllSources().
  WindowedNotificationObserver(int notification_type,
                               const content::NotificationSource& source);
  virtual ~WindowedNotificationObserver();

  // Wait until the specified notification occurs.  If the notification was
  // emitted between the construction of this object and this call then it
  // returns immediately.
  void Wait();

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  bool seen_;
  bool running_;
  std::set<uintptr_t> sources_seen_;
  content::NotificationSource waiting_for_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(WindowedNotificationObserver);
};

// Similar to WindowedNotificationObserver but also provides a way of retrieving
// the details associated with the notification.
// Note that in order to use that class the details class should be copiable,
// which is the case with most notifications.
template <class U>
class WindowedNotificationObserverWithDetails
    : public WindowedNotificationObserver {
 public:
  WindowedNotificationObserverWithDetails(
      int notification_type,
      const content::NotificationSource& source)
      : WindowedNotificationObserver(notification_type, source) {}

  // Fills |details| with the details of the notification received for |source|.
  bool GetDetailsFor(uintptr_t source, U* details) {
    typename std::map<uintptr_t, U>::const_iterator iter =
        details_.find(source);
    if (iter == details_.end())
      return false;
    *details = iter->second;
    return true;
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
    const U* details_ptr = content::Details<U>(details).ptr();
    if (details_ptr)
      details_[source.map_key()] = *details_ptr;
    WindowedNotificationObserver::Observe(type, source, details);
  }

 private:
  std::map<uintptr_t, U> details_;

  DISALLOW_COPY_AND_ASSIGN(WindowedNotificationObserverWithDetails);
};

// Watches title changes on a tab, blocking until an expected title is set.
class TitleWatcher : public content::NotificationObserver {
 public:
  // |tab_contents| must be non-NULL and needs to stay alive for the
  // entire lifetime of |this|. |expected_title| is the title that |this|
  // will wait for.
  TitleWatcher(TabContents* tab_contents, const string16& expected_title);
  virtual ~TitleWatcher();

  // Adds another title to watch for.
  void AlsoWaitForTitle(const string16& expected_title);

  // Waits until the title matches either expected_title or one of the titles
  // added with  AlsoWaitForTitle.  Returns the value of the most recently
  // observed matching title.
  const string16& WaitAndGetTitle() WARN_UNUSED_RESULT;

 private:
  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  TabContents* tab_contents_;
  std::vector<string16> expected_titles_;
  content::NotificationRegistrar notification_registrar_;

  // The most recently observed expected title, if any.
  string16 observed_title_;

  bool expected_title_observed_;
  bool quit_loop_on_observation_;

  DISALLOW_COPY_AND_ASSIGN(TitleWatcher);
};

// See SendKeyPressAndWait.  This function additionally performs a check on the
// NotificationDetails using the provided Details<U>.
template <class U>
bool SendKeyPressAndWaitWithDetails(
    const Browser* browser,
    ui::KeyboardCode key,
    bool control,
    bool shift,
    bool alt,
    bool command,
    int type,
    const content::NotificationSource& source,
    const content::Details<U>& details) WARN_UNUSED_RESULT;

template <class U>
bool SendKeyPressAndWaitWithDetails(
    const Browser* browser,
    ui::KeyboardCode key,
    bool control,
    bool shift,
    bool alt,
    bool command,
    int type,
    const content::NotificationSource& source,
    const content::Details<U>& details) {
  WindowedNotificationObserverWithDetails<U> observer(type, source);

  if (!SendKeyPressSync(browser, key, control, shift, alt, command))
    return false;

  observer.Wait();

  U my_details;
  if (!observer.GetDetailsFor(source.map_key(), &my_details))
    return false;

  return *details.ptr() == my_details && !testing::Test::HasFatalFailure();
}

// Hide a native window.
void HideNativeWindow(gfx::NativeWindow window);

// Show and focus a native window. Returns true on success.
bool ShowAndFocusNativeWindow(gfx::NativeWindow window) WARN_UNUSED_RESULT;

// Watches for responses from the DOMAutomationController and keeps them in a
// queue. Useful for waiting for a message to be received.
class DOMMessageQueue : public content::NotificationObserver {
 public:
  // Constructs a DOMMessageQueue and begins listening for messages from the
  // DOMAutomationController. Do not construct this until the browser has
  // started.
  DOMMessageQueue();
  virtual ~DOMMessageQueue();

  // Wait for the next message to arrive. |message| will be set to the next
  // message, if not null. Returns true on success.
  bool WaitForMessage(std::string* message) WARN_UNUSED_RESULT;

  // Overridden content::NotificationObserver methods.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  content::NotificationRegistrar registrar_;
  std::queue<std::string> message_queue_;
  bool waiting_for_message_;

  DISALLOW_COPY_AND_ASSIGN(DOMMessageQueue);
};

// Takes a snapshot of the given render widget, rendered at |page_size|. The
// snapshot is set to |bitmap|. Returns true on success.
bool TakeRenderWidgetSnapshot(RenderWidgetHost* rwh,
                              const gfx::Size& page_size,
                              SkBitmap* bitmap) WARN_UNUSED_RESULT;

// Takes a snapshot of the entire page, according to the width and height
// properties of the DOM's document. Returns true on success. DOMAutomation
// must be enabled.
bool TakeEntirePageSnapshot(RenderViewHost* rvh,
                            SkBitmap* bitmap) WARN_UNUSED_RESULT;

}  // namespace ui_test_utils

#endif  // CHROME_TEST_BASE_UI_TEST_UTILS_H_
