// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automation/automation_proxy.h"

#include <sstream>

#include "base/basictypes.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/process_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "chrome/common/automation_constants.h"
#include "chrome/common/automation_messages.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/test/automation/automation_json_requests.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "ipc/ipc_descriptors.h"
#if defined(OS_WIN)
// TODO(port): Enable when dialog_delegate is ported.
#include "ui/views/window/dialog_delegate.h"
#endif

using base::TimeDelta;
using base::TimeTicks;

namespace {

const char kChannelErrorVersionString[] = "***CHANNEL_ERROR***";

// This object allows messages received on the background thread to be
// properly triaged.
class AutomationMessageFilter : public IPC::ChannelProxy::MessageFilter {
 public:
  explicit AutomationMessageFilter(AutomationProxy* server) : server_(server) {}

  // Return true to indicate that the message was handled, or false to let
  // the message be handled in the default way.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
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

  virtual void OnFilterAdded(IPC::Channel* channel) OVERRIDE {
    server_->SetChannel(channel);
  }

  virtual void OnFilterRemoved() OVERRIDE {
    server_->ResetChannel();
  }

  virtual void OnChannelError() OVERRIDE {
    server_->SignalAppLaunch(kChannelErrorVersionString);
    server_->SignalNewTabUITab(-1);
  }

 private:
  void NewTabLoaded(int load_time) {
    server_->SignalNewTabUITab(load_time);
  }

  void OnAutomationHello(const IPC::Message& hello_message) {
    std::string server_version;
    PickleIterator iter(hello_message);
    if (!hello_message.ReadString(&iter, &server_version)) {
      // We got an AutomationMsg_Hello from an old automation provider
      // that doesn't send version info. Leave server_version as an empty
      // string to signal a version mismatch.
      LOG(ERROR) << "Pre-versioning protocol detected in automation provider.";
    }

    server_->SignalAppLaunch(server_version);
  }

  AutomationProxy* server_;

  DISALLOW_COPY_AND_ASSIGN(AutomationMessageFilter);
};

}  // anonymous namespace


AutomationProxy::AutomationProxy(base::TimeDelta action_timeout,
                                 bool disconnect_on_failure)
    : app_launched_(true, false),
      initial_loads_complete_(true, false),
      new_tab_ui_load_complete_(true, false),
      shutdown_event_(new base::WaitableEvent(true, false)),
      perform_version_check_(false),
      disconnect_on_failure_(disconnect_on_failure),
      channel_disconnected_on_failure_(false),
      action_timeout_(action_timeout),
      listener_thread_id_(0) {
  // base::WaitableEvent::TimedWait() will choke if we give it a negative value.
  // Zero also seems unreasonable, since we need to wait for IPC, but at
  // least it is legal... ;-)
  DCHECK_GE(action_timeout.InMilliseconds(), 0);
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
      this,  // we are the listener
      thread_->message_loop_proxy(),
      shutdown_event_.get()));
  channel_->AddFilter(new AutomationMessageFilter(this));

  // Create the pipe synchronously so that Chrome doesn't try to connect to an
  // unready server. Note this is done after adding a message filter to
  // guarantee that it doesn't miss any messages when we are the client.
  // See crbug.com/102894.
  channel_->Init(
      channel_id,
      use_named_interface ? IPC::Channel::MODE_NAMED_CLIENT
                          : IPC::Channel::MODE_SERVER,
      true /* create_pipe_now */);
}

void AutomationProxy::InitializeHandleTracker() {
  tracker_.reset(new AutomationHandleTracker());
}

AutomationLaunchResult AutomationProxy::WaitForAppLaunch() {
  AutomationLaunchResult result = AUTOMATION_SUCCESS;
  if (app_launched_.TimedWait(action_timeout_)) {
    if (server_version_ == kChannelErrorVersionString) {
      result = AUTOMATION_CHANNEL_ERROR;
    } else if (perform_version_check_) {
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
  server_version_ = version_string;
  app_launched_.Signal();
}

bool AutomationProxy::WaitForProcessLauncherThreadToGoIdle() {
  return Send(new AutomationMsg_WaitForProcessLauncherThreadToGoIdle());
}

bool AutomationProxy::WaitForInitialLoads() {
  return initial_loads_complete_.TimedWait(action_timeout_);
}

bool AutomationProxy::WaitForInitialNewTabUILoad(int* load_time) {
  if (new_tab_ui_load_complete_.TimedWait(action_timeout_)) {
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

scoped_refptr<BrowserProxy> AutomationProxy::GetBrowserWindow(
    int window_index) {
  int handle = 0;
  if (!Send(new AutomationMsg_BrowserWindow(window_index, &handle)))
    return NULL;

  return ProxyObjectFromHandle<BrowserProxy>(handle);
}

IPC::SyncChannel* AutomationProxy::channel() {
  return channel_.get();
}

bool AutomationProxy::Send(IPC::Message* message) {
  return Send(message,
    static_cast<int>(action_timeout_.InMilliseconds()));
}

bool AutomationProxy::Send(IPC::Message* message, int timeout_ms) {
  if (!channel_.get()) {
    LOG(ERROR) << "Automation channel has been closed; dropping message!";
    delete message;
    return false;
  }

  bool success = channel_->SendWithTimeout(message, timeout_ms);

  if (!success && disconnect_on_failure_) {
    // Send failed (possibly due to a timeout). Browser is likely in a weird
    // state, and further IPC requests are extremely likely to fail (possibly
    // timeout, which would make tests slower). Disconnect the channel now
    // to avoid the slowness.
    channel_disconnected_on_failure_ = true;
    LOG(ERROR) << "Disconnecting channel after error!";
    Disconnect();
  }

  return success;
}

void AutomationProxy::InvalidateHandle(const IPC::Message& message) {
  PickleIterator iter(message);
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

bool AutomationProxy::BeginTracing(const std::string& categories) {
  bool result = false;
  bool send_success = Send(new AutomationMsg_BeginTracing(categories,
                                                          &result));
  return send_success && result;
}

bool AutomationProxy::EndTracing(std::string* json_trace_output) {
  bool success = false;
  size_t num_trace_chunks = 0;
  if (!Send(new AutomationMsg_EndTracing(&num_trace_chunks, &success)) ||
      !success)
    return false;

  std::string chunk;
  base::debug::TraceResultBuffer buffer;
  base::debug::TraceResultBuffer::SimpleOutput output;
  buffer.SetOutputCallback(output.GetCallback());

  // TODO(jbates): See bug 100255, IPC send fails if message is too big. This
  // code can be simplified if that limitation is fixed.
  // Workaround IPC payload size limitation by getting chunks.
  buffer.Start();
  for (size_t i = 0; i < num_trace_chunks; ++i) {
    // The broswer side AutomationProvider resets state at BeginTracing,
    // so it can recover even after this fails mid-way.
    if (!Send(new AutomationMsg_GetTracingOutput(&chunk, &success)) ||
        !success)
      return false;
    buffer.AddFragment(chunk);
  }
  buffer.Finish();

  *json_trace_output = output.json_output;
  return true;
}

bool AutomationProxy::SendJSONRequest(const std::string& request,
                                      int timeout_ms,
                                      std::string* response) {
  bool result = false;
  if (!SendAutomationJSONRequest(this, request, timeout_ms, response, &result))
    return false;
  return result;
}
