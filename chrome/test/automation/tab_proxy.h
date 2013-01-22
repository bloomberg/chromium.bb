// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_TAB_PROXY_H_
#define CHROME_TEST_AUTOMATION_TAB_PROXY_H_

#include "build/build_config.h"  // NOLINT

#if defined(OS_WIN)
#include <wtypes.h>  // NOLINT
#endif

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "chrome/common/automation_constants.h"
#include "chrome/test/automation/automation_handle_tracker.h"
#include "content/public/browser/save_page_type.h"
#include "content/public/common/page_type.h"
#include "content/public/common/security_style.h"
#include "net/base/cert_status_flags.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/window_open_disposition.h"

class BrowserProxy;
class FilePath;
class GURL;
namespace IPC {
class Message;
}

namespace base {
class Value;
}

class TabProxy : public AutomationResourceProxy {
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
  bool GetTabTitle(std::wstring* title) const WARN_UNUSED_RESULT;

  // Gets the tabstrip index of the tab.
  bool GetTabIndex(int* index) const WARN_UNUSED_RESULT;

  // Executes a javascript in a frame's context whose xpath is provided as the
  // first parameter and extract the values from the resulting json string.
  // Examples:
  // jscript = "window.domAutomationController.send('string');"
  // will result in value = "string"
  // jscript = "window.domAutomationController.send(24);"
  // will result in value = 24
  // NOTE: If this is called from a ui test, |dom_automation_enabled_| must be
  // set to true for these functions to work.
  bool ExecuteAndExtractString(const std::wstring& frame_xpath,
                               const std::wstring& jscript,
                               std::wstring* value) WARN_UNUSED_RESULT;
  bool ExecuteAndExtractBool(const std::wstring& frame_xpath,
                             const std::wstring& jscript,
                             bool* value) WARN_UNUSED_RESULT;
  bool ExecuteAndExtractInt(const std::wstring& frame_xpath,
                            const std::wstring& jscript,
                            int* value) WARN_UNUSED_RESULT;

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

  // Sends off an asynchronous request for printing.
  bool PrintAsync() WARN_UNUSED_RESULT;

  // Waits until the infobar count is |count|.
  // Returns true on success.
  bool WaitForInfoBarCount(size_t count) WARN_UNUSED_RESULT;

  // Uses the specified encoding to override encoding of the page in the tab.
  bool OverrideEncoding(const std::string& encoding) WARN_UNUSED_RESULT;

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

  // Simulates a key press. |key| is the virtual key code of the key pressed.
  void SimulateKeyPress(ui::KeyboardCode key);

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

  // Called when tracking the first object. Used for reference counting
  // purposes.
  void FirstObjectAdded();

  // Called when no longer tracking any objects. Used for reference counting
  // purposes.
  void LastObjectRemoved();

  // Caller takes ownership over returned value.  Returns NULL on failure.
  base::Value* ExecuteAndExtractValue(
      const std::wstring& frame_xpath,
      const std::wstring& jscript) WARN_UNUSED_RESULT;

 private:
  base::Lock list_lock_;  // Protects the observers_list_.
  ObserverList<TabProxyDelegate> observers_list_;
  DISALLOW_COPY_AND_ASSIGN(TabProxy);
};

#endif  // CHROME_TEST_AUTOMATION_TAB_PROXY_H_
