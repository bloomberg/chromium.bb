// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_AUTOMATION_PROXY_H_
#define CHROME_TEST_AUTOMATION_AUTOMATION_PROXY_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/process_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/automation_constants.h"
#include "chrome/test/automation/automation_handle_tracker.h"
#include "chrome/test/automation/browser_proxy.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_sender.h"
#include "ipc/ipc_sync_channel.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/native_widget_types.h"

class BrowserProxy;
class TabProxy;
class WindowProxy;
struct ExternalTabSettings;

// This is an interface that AutomationProxy-related objects can use to
// access the message-sending abilities of the Proxy.
class AutomationMessageSender : public IPC::Sender {
 public:
  // Sends a message synchronously; it doesn't return until a response has been
  // received or a timeout has expired.
  //
  // The function returns true if a response is received, and returns false if
  // there is a failure or timeout (in milliseconds).
  //
  // NOTE: When timeout occurs, the connection between proxy provider may be
  //       in transit state. Specifically, there might be pending IPC messages,
  //       and the proxy provider might be still working on the previous
  //       request.
  virtual bool Send(IPC::Message* message) = 0;
  virtual bool Send(IPC::Message* message, int timeout_ms) = 0;
};

// This is the interface that external processes can use to interact with
// a running instance of the app.
class AutomationProxy : public IPC::Listener, public AutomationMessageSender {
 public:
  AutomationProxy(base::TimeDelta action_timeout, bool disconnect_on_failure);
  virtual ~AutomationProxy();

  // Creates a previously unused channel id.
  static std::string GenerateChannelID();

  // Initializes a channel for a connection to an AutomationProvider.
  // If use_named_interface is false, it will act as a client
  // and connect to the named IPC socket with channel_id as its path.
  // If use_named_interface is true, it will act as a server and
  // use an anonymous socketpair instead.
  void InitializeChannel(const std::string& channel_id,
                         bool use_named_interface);

  // IPC callback
  virtual bool OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelError();

  // Close the automation IPC channel.
  void Disconnect();

  // Waits for the app to launch and the automation provider to say hello
  // (the app isn't fully done loading by this point).
  // Returns SUCCESS if the launch is successful.
  // Returns TIMEOUT if there was no response by action_timeout_
  // Returns VERSION_MISMATCH if the automation protocol version of the
  // automation provider does not match and if perform_version_check_ is set
  // to true. Note that perform_version_check_ defaults to false, call
  // set_perform_version_check() to set it.
  AutomationLaunchResult WaitForAppLaunch();

  // See description in AutomationMsg_WaitForProcessLauncherThreadToGoIdle.
  bool WaitForProcessLauncherThreadToGoIdle() WARN_UNUSED_RESULT;

  // Waits for any initial page loads to complete.
  // NOTE: this only fires once for a run of the application.
  // Returns true if the load is successful
  bool WaitForInitialLoads() WARN_UNUSED_RESULT;

  // Waits for the initial destinations tab to report that it has finished
  // querying.  |load_time| is filled in with how long it took, in milliseconds.
  // NOTE: this only fires once for a run of the application.
  // Returns true if the load is successful.
  bool WaitForInitialNewTabUILoad(int* load_time) WARN_UNUSED_RESULT;

  // Open a new browser window of type |type|, returning true on success. |show|
  // identifies whether the window should be shown. Returns true on success.
  bool OpenNewBrowserWindow(Browser::Type type, bool show) WARN_UNUSED_RESULT;

  // Fills the number of open browser windows into the given variable, returning
  // true on success. False likely indicates an IPC error.
  bool GetBrowserWindowCount(int* num_windows) WARN_UNUSED_RESULT;

  // Block the thread until the window count becomes the provided value.
  // Returns true on success.
  bool WaitForWindowCountToBecome(int target_count) WARN_UNUSED_RESULT;

  // Fills the number of open normal browser windows (normal type and
  // non-incognito mode) into the given variable, returning true on success.
  // False likely indicates an IPC error.
  bool GetNormalBrowserWindowCount(int* num_windows) WARN_UNUSED_RESULT;

  // Returns true if one of the tabs in any window displays given url.
  bool IsURLDisplayed(GURL url) WARN_UNUSED_RESULT;

  // Get the duration of the last |event_name| in the browser.  Returns
  // false if the IPC failed to send.
  bool GetMetricEventDuration(const std::string& event_name,
                              int* duration_ms) WARN_UNUSED_RESULT;

  // Returns the BrowserProxy for the browser window at the given index,
  // transferring ownership of the pointer to the caller.
  // On failure, returns NULL.
  //
  // Use GetBrowserWindowCount to see how many browser windows you can ask for.
  // Window numbers are 0-based.
  scoped_refptr<BrowserProxy> GetBrowserWindow(int window_index);

  // Sends the browser a new proxy configuration to start using. Returns true
  // if the proxy config was successfully sent, false otherwise.
  bool SendProxyConfig(const std::string& new_proxy_config) WARN_UNUSED_RESULT;

