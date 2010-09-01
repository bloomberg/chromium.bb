// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_UI_TEST_UTILS_H_
#define CHROME_TEST_UI_TEST_UTILS_H_
#pragma once

#include <map>
#include <string>
#include <set>

#include "base/basictypes.h"
#include "base/keyboard_codes.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/string16.h"
#include "chrome/browser/view_ids.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/notification_service.h"
#include "chrome/test/automation/dom_element_proxy.h"
#include "gfx/native_widget_types.h"

class AppModalDialog;
class BookmarkModel;
class BookmarkNode;
class Browser;
class CommandLine;
class DownloadManager;
class ExtensionAction;
class FilePath;
class GURL;
class MessageLoop;
class NavigationController;
class Profile;
class RenderViewHost;
class ScopedTempDir;
class TabContents;
class Value;

// A collections of functions designed for use with InProcessBrowserTest.
namespace ui_test_utils {

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

// Waits for the current tab to complete the navigation. Returns true on
// success.
bool WaitForNavigationInCurrentTab(Browser* browser);

// Waits for the current tab to complete the specified number of navigations.
// Returns true on success.
bool WaitForNavigationsInCurrentTab(Browser* browser,
                                    int number_of_navigations);

// Waits for |controller| to complete a navigation. This blocks until
// the navigation finishes.
void WaitForNavigation(NavigationController* controller);

// Waits for |controller| to complete a navigation. This blocks until
// the specified number of navigations complete.
void WaitForNavigations(NavigationController* controller,
                        int number_of_navigations);

// Waits for a new tab to be added to |browser|.
void WaitForNewTab(Browser* browser);

// Waits for a |browser_action| to be updated.
void WaitForBrowserActionUpdated(ExtensionAction* browser_action);

// Waits for a load stop for the specified |controller|.
void WaitForLoadStop(NavigationController* controller);

// Waits for a new browser to be created, returning the browser.
Browser* WaitForNewBrowser();

// Opens |url| in an incognito browser window with the off the record profile of
// |profile|, blocking until the navigation finishes. This will create a new
// browser if a browser with the off the record profile does not exist.
void OpenURLOffTheRecord(Profile* profile, const GURL& url);

// Navigates the selected tab of |browser| to |url|, blocking until the
// navigation finishes.
void NavigateToURL(Browser* browser, const GURL& url);

// Navigates the selected tab of |browser| to |url|, blocking until the
// number of navigations specified complete.
void NavigateToURLBlockUntilNavigationsComplete(Browser* browser,
                                                const GURL& url,
                                                int number_of_navigations);

// Gets the DOMDocument for the active tab in |browser|.
// Returns a NULL reference on failure.
DOMElementProxyRef GetActiveDOMDocument(Browser* browser);

// Executes the passed |script| in the frame pointed to by |frame_xpath| (use
// empty string for main frame) and returns the value the evaluation of the
// script returned.  The caller owns the returned value.
Value* ExecuteJavaScript(RenderViewHost* render_view_host,
                         const std::wstring& frame_xpath,
                         const std::wstring& script);

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
                                    int* result);
bool ExecuteJavaScriptAndExtractBool(RenderViewHost* render_view_host,
                                     const std::wstring& frame_xpath,
                                     const std::wstring& script,
                                     bool* result);
bool ExecuteJavaScriptAndExtractString(RenderViewHost* render_view_host,
                                       const std::wstring& frame_xpath,
                                       const std::wstring& script,
                                       std::string* result);

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

// Creates an observer that waits for |download_manager| to report that it
// has a total of |count| downloads that have been handles
void WaitForDownloadCount(DownloadManager* download_manager, size_t count);

// Blocks until an application modal dialog is showns and returns it.
AppModalDialog* WaitForAppModalDialog();

// Causes the specified tab to crash. Blocks until it is crashed.
void CrashTab(TabContents* tab);

// Waits for the focus to change in the specified RenderViewHost.
void WaitForFocusChange(RenderViewHost* rvh);

// Waits for the renderer to return focus to the browser (happens through tab
// traversal).
void WaitForFocusInBrowser(Browser* browser);

// Performs a find in the page of the specified tab. Returns the number of
// matches found.  |ordinal| is an optional parameter which is set to the index
// of the current match.
int FindInPage(TabContents* tab,
               const string16& search_string,
               bool forward,
               bool case_sensitive,
               int* ordinal);

// Returns true if the View is focused.
bool IsViewFocused(const Browser* browser, ViewID vid);

// Simulates a mouse click on a View in the browser.
void ClickOnView(const Browser* browser, ViewID vid);

// Blocks until a notification for given |type| is received.
void WaitForNotification(NotificationType::Type type);

// Register |observer| for the given |type| and |source| and run
// the message loop until the observer posts a quit task.
void RegisterAndWait(NotificationObserver* observer,
                     NotificationType::Type type,
                     const NotificationSource& source);

// Blocks until |model| finishes loading.
void WaitForBookmarkModelToLoad(BookmarkModel* model);

