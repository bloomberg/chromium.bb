// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_UI_TEST_UTILS_H_
#define CHROME_TEST_UI_TEST_UTILS_H_

#include <string>

#include "base/basictypes.h"
#include "base/string16.h"
#include "chrome/browser/view_ids.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_type.h"

class AppModalDialog;
class Browser;
class DownloadManager;
class GURL;
class MessageLoop;
class NavigationController;
class Profile;
class RenderViewHost;
class TabContents;
class Value;

// A collections of functions designed for use with InProcessBrowserTest.
namespace ui_test_utils {

// Turns on nestable tasks, runs the message loop, then resets nestable tasks
// to what they were originally. Prefer this over MessageLoop::Run for in
// process browser tests that need to block until a condition is met.
void RunMessageLoop();

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

// Waits for a load stop for the specified |controller|.
void WaitForLoadStop(NavigationController* controller);

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

GURL GetTestUrl(const std::wstring& dir, const std::wstring file);

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

// Waits for the language of the page to have been detected and returns it.
// This should be called right after a navigation notification was received.
std::string WaitForLanguageDetection(TabContents* tab_contents);

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

// Register |observer| for the given |type| and run the message loop until
// either the observer posts a quit task or we timeout.
void RegisterAndWait(NotificationType::Type type,
                     NotificationObserver* observer,
                     int64 timeout_ms);

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

}  // namespace ui_test_utils

#endif  // CHROME_TEST_UI_TEST_UTILS_H_
