// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automation/automation_proxy.h"

#include <gtest/gtest.h>

#include <sstream>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/threading/platform_thread.h"
#include "base/process_util.h"
#include "base/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/common/automation_constants.h"
#include "chrome/common/automation_messages.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/extension_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "ipc/ipc_descriptors.h"
#if defined(OS_WIN)
// TODO(port): Enable when dialog_delegate is ported.
#include "views/window/dialog_delegate.h"
#endif

using base::TimeDelta;
using base::TimeTicks;

namespace {

// This object allows messages received on the background thread to be
// properly triaged.
class AutomationMessageFilter : public IPC::ChannelProxy::MessageFilter {
 public:
  explicit AutomationMessageFilter(AutomationProxy* server) : server_(server) {}

  // Return true to indicate that the message was handled, or false to let
  // the message be handled in the default way.
  virtual bool OnMessageReceived(const IPC::Message& message) {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(AutomationMessageFilter, message)
      IPC_MESSAGE_HANDLER_GENERIC(AutomationMsg_Hello,
                                  OnAutomationHello(message))
      IPC_MESSAGE_HANDLER_GENERIC(
        AutomationMsg_InitialLoadsComplete, server_->SignalInitialLoads())
      IPC_MESSAGE_HANDLER(AutomationMsg_InitialNewTabUILoadComplete,
                          NewTabLoaded)
      IPC_MESSAGE_HANDLER_GENERIC(
        AutomationMsg_InvalidateHandle, server_->InvalidateHandle(message))
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()

    return handled;
  }

  virtual void OnFilterAdded(IPC::Channel* channel) {
    server_->SetChannel(channel);
  }

  virtual void OnFilterRemoved() {
    server_->ResetChannel();
  }

  void NewTabLoaded(int load_time) {
    server_->SignalNewTabUITab(load_time);
  }

  void OnAutomationHello(const IPC::Message& hello_message) {
    std::string server_version;
    void* iter = NULL;
    if (!hello_message.ReadString(&iter, &server_version)) {
      // We got an AutomationMsg_Hello from an old automation provider
      // that doesn't send version info. Leave server_version as an empty
      // string to signal a version mismatch.
      LOG(ERROR) << "Pre-versioning protocol detected in automation provider.";
    }

    server_->SignalAppLaunch(server_version);
  }

 private:
  AutomationProxy* server_;
};

}  // anonymous namespace


AutomationProxy::AutomationProxy(int command_execution_timeout_ms,
                                 bool disconnect_on_failure)
    : app_launched_(true, false),
      initial_loads_complete_(true, false),
      new_tab_ui_load_complete_(true, false),
      shutdown_event_(new base::WaitableEvent(true, false)),
      app_launch_signaled_(0),
      perform_version_check_(false),
      disconnect_on_failure_(disconnect_on_failure),
      command_execution_timeout_(
          TimeDelta::FromMilliseconds(command_execution_timeout_ms)),
      listener_thread_id_(0) {
  // base::WaitableEvent::TimedWait() will choke if we give it a negative value.
  // Zero also seems unreasonable, since we need to wait for IPC, but at
  // least it is legal... ;-)
  DCHECK_GE(command_execution_timeout_ms, 0);
  listener_thread_id_ = base::PlatformThread::CurrentId();
  InitializeHandleTracker();
  InitializeThread();
}

AutomationProxy::~AutomationProxy() {
  // Destruction order is important. Thread has to outlive the channel and
  // tracker has to outlive the thread since we access the tracker inside
  // AutomationMessageFilter::OnMessageReceived.
  Disconnect();
  thread_.reset();
  tracker_.reset();
}

std::string AutomationProxy::GenerateChannelID() {
  // The channel counter keeps us out of trouble if we create and destroy
  // several AutomationProxies sequentially over the course of a test run.
  // (Creating the channel sometimes failed before when running a lot of
  // tests in sequence, and our theory is that sometimes the channel ID
  // wasn't getting freed up in time for the next test.)
  static int channel_counter = 0;

  std::ostringstream buf;
  buf << "ChromeTestingInterface:" << base::GetCurrentProcId() <<
         "." << ++channel_counter;
  return buf.str();
}

void AutomationProxy::InitializeThread() {
  scoped_ptr<base::Thread> thread(
      new base::Thread("AutomationProxy_BackgroundThread"));
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  bool thread_result = thread->StartWithOptions(options);
  DCHECK(thread_result);
  thread_.swap(thread);
}