// Sends a key press blocking until the key press is received or the test times
// out. This uses ui_controls::SendKeyPress, see it for details. Returns true
// if the event was successfully sent and received.
bool SendKeyPressSync(gfx::NativeWindow window,
                      base::KeyboardCode key,
                      bool control,
                      bool shift,
                      bool alt,
                      bool command) WARN_UNUSED_RESULT;

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
//    third_paty/WebKit/WebKitTools/Scripts/new-run-webkit-websocketserver
//
// Only *_wsh.py handlers found under "websocket/tests" from the
// |root_directory| will be found and active while running the test
// server.
class TestWebSocketServer {
 public:
  // Creates and starts a python websocket server with |root_directory|.
  explicit TestWebSocketServer(const FilePath& root_directory);

  // Destroys and stops the server.
  ~TestWebSocketServer();

 private:
  // Sets up PYTHONPATH to run websocket_server.py.
  void SetPythonPath();

  // Creates a CommandLine for invoking the python interpreter.
  CommandLine* CreatePythonCommandLine();

  // Creates a CommandLine for invoking the python websocker server.
  CommandLine* CreateWebSocketServerCommandLine();

  // A Scoped temporary directory for holding the python pid file.
  ScopedTempDir temp_dir_;

  // Used to close the same python interpreter when server falls out
  // scope.
  FilePath websocket_pid_file_;

  DISALLOW_COPY_AND_ASSIGN(TestWebSocketServer);
};

// A notification observer which quits the message loop when a notification
// is received. It also records the source and details of the notification.
class TestNotificationObserver : public NotificationObserver {
 public:
  TestNotificationObserver() : source_(NotificationService::AllSources()) {
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    source_ = source;
    details_ = details;
    MessageLoopForUI::current()->Quit();
  }

  const NotificationSource& source() const {
    return source_;
  }

  const NotificationDetails& details() const {
    return details_;
  }

 private:
  NotificationSource source_;
  NotificationDetails details_;
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
//   WindowedNotificationObserver<type> signal(...)
//   PerformAction()
//   wait_for_signal.Wait()
template <class T>
class WindowedNotificationObserver : public NotificationObserver {
 public:
  /* Register to listen for notifications of the given type from either
   * a specific source, of from all sources if |source| is NULL */
  WindowedNotificationObserver(NotificationType notification_type,
                               T* source)
      : seen_(false),
        running_(false),
        waiting_for_(source) {
    if (source) {
      registrar_.Add(this, notification_type, waiting_for_);
    } else {
      registrar_.Add(this, notification_type,
                     NotificationService::AllSources());
    }
  }

  /* Wait sleeps until the specified notification occurs. You must have
   * specified a source in the arguments to the constructor in order to
   * use this function. Otherwise, you should use WaitFor. */
  void Wait() {
    if (!waiting_for_.ptr()) {
      LOG(FATAL) << "Wait called when monitoring all sources. You must use "
                 << "WaitFor in this case.";
    }

    if (seen_)
      return;

    running_ = true;
    ui_test_utils::RunMessageLoop();
  }

  /* WaitFor waits until the given notification type is received from the
   * given object. If the notification was emitted between the construction of
   * this object and this call then it returns immediately.
   *
   * Beware that this is inheriently plagued by ABA issues. Consider:
   *   WindowedNotificationObserver is created with NULL source
   *   Object A is created with address x and fires a notification
   *   Object A is freed
   *   Object B is created with the same address
   *   WaitFor is called with the address of B
   *
   * In this case, WaitFor will return immediately because of the
   * notification from A (because they shared an address), despite being
   * different objects.
   */
  void WaitFor(T* source) {
    if (waiting_for_.ptr()) {
      LOG(FATAL) << "WaitFor called when already waiting on a specific "
                 << "source. Use Wait in this case.";
    }

    waiting_for_ = Source<T>(source);
    if (sources_seen_.count(waiting_for_.map_key()) > 0)
      return;

    running_ = true;
    ui_test_utils::RunMessageLoop();
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (waiting_for_ == source) {
      seen_ = true;
      if (running_)
        MessageLoopForUI::current()->Quit();
    } else {
      sources_seen_.insert(source.map_key());
    }
  }

 private:
  bool seen_;
  bool running_;
  std::set<uintptr_t> sources_seen_;
  Source<T> waiting_for_;
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(WindowedNotificationObserver);
};

// Similar to WindowedNotificationObserver but also provides a way of retrieving
// the details associated with the notification.
// Note that in order to use that class the details class should be copiable,
// which is the case with most notifications.
template <class T, class U>
class WindowedNotificationObserverWithDetails
    : public WindowedNotificationObserver<T> {
 public:
  WindowedNotificationObserverWithDetails(NotificationType notification_type,
                                          T* source)
      : WindowedNotificationObserver<T>(notification_type, source) {}

  // Fills |details| with the details of the notification received for |source|.
  bool GetDetailsFor(T* source, U* details) {
    typename std::map<T*, U>::const_iterator iter = details_.find(source);
    if (iter == details_.end())
      return false;
    *details = iter->second;
    return true;
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    details_[Source<T>(source).ptr()] = *Details<U>(details).ptr();
    WindowedNotificationObserver<T>::Observe(type, source, details);
  }

 private:
  std::map<T*, U> details_;

  DISALLOW_COPY_AND_ASSIGN(WindowedNotificationObserverWithDetails);
};

// Hide a native window.
void HideNativeWindow(gfx::NativeWindow window);

// Show and focus a native window.
void ShowAndFocusNativeWindow(gfx::NativeWindow window);

}  // namespace ui_test_utils

#endif  // CHROME_TEST_UI_TEST_UTILS_H_