  // These methods are intended to be called by the background thread
  // to signal that the given event has occurred, and that any corresponding
  // Wait... function can return.
  void SignalAppLaunch(const std::string& version_string);
  void SignalInitialLoads();
  // load_time is how long, in ms, the tab contents took to load.
  void SignalNewTabUITab(int load_time);

  // Gets the next extension test result in |result|. Returns false if there
  // was a problem sending the result querying RPC.
  bool GetExtensionTestResult(bool* result, std::string* message);

  // Generic pattern for sending automation requests.
  bool SendJSONRequest(const std::string& request,
                       int timeout_ms,
                       std::string* response) WARN_UNUSED_RESULT;

  // Begin tracing specified categories on the browser instance. Blocks until
  // browser acknowledges that tracing has begun (or failed if false is
  // returned). |categories| is a comma-delimited list of category wildcards.
  // A category can have an optional '-' prefix to make it an excluded category.
  // Either all categories must be included or all must be excluded.
  //
  // Example: BeginTracing("test_MyTest*");
  // Example: BeginTracing("test_MyTest*,test_OtherStuff");
  // Example: BeginTracing("-excluded_category1,-excluded_category2");
  //
  // See base/event_trace.h for documentation of included and excluded
  // categories.
  bool BeginTracing(const std::string& categories) WARN_UNUSED_RESULT;

  // End trace and collect the trace output as a json string.
  bool EndTracing(std::string* json_trace_output) WARN_UNUSED_RESULT;

  IPC::SyncChannel* channel();

  // AutomationMessageSender implementation.
  virtual bool Send(IPC::Message* message) WARN_UNUSED_RESULT;
  virtual bool Send(IPC::Message* message, int timeout_ms) WARN_UNUSED_RESULT;

  // Wrapper over AutomationHandleTracker::InvalidateHandle. Receives the
  // message from AutomationProxy, unpacks the messages and routes that call to
  // the tracker.
  virtual void InvalidateHandle(const IPC::Message& message);

  // Creates a tab that can hosted in an external process. The function
  // returns a TabProxy representing the tab as well as a window handle
  // that can be reparented in another process.
  scoped_refptr<TabProxy> CreateExternalTab(
      const ExternalTabSettings& settings,
      gfx::AcceleratedWidget* external_tab_container,
      gfx::AcceleratedWidget* tab);

  base::TimeDelta action_timeout() const {
    return action_timeout_;
  }

  // Sets the timeout for subsequent automation calls.
  void set_action_timeout(base::TimeDelta timeout) {
    DCHECK(timeout <= base::TimeDelta::FromMinutes(10))
        << "10+ min of automation timeout "
           "can make the test hang and be killed by buildbot";
    action_timeout_ = timeout;
  }

  // Returns the server version of the server connected. You may only call this
  // method after WaitForAppLaunch() has returned SUCCESS or VERSION_MISMATCH.
  // If you call it before this, the return value is undefined.
  std::string server_version() const {
    return server_version_;
  }

  // Call this while passing true to tell the automation proxy to perform
  // a version check when WaitForAppLaunch() is called. Note that
  // platform_version_check_ defaults to false.
  void set_perform_version_check(bool perform_version_check) {
    perform_version_check_ = perform_version_check;
  }

  // These functions set and reset the IPC::Channel pointer on the tracker.
  void SetChannel(IPC::Channel* channel);
  void ResetChannel();

  // See description above |channel_disconnected_on_failure_|.
  bool channel_disconnected_on_failure() const {
    return channel_disconnected_on_failure_;
  }

 protected:
  template <class T> scoped_refptr<T> ProxyObjectFromHandle(int handle);
  void InitializeThread();
  void InitializeHandleTracker();

  scoped_ptr<base::Thread> thread_;
  scoped_ptr<IPC::SyncChannel> channel_;
  scoped_ptr<AutomationHandleTracker> tracker_;

  base::WaitableEvent app_launched_;
  base::WaitableEvent initial_loads_complete_;
  base::WaitableEvent new_tab_ui_load_complete_;
  int new_tab_ui_load_time_;

  // An event that notifies when we are shutting-down.
  scoped_ptr<base::WaitableEvent> shutdown_event_;

  // The version of the automation provider we are communicating with.
  std::string server_version_;

  // Whether to perform a version check between the automation proxy and
  // the automation provider at connection time. Defaults to false, you can
  // set this to true if building the automation proxy into a module with
  // a version resource.
  bool perform_version_check_;

  // If true, the proxy will disconnect the IPC channel on first failure
  // to send an IPC message. This helps avoid long timeouts in tests.
  bool disconnect_on_failure_;

  // Set if disconnect_on_failure_ caused the connection to be dropped.
  bool channel_disconnected_on_failure_;

  // Delay to let the browser execute the command.
  base::TimeDelta action_timeout_;

  base::PlatformThreadId listener_thread_id_;

  DISALLOW_COPY_AND_ASSIGN(AutomationProxy);
};

#endif  // CHROME_TEST_AUTOMATION_AUTOMATION_PROXY_H_