void AutomationProxy::InitializeChannel(const std::string& channel_id,
                                        bool use_named_interface) {
  DCHECK(shutdown_event_.get() != NULL);

  // TODO(iyengar)
  // The shutdown event could be global on the same lines as the automation
  // provider, where we use the shutdown event provided by the chrome browser
  // process.
  channel_.reset(new IPC::SyncChannel(
    channel_id,
    use_named_interface ? IPC::Channel::MODE_NAMED_CLIENT
                        : IPC::Channel::MODE_SERVER,
    this,  // we are the listener
    thread_->message_loop(),
    true,
    shutdown_event_.get()));
  channel_->AddFilter(new AutomationMessageFilter(this));
}

void AutomationProxy::InitializeHandleTracker() {
  tracker_.reset(new AutomationHandleTracker());
}

AutomationLaunchResult AutomationProxy::WaitForAppLaunch() {
  AutomationLaunchResult result = AUTOMATION_SUCCESS;
  if (app_launched_.TimedWait(command_execution_timeout_)) {
    if (perform_version_check_) {
      // Obtain our own version number and compare it to what the automation
      // provider sent.
      chrome::VersionInfo version_info;
      DCHECK(version_info.is_valid());

      // Note that we use a simple string comparison since we expect the version
      // to be a punctuated numeric string. Consider using base/Version if we
      // ever need something more complicated here.
      if (server_version_ != version_info.Version()) {
        result = AUTOMATION_VERSION_MISMATCH;
      }
    }
  } else {
    result = AUTOMATION_TIMEOUT;
  }
  return result;
}

void AutomationProxy::SignalAppLaunch(const std::string& version_string) {
  // The synchronization of the reading / writing of server_version_ is a bit
  // messy but does work as long as SignalAppLaunch is only called once.
  // Review this if we ever want an AutomationProxy instance to launch
  // multiple AutomationProviders.
  app_launch_signaled_++;
  if (app_launch_signaled_ > 1) {
    NOTREACHED();
    LOG(ERROR) << "Multiple AutomationMsg_Hello messages received";
    return;
  }
  server_version_ = version_string;
  app_launched_.Signal();
}

bool AutomationProxy::WaitForProcessLauncherThreadToGoIdle() {
  return Send(new AutomationMsg_WaitForProcessLauncherThreadToGoIdle());
}

bool AutomationProxy::WaitForInitialLoads() {
  return initial_loads_complete_.TimedWait(command_execution_timeout_);
}

bool AutomationProxy::WaitForInitialNewTabUILoad(int* load_time) {
  if (new_tab_ui_load_complete_.TimedWait(command_execution_timeout_)) {
    *load_time = new_tab_ui_load_time_;
    new_tab_ui_load_complete_.Reset();
    return true;
  }
  return false;
}

void AutomationProxy::SignalInitialLoads() {
  initial_loads_complete_.Signal();
}

void AutomationProxy::SignalNewTabUITab(int load_time) {
  new_tab_ui_load_time_ = load_time;
  new_tab_ui_load_complete_.Signal();
}

bool AutomationProxy::SavePackageShouldPromptUser(bool should_prompt) {
  return Send(new AutomationMsg_SavePackageShouldPromptUser(should_prompt));
}

scoped_refptr<ExtensionProxy> AutomationProxy::InstallExtension(
    const FilePath& crx_file, bool with_ui) {
  int handle = 0;
  if (!Send(new AutomationMsg_InstallExtensionAndGetHandle(crx_file, with_ui,
                                                           &handle)))
    return NULL;

  return ProxyObjectFromHandle<ExtensionProxy>(handle);
}

void AutomationProxy::EnsureExtensionTestResult() {
  bool result;
  std::string message;
  if (!Send(new AutomationMsg_WaitForExtensionTestResult(&result,
                                                         &message))) {
    FAIL() << "Could not send WaitForExtensionTestResult message";
    return;
  }
  ASSERT_TRUE(result) << "Extension test message: " << message;
}

bool AutomationProxy::GetEnabledExtensions(
    std::vector<FilePath>* extension_directories) {
  return Send(new AutomationMsg_GetEnabledExtensions(extension_directories));
}

bool AutomationProxy::GetBrowserWindowCount(int* num_windows) {
  if (!num_windows) {
    NOTREACHED();
    return false;
  }

  return Send(new AutomationMsg_BrowserWindowCount(num_windows));
}

