// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/plugin_channel_host.h"

#include "base/metrics/histogram.h"
#include "base/time.h"
#include "content/common/child_process.h"
#include "content/common/npobject_base.h"
#include "content/common/plugin_messages.h"

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"

// TODO(shess): Debugging for http://crbug.com/97285
//
// The hypothesis at #55 requires that RemoveRoute() be called between
// sending ViewHostMsg_OpenChannelToPlugin to the browser, and calling
// GetPluginChannelHost() on the result.  This code detects that case
// and stores the backtrace of the RemoveRoute() in a breakpad key.
// The specific RemoveRoute() is not tracked (there could be multiple,
// and which is the one can't be known until the open completes), but
// the backtrace from any such nested call should be sufficient to
// drive a repro.
#if defined(OS_MACOSX)
#include "base/debug/crash_logging.h"
#include "base/debug/stack_trace.h"
#include "base/strings/sys_string_conversions.h"

namespace {

// Breakpad key for the RemoveRoute() backtrace.
const char* kRemoveRouteTraceKey = "remove_route_bt";

// Breakpad key for the OnChannelError() backtrace.
const char* kChannelErrorTraceKey = "channel_error_bt";

// GetRemoveTrackingFlag() exposes this so that
// WebPluginDelegateProxy::Initialize() can do scoped set/reset.  When
// true, RemoveRoute() knows WBDP::Initialize() is on the stack, and
// records the backtrace.
bool remove_tracking = false;

}  // namespace
#endif

namespace content {

// A simple MessageFilter that will ignore all messages and respond to sync
// messages with an error when is_listening_ is false.
class IsListeningFilter : public IPC::ChannelProxy::MessageFilter {
 public:
  IsListeningFilter() : channel_(NULL) {}

  // MessageFilter overrides
  virtual void OnFilterRemoved() OVERRIDE {}
  virtual void OnFilterAdded(IPC::Channel* channel) OVERRIDE {
    channel_ = channel;
  }
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  static bool is_listening_;

 protected:
  virtual ~IsListeningFilter() {}

 private:
  IPC::Channel* channel_;

  DISALLOW_COPY_AND_ASSIGN(IsListeningFilter);
};

bool IsListeningFilter::OnMessageReceived(const IPC::Message& message) {
  if (IsListeningFilter::is_listening_) {
    // Proceed with normal operation.
    return false;
  }

  // Always process message reply to prevent renderer from hanging on sync
  // messages.
  if (message.is_reply() || message.is_reply_error()) {
    return false;
  }

  // Reply to synchronous messages with an error (so they don't block while
  // we're not listening).
  if (message.is_sync()) {
    IPC::Message* reply = IPC::SyncMessage::GenerateReply(&message);
    reply->set_reply_error();
    channel_->Send(reply);
  }
  return true;
}

// static
bool IsListeningFilter::is_listening_ = true;

// static
bool PluginChannelHost::IsListening() {
  return IsListeningFilter::is_listening_;
}

// static
void PluginChannelHost::SetListening(bool flag) {
  IsListeningFilter::is_listening_ = flag;
}

#if defined(OS_MACOSX)
// static
bool* PluginChannelHost::GetRemoveTrackingFlag() {
  return &remove_tracking;
}
#endif

// static
PluginChannelHost* PluginChannelHost::GetPluginChannelHost(
    const IPC::ChannelHandle& channel_handle,
    base::MessageLoopProxy* ipc_message_loop) {
  PluginChannelHost* result =
      static_cast<PluginChannelHost*>(NPChannelBase::GetChannel(
          channel_handle,
          IPC::Channel::MODE_CLIENT,
          ClassFactory,
          ipc_message_loop,
          true,
          ChildProcess::current()->GetShutDownEvent()));
  return result;
}

PluginChannelHost::PluginChannelHost() : expecting_shutdown_(false) {
}

PluginChannelHost::~PluginChannelHost() {
}

bool PluginChannelHost::Init(base::MessageLoopProxy* ipc_message_loop,
                             bool create_pipe_now,
                             base::WaitableEvent* shutdown_event) {
  bool ret =
      NPChannelBase::Init(ipc_message_loop, create_pipe_now, shutdown_event);
  if (ret) {
    is_listening_filter_ = new IsListeningFilter;
    channel_->AddFilter(is_listening_filter_);
  }
  return ret;
}

int PluginChannelHost::GenerateRouteID() {
  int route_id = MSG_ROUTING_NONE;
  Send(new PluginMsg_GenerateRouteID(&route_id));

  return route_id;
}

void PluginChannelHost::AddRoute(int route_id,
                                 IPC::Listener* listener,
                                 NPObjectBase* npobject) {
  NPChannelBase::AddRoute(route_id, listener, npobject);

  if (!npobject)
    proxies_[route_id] = listener;
}

void PluginChannelHost::RemoveRoute(int route_id) {
#if defined(OS_MACOSX)
  if (remove_tracking) {
    base::debug::StackTrace trace;
    size_t count = 0;
    const void* const* addresses = trace.Addresses(&count);
    base::debug::SetCrashKeyFromAddresses(
        kRemoveRouteTraceKey, addresses, count);
  }
#endif

  proxies_.erase(route_id);
  NPChannelBase::RemoveRoute(route_id);
}

bool PluginChannelHost::OnControlMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PluginChannelHost, message)
    IPC_MESSAGE_HANDLER(PluginHostMsg_SetException, OnSetException)
    IPC_MESSAGE_HANDLER(PluginHostMsg_PluginShuttingDown, OnPluginShuttingDown)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  return handled;
}

void PluginChannelHost::OnSetException(const std::string& message) {
  WebKit::WebBindings::setException(NULL, message.c_str());
}

void PluginChannelHost::OnPluginShuttingDown() {
  expecting_shutdown_ = true;
}

bool PluginChannelHost::Send(IPC::Message* msg) {
  if (msg->is_sync()) {
    base::TimeTicks start_time(base::TimeTicks::Now());
    bool result = NPChannelBase::Send(msg);
    UMA_HISTOGRAM_TIMES("Plugin.SyncMessageTime",
                        base::TimeTicks::Now() - start_time);
    return result;
  }
  return NPChannelBase::Send(msg);
}

void PluginChannelHost::OnChannelError() {
#if defined(OS_MACOSX)
  if (remove_tracking) {
    base::debug::StackTrace trace;
    size_t count = 0;
    const void* const* addresses = trace.Addresses(&count);
    base::debug::SetCrashKeyFromAddresses(
        kChannelErrorTraceKey, addresses, count);
  }
#endif

  NPChannelBase::OnChannelError();

  for (ProxyMap::iterator iter = proxies_.begin();
       iter != proxies_.end(); iter++) {
    iter->second->OnChannelError();
  }

  proxies_.clear();
}

}  // namespace content
