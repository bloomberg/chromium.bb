// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_TAB_PROXY_H_
#define CHROME_TEST_AUTOMATION_TAB_PROXY_H_
#pragma once

#include "build/build_config.h"  // NOLINT

#if defined(OS_WIN)
#include <wtypes.h>  // NOLINT
#endif

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "chrome/browser/download/save_package.h"
#include "chrome/common/automation_constants.h"
#include "chrome/common/page_type.h"
#include "chrome/common/security_style.h"
#include "chrome/test/automation/automation_handle_tracker.h"
#include "chrome/test/automation/dom_element_proxy.h"
#include "chrome/test/automation/javascript_execution_controller.h"
#include "webkit/glue/window_open_disposition.h"

class GURL;
class Value;
namespace IPC {
class Message;
};

enum FindInPageDirection { BACK = 0, FWD = 1 };
enum FindInPageCase { IGNORE_CASE = 0, CASE_SENSITIVE = 1 };
// Specifies the font size on a page which is requested by an automation
// client.
enum AutomationPageFontSize {
  SMALLEST_FONT = 8,
  SMALL_FONT = 12,
  MEDIUM_FONT = 16,
  LARGE_FONT = 24,
  LARGEST_FONT = 36
};

class TabProxy : public AutomationResourceProxy,
                 public JavaScriptExecutionController {
 public:
  class TabProxyDelegate {
   public:
    virtual bool OnMessageReceived(TabProxy* tab, const IPC::Message& msg) {
      return false;
    }
    virtual void OnChannelError(TabProxy* tab) {}

   protected:
    virtual ~TabProxyDelegate() {}
  };

  TabProxy(AutomationMessageSender* sender,
           AutomationHandleTracker* tracker,
           int handle);

  // Gets the current url of the tab.
  bool GetCurrentURL(GURL* url) const WARN_UNUSED_RESULT;

  // Gets the title of the tab.
  bool GetTabTitle(string16* title) const WARN_UNUSED_RESULT;

  // Gets the tabstrip index of the tab.
  bool GetTabIndex(int* index) const WARN_UNUSED_RESULT;

  // Gets the number of constrained window for this tab.
  bool GetConstrainedWindowCount(int* count) const WARN_UNUSED_RESULT;

  // Executes a javascript in a frame's context whose xpath is provided as the
  // first parameter and extract the values from the resulting json string.
  // Examples:
  // jscript = "window.domAutomationController.send('string');"
  // will result in value = "string"
  // jscript = "window.domAutomationController.send(24);"
  // will result in value = 24
  // NOTE: If this is called from a ui test, |dom_automation_enabled_| must be
  // set to true for these functions to work.
  bool ExecuteAndExtractString(const string16& frame_xpath,
                               const string16& jscript,
                               string16* value) WARN_UNUSED_RESULT;
  bool ExecuteAndExtractBool(const string16& frame_xpath,
                             const string16& jscript,
                             bool* value) WARN_UNUSED_RESULT;
  bool ExecuteAndExtractInt(const string16& frame_xpath,
                            const string16& jscript,
                            int* value) WARN_UNUSED_RESULT;
  bool ExecuteAndExtractValue(const string16& frame_xpath,
                              const string16& jscript,
                              Value** value) WARN_UNUSED_RESULT;

  // Returns a DOMElementProxyRef to the tab's current DOM document.
  // This proxy is invalidated when the document changes.
  DOMElementProxyRef GetDOMDocument();

  // Configure extension automation mode. When extension automation
  // mode is turned on, the automation host can overtake extension API calls
  // e.g. to make UI tests for extensions easier to write.  Returns true if
  // the message is successfully sent.
  //
  // Note that API calls in _any_ extension view will be routed to the current
  // tab.  This is to enable UI testing of e.g. extension background pages.
  //
  // Enabling extension automation from more than one tab is an error.
  //
  // You must disable extension automation before destroying the tab.
  //
  // The parameter can take the following types of values:
  // a) An empty list to turn off extension automation.
  // b) A list with one item, "*", to turn extension automation on for all
  //    functions.
  // c) A list with one or more items which are the names of Chrome Extension
  //    API functions that should be forwarded over the automation interface.
  //    Other functions will continue to be fulfilled as normal. This lets you
  //    write tests where some functionality continues to function as normal,
  //    and other functionality is mocked out by the test.
  bool SetEnableExtensionAutomation(
    const std::vector<std::string>& functions_enabled) WARN_UNUSED_RESULT;

  // Navigates to a url. This method accepts the same kinds of URL input that
  // can be passed to Chrome on the command line. This is a synchronous call and
  // hence blocks until the navigation completes.
  AutomationMsg_NavigationResponseValues NavigateToURL(
      const GURL& url) WARN_UNUSED_RESULT;

  // Navigates to a url. This method accepts the same kinds of URL input that
  // can be passed to Chrome on the command line. This is a synchronous call and
  // hence blocks until the |number_of_navigations| navigations complete.
  AutomationMsg_NavigationResponseValues
      NavigateToURLBlockUntilNavigationsComplete(
          const GURL& url, int number_of_navigations) WARN_UNUSED_RESULT;


  // Navigates to a url. This is an asynchronous version of NavigateToURL.
  // The function returns immediately after sending the LoadURL notification
  // to the browser.
  // TODO(vibhor): Add a callback if needed in future.
  // TODO(mpcomplete): If the navigation results in an auth challenge, the
  // TabProxy we attach won't know about it.  See bug 666730.
  bool NavigateToURLAsync(const GURL& url) WARN_UNUSED_RESULT;

  // Asynchronously navigates to a url using a non-default disposition.
  // This can be used for example to open a URL in a new tab.
  bool NavigateToURLAsyncWithDisposition(
      const GURL& url,
      WindowOpenDisposition disposition) WARN_UNUSED_RESULT;

  // Replaces a vector contents with the redirect chain out of the given URL.
  // Returns true on success. Failure may be due to being unable to send the
  // message, parse the response, or a failure of the history system in the
  // browser.
  bool GetRedirectsFrom(const GURL& source_url,
                        std::vector<GURL>* redirects) WARN_UNUSED_RESULT;

  // Equivalent to hitting the Back button. This is a synchronous call and
  // hence blocks until the navigation completes.
  AutomationMsg_NavigationResponseValues GoBack() WARN_UNUSED_RESULT;

  // Equivalent to hitting the Back button. This is a synchronous call and
  // hence blocks until the |number_of_navigations| navigations complete.
  AutomationMsg_NavigationResponseValues GoBackBlockUntilNavigationsComplete(
      int number_of_navigations) WARN_UNUSED_RESULT;

  // Equivalent to hitting the Forward button. This is a synchronous call and
  // hence blocks until the navigation completes.
  AutomationMsg_NavigationResponseValues GoForward() WARN_UNUSED_RESULT;

  // Equivalent to hitting the Forward button. This is a synchronous call and
  // hence blocks until the |number_of_navigations| navigations complete.
  AutomationMsg_NavigationResponseValues GoForwardBlockUntilNavigationsComplete(
      int number_of_navigations) WARN_UNUSED_RESULT;

  // Equivalent to hitting the Reload button. This is a synchronous call and
  // hence blocks until the navigation completes.
  AutomationMsg_NavigationResponseValues Reload() WARN_UNUSED_RESULT;

  // Closes the tab. This is synchronous, but does NOT block until the tab has
  // closed, rather it blocks until the browser has initiated the close. Use
  // Close(true) if you need to block until tab completely closes.
  //
  // Note that this proxy is invalid after this call.
  bool Close() WARN_UNUSED_RESULT;

  // Variant of close that allows you to specify whether you want to block
  // until the tab has completely closed (wait_until_closed == true) or block
  // until the browser has initiated the close (wait_until_closed = false).
  //
  // When a tab is closed the browser does additional work via invoke later
  // and may wait for messages from the renderer. Supplying a value of true to
  // this method waits until all processing is done. Be careful with this,
  // when closing the last tab it is possible for the browser to shutdown BEFORE
  // the tab has completely closed. In other words, this may NOT be sent for
  // the last tab.
  bool Close(bool wait_until_closed) WARN_UNUSED_RESULT;

  // Gets the process ID that corresponds to the content area of this tab.
  // Returns true if the call was successful.  If the specified tab has no
  // separate process for rendering its content, the return value is true but
  // the process_id is 0.
  bool GetProcessID(int* process_id) const WARN_UNUSED_RESULT;

  // Supply or cancel authentication to a login prompt.  These are synchronous
  // calls and hence block until the load finishes (or another login prompt
  // appears, in the case of invalid login info).
  bool SetAuth(const string16& username,
               const string16& password) WARN_UNUSED_RESULT;
  bool CancelAuth() WARN_UNUSED_RESULT;

  // Checks if this tab has a login prompt waiting for auth.  This will be
  // true if a navigation results in a login prompt, and if an attempted login
  // fails.
  // Note that this is only valid if you've done a navigation on this same
  // object; different TabProxy objects can refer to the same Tab.  Calls
  // that can set this are NavigateToURL, GoBack, and GoForward.
  // TODO(mpcomplete): we have no way of knowing if auth is needed after either
  // NavigateToURLAsync, or after appending a tab with an URL that triggers
  // auth.
  bool NeedsAuth() const WARN_UNUSED_RESULT;

  // Starts a search within the current tab. The parameter |search_string|
  // specifies what string to search for, |forward| specifies whether to search
  // in forward direction, and |match_case| specifies case sensitivity
  // (true=case sensitive). |find_next| specifies whether this is a new search
  // or a continuation of the old one. |ordinal| is an optional parameter that
  // returns the ordinal of the active match (also known as "the 7" part of
  // "7 of 9"). A return value of -1 indicates failure.
  int FindInPage(const std::wstring& search_string, FindInPageDirection forward,
                 FindInPageCase match_case, bool find_next, int* ordinal);

  bool GetCookies(const GURL& url, std::string* cookies) WARN_UNUSED_RESULT;
  bool GetCookieByName(const GURL& url,
                       const std::string& name,
                       std::string* cookies) WARN_UNUSED_RESULT;
  bool SetCookie(const GURL& url, const std::string& value) WARN_UNUSED_RESULT;
  bool DeleteCookie(const GURL& url,
                    const std::string& name) WARN_UNUSED_RESULT;

  // Opens the collected cookies dialog for the current tab. This function can
  // be invoked on any valid tab.
  bool ShowCollectedCookiesDialog() WARN_UNUSED_RESULT;

  // Sends a InspectElement message for the current tab. |x| and |y| are the
  // coordinates that we want to simulate that the user is trying to inspect.
  int InspectElement(int x, int y);

  // Block the thread until the constrained(child) window count changes.
  // First parameter is the original child window count
  // The second parameter is updated with the number of new child windows.
  // The third parameter specifies the timeout length for the wait loop.
  // Returns false if the count does not change.
  bool WaitForChildWindowCountToChange(int count, int* new_count,
      int wait_timeout) WARN_UNUSED_RESULT;

  // Gets the number of popups blocked from this tab.
  bool GetBlockedPopupCount(int* count) const WARN_UNUSED_RESULT;

  // Blocks the thread until the number of blocked popup is equal to
  // |target_count|.
  bool WaitForBlockedPopupCountToChangeTo(int target_count,
                                          int wait_timeout) WARN_UNUSED_RESULT;

  bool GetDownloadDirectory(FilePath* download_directory) WARN_UNUSED_RESULT;

  // Shows an interstitial page.  Blocks until the interstitial page
  // has been loaded. Return false if a failure happens.
  bool ShowInterstitialPage(const std::string& html_text) WARN_UNUSED_RESULT;

  // Hides the currently shown interstitial page. Blocks until the interstitial
  // page has been hidden. Return false if a failure happens.
  bool HideInterstitialPage() WARN_UNUSED_RESULT;

#if defined(OS_WIN)
  // The functions in this block are for external tabs, hence Windows only.

  // The container of an externally hosted tab calls this to reflect any
  // accelerator keys that it did not process. This gives the tab a chance
  // to handle the keys
  bool ProcessUnhandledAccelerator(const MSG& msg) WARN_UNUSED_RESULT;

  // Ask the tab to set focus to either the first or last element on the page.
  // When the restore_focus_to_view parameter is true, the render view
  // associated with the current tab is informed that it is receiving focus.
  // For external tabs only.
  bool SetInitialFocus(bool reverse, bool restore_focus_to_view)
      WARN_UNUSED_RESULT;

  // Navigates to a url in an externally hosted tab.
  // This method accepts the same kinds of URL input that
  // can be passed to Chrome on the command line. This is a synchronous call and
  // hence blocks until the navigation completes.
  AutomationMsg_NavigationResponseValues NavigateInExternalTab(
      const GURL& url, const GURL& referrer) WARN_UNUSED_RESULT;

  AutomationMsg_NavigationResponseValues NavigateExternalTabAtIndex(
      int index) WARN_UNUSED_RESULT;

  // Posts a message to the external tab.
  void HandleMessageFromExternalHost(const std::string& message,
                                     const std::string& origin,
                                     const std::string& target);
#endif  // defined(OS_WIN)

  // Waits for the tab to finish being restored. Returns true on success.
  // timeout_ms gives the max amount of time to wait for restore to complete.
  bool WaitForTabToBeRestored(uint32 timeout_ms) WARN_UNUSED_RESULT;

  // Retrieves the different security states for the current tab.
  bool GetSecurityState(SecurityStyle* security_style,
                        int* ssl_cert_status,
                        int* insecure_content_status) WARN_UNUSED_RESULT;

  // Returns the type of the page currently showing (normal, interstitial,
  // error).
  bool GetPageType(PageType* page_type) WARN_UNUSED_RESULT;

  // Simulates the user action on the SSL blocking page.  if |proceed| is true,
  // this is equivalent to clicking the 'Proceed' button, if false to 'Take me
  // out of there' button.
  bool TakeActionOnSSLBlockingPage(bool proceed) WARN_UNUSED_RESULT;

  // Prints the current page without user intervention.
  bool PrintNow() WARN_UNUSED_RESULT;

  // Sends off an asynchronous request for printing.
  bool PrintAsync() WARN_UNUSED_RESULT;

  // Save the current web page. |file_name| is the HTML file name, and
  // |dir_path| is the directory for saving resource files. |type| indicates
  // which type we're saving as: HTML only or the complete web page.
  bool SavePage(const FilePath& file_name, const FilePath& dir_path,
                SavePackage::SavePackageType type) WARN_UNUSED_RESULT;

  // Retrieves the number of info-bars currently showing in |count|.
  bool GetInfoBarCount(size_t* count) WARN_UNUSED_RESULT;

  // Waits until the infobar count is |count|.
  // Returns true on success.
  bool WaitForInfoBarCount(size_t count) WARN_UNUSED_RESULT;

  // Causes a click on the "accept" button of the info-bar at |info_bar_index|.
  // If |wait_for_navigation| is true, this call does not return until a
  // navigation has occurred.
  bool ClickInfoBarAccept(size_t info_bar_index,
                          bool wait_for_navigation) WARN_UNUSED_RESULT;

  // Retrieves the time at which the last navigation occurred.  This is intended
  // to be used with WaitForNavigation (see below).
  bool GetLastNavigationTime(int64* last_navigation_time) WARN_UNUSED_RESULT;

  // Waits for a new navigation if none as occurred since |last_navigation_time|
  // The purpose of this function is for operations that causes asynchronous
  // navigation to happen.
  // It is supposed to be used as follow:
  // int64 last_nav_time;
  // tab_proxy->GetLastNavigationTime(&last_nav_time);
  // tab_proxy->SomeOperationThatTriggersAnAsynchronousNavigation();
  // tab_proxy->WaitForNavigation(last_nav_time);
  bool WaitForNavigation(int64 last_navigation_time) WARN_UNUSED_RESULT;

  // Gets the current used encoding of the page in the tab.
  bool GetPageCurrentEncoding(std::string* encoding) WARN_UNUSED_RESULT;

  // Uses the specified encoding to override encoding of the page in the tab.
  bool OverrideEncoding(const std::string& encoding) WARN_UNUSED_RESULT;

  // Loads all blocked plug-ins on the page.
  bool LoadBlockedPlugins() WARN_UNUSED_RESULT;

  // Captures the entire page and saves as a PNG at the given path. Returns
  // true on success.
  bool CaptureEntirePageAsPNG(const FilePath& path) WARN_UNUSED_RESULT;

#if defined(OS_WIN)
  // Resizes the tab window.
  // The parent_window parameter allows a parent to be specified for the window
  // passed in.
  void Reposition(HWND window, HWND window_insert_after, int left, int top,
                  int width, int height, int flags, HWND parent_window);

  // Sends the selected context menu command to the chrome instance
  void SendContextMenuCommand(int selected_command);

  // To be called when the window hosting the tab has moved.
  void OnHostMoved();
#endif  // defined(OS_WIN)

  // Selects all contents on the page.
  void SelectAll();

  // Edit operations on the page.
  void Cut();
  void Copy();
  void Paste();

  // These handlers issue asynchronous Reload, Stop and SaveAs notifications to
  // the chrome instance.
  void ReloadAsync();
  void StopAsync();
  void SaveAsAsync();

  // Notify the JavaScript engine in the render to change its parameters
  // while performing stress testing. See
  // |ViewHostMsg_JavaScriptStressTestControl_Commands| in render_messages.h
  // for information on the arguments.
  void JavaScriptStressTestControl(int cmd, int param);

  // Calls delegates
  void AddObserver(TabProxyDelegate* observer);
  void RemoveObserver(TabProxyDelegate* observer);
  bool OnMessageReceived(const IPC::Message& message);
  void OnChannelError();
 protected:
  virtual ~TabProxy();

  // Override JavaScriptExecutionController methods.
  // Executes |script| and gets the response JSON. Returns true on success.
  bool ExecuteJavaScriptAndGetJSON(const std::string& script,
                                   std::string* json) WARN_UNUSED_RESULT;

  // Called when tracking the first object. Used for reference counting
  // purposes.
  void FirstObjectAdded();

  // Called when no longer tracking any objects. Used for reference counting
  // purposes.
  void LastObjectRemoved();

 private:
  base::Lock list_lock_;  // Protects the observers_list_.
  ObserverList<TabProxyDelegate> observers_list_;
  DISALLOW_COPY_AND_ASSIGN(TabProxy);
};

#endif  // CHROME_TEST_AUTOMATION_TAB_PROXY_H_