bool AutomationProxy::GetNormalBrowserWindowCount(int* num_windows) {
  if (!num_windows) {
    NOTREACHED();
    return false;
  }

  return Send(new AutomationMsg_NormalBrowserWindowCount(num_windows));
}

bool AutomationProxy::WaitForWindowCountToBecome(int count) {
  bool wait_success = false;
  if (!Send(new AutomationMsg_WaitForBrowserWindowCountToBecome(
                count, &wait_success))) {
    return false;
  }
  return wait_success;
}

bool AutomationProxy::GetShowingAppModalDialog(
    bool* showing_app_modal_dialog,
    ui::MessageBoxFlags::DialogButton* button) {
  if (!showing_app_modal_dialog || !button) {
    NOTREACHED();
    return false;
  }

  int button_int = 0;

  if (!Send(new AutomationMsg_ShowingAppModalDialog(
                showing_app_modal_dialog, &button_int))) {
    return false;
  }

  *button = static_cast<ui::MessageBoxFlags::DialogButton>(button_int);
  return true;
}

bool AutomationProxy::ClickAppModalDialogButton(
    ui::MessageBoxFlags::DialogButton button) {
  bool succeeded = false;

  if (!Send(new AutomationMsg_ClickAppModalDialogButton(
                button, &succeeded))) {
    return false;
  }

  return succeeded;
}

bool AutomationProxy::WaitForAppModalDialog() {
  bool wait_success = false;
  if (!Send(new AutomationMsg_WaitForAppModalDialogToBeShown(&wait_success)))
    return false;
  return wait_success;
}

bool AutomationProxy::IsURLDisplayed(GURL url) {
  int window_count;
  if (!GetBrowserWindowCount(&window_count))
    return false;

  for (int i = 0; i < window_count; i++) {
    scoped_refptr<BrowserProxy> window = GetBrowserWindow(i);
    if (!window.get())
      break;

    int tab_count;
    if (!window->GetTabCount(&tab_count))
      continue;

    for (int j = 0; j < tab_count; j++) {
      scoped_refptr<TabProxy> tab = window->GetTab(j);
      if (!tab.get())
        break;

      GURL tab_url;
      if (!tab->GetCurrentURL(&tab_url))
        continue;

      if (tab_url == url)
        return true;
    }
  }

  return false;
}

bool AutomationProxy::GetMetricEventDuration(const std::string& event_name,
                                             int* duration_ms) {
  return Send(new AutomationMsg_GetMetricEventDuration(event_name,
                                                       duration_ms));
}

bool AutomationProxy::SetFilteredInet(bool enabled) {
  return Send(new AutomationMsg_SetFilteredInet(enabled));
}

int AutomationProxy::GetFilteredInetHitCount() {
  int hit_count;
  if (!Send(new AutomationMsg_GetFilteredInetHitCount(&hit_count)))
    return -1;
  return hit_count;
}

bool AutomationProxy::SendProxyConfig(const std::string& new_proxy_config) {
  return Send(new AutomationMsg_SetProxyConfig(new_proxy_config));
}

void AutomationProxy::Disconnect() {
  DCHECK(shutdown_event_.get() != NULL);
  shutdown_event_->Signal();
  channel_.reset();
}

bool AutomationProxy::OnMessageReceived(const IPC::Message& msg) {
  // This won't get called unless AutomationProxy is run from
  // inside a message loop.
  NOTREACHED();
  return false;
}

void AutomationProxy::OnChannelError() {
  LOG(ERROR) << "Channel error in AutomationProxy.";
  if (disconnect_on_failure_)
    Disconnect();
}

scoped_refptr<WindowProxy> AutomationProxy::GetActiveWindow() {
  int handle = 0;
  if (!Send(new AutomationMsg_ActiveWindow(&handle)))
    return NULL;

  return ProxyObjectFromHandle<WindowProxy>(handle);
}

scoped_refptr<BrowserProxy> AutomationProxy::GetBrowserWindow(
    int window_index) {
  int handle = 0;
  if (!Send(new AutomationMsg_BrowserWindow(window_index, &handle)))
    return NULL;

  return ProxyObjectFromHandle<BrowserProxy>(handle);
}

bool AutomationProxy::GetBrowserLocale(string16* locale) {
  DCHECK(locale != NULL);
  if (!Send(new AutomationMsg_GetBrowserLocale(locale)))
    return false;

  return !locale->empty();
}

scoped_refptr<BrowserProxy> AutomationProxy::FindNormalBrowserWindow() {
  int handle = 0;
  if (!Send(new AutomationMsg_FindNormalBrowserWindow(&handle)))
    return NULL;

  return ProxyObjectFromHandle<BrowserProxy>(handle);
}

scoped_refptr<BrowserProxy> AutomationProxy::GetLastActiveBrowserWindow() {
  int handle = 0;
  if (!Send(new AutomationMsg_LastActiveBrowserWindow(&handle)))
    return NULL;

  return ProxyObjectFromHandle<BrowserProxy>(handle);
}

#if defined(OS_POSIX)
base::file_handle_mapping_vector AutomationProxy::fds_to_map() const {
  base::file_handle_mapping_vector map;
  const int ipcfd = channel_->GetClientFileDescriptor();
  if (ipcfd > -1)
    map.push_back(std::make_pair(ipcfd, kPrimaryIPCChannel + 3));
  return map;
}
#endif  // defined(OS_POSIX)

bool AutomationProxy::Send(IPC::Message* message) {
  if (!channel_.get()) {
    LOG(ERROR) << "Automation channel has been closed; dropping message!";
    delete message;
    return false;
  }

  bool success = channel_->SendWithTimeout(message,
                                           command_execution_timeout_ms());

  if (!success && disconnect_on_failure_) {
    // Send failed (possibly due to a timeout). Browser is likely in a weird
    // state, and further IPC requests are extremely likely to fail (possibly
    // timeout, which would make tests slower). Disconnect the channel now
    // to avoid the slowness.
    LOG(ERROR) << "Disconnecting channel after error!";
    Disconnect();
  }

  return success;
}

void AutomationProxy::InvalidateHandle(const IPC::Message& message) {
  void* iter = NULL;
  int handle;

  if (message.ReadInt(&iter, &handle)) {
    tracker_->InvalidateHandle(handle);
  }
}

bool AutomationProxy::OpenNewBrowserWindow(Browser::Type type, bool show) {
  return Send(
      new AutomationMsg_OpenNewBrowserWindowOfType(static_cast<int>(type),
                                                   show));
}

scoped_refptr<TabProxy> AutomationProxy::CreateExternalTab(
    const ExternalTabSettings& settings,
    gfx::NativeWindow* external_tab_container,
    gfx::NativeWindow* tab) {
  int handle = 0;
  int session_id = 0;
  bool succeeded =
      Send(new AutomationMsg_CreateExternalTab(settings,
                                               external_tab_container,
                                               tab,
                                               &handle,
                                               &session_id));
  if (!succeeded) {
    return NULL;
  }

#if defined(OS_WIN)
  DCHECK(IsWindow(*external_tab_container));
#else  // defined(OS_WIN)
  DCHECK(*external_tab_container);
#endif  // defined(OS_WIN)
  DCHECK(tracker_->GetResource(handle) == NULL);
  return new TabProxy(this, tracker_.get(), handle);
}

template <class T> scoped_refptr<T> AutomationProxy::ProxyObjectFromHandle(
    int handle) {
  if (!handle)
    return NULL;

  // Get AddRef-ed pointer to the object if handle is already seen.
  T* p = static_cast<T*>(tracker_->GetResource(handle));
  if (!p) {
    p = new T(this, tracker_.get(), handle);
    p->AddRef();
  }

  // Since there is no scoped_refptr::attach.
  scoped_refptr<T> result;
  result.swap(&p);
  return result;
}

void AutomationProxy::SetChannel(IPC::Channel* channel) {
  if (tracker_.get())
    tracker_->put_channel(channel);
}

void AutomationProxy::ResetChannel() {
  if (tracker_.get())
    tracker_->put_channel(NULL);
}

#if defined(OS_CHROMEOS)
bool AutomationProxy::LoginWithUserAndPass(const std::string& username,
                                           const std::string& password) {
  bool success;
  bool sent = Send(new AutomationMsg_LoginWithUserAndPass(username,
                                                          password,
                                                          &success));
  // If message sending unsuccessful or test failed, return false.
  return sent && success;
}
#endif

bool AutomationProxy::ResetToDefaultTheme() {
  return Send(new AutomationMsg_ResetToDefaultTheme());
}

bool AutomationProxy::SendJSONRequest(const std::string& request,
                                      std::string* response) {
  bool result = false;
  return Send(new AutomationMsg_SendJSONRequest(
      -1, request, response, &result));
  return result;
}

