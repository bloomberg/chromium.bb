// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_render_thread.h"

#include "base/message_loop/message_loop_proxy.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/public/renderer/render_process_observer.h"
#include "content/renderer/render_view_impl.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_sync_message.h"
#include "ipc/message_filter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebScriptController.h"

namespace content {

MockRenderThread::MockRenderThread()
    : routing_id_(0),
      surface_id_(0),
      opener_id_(0),
      new_window_routing_id_(0),
      new_window_main_frame_routing_id_(0),
      new_frame_routing_id_(0) {
}

MockRenderThread::~MockRenderThread() {
  while (!filters_.empty()) {
    scoped_refptr<IPC::MessageFilter> filter = filters_.back();
    filters_.pop_back();
    filter->OnFilterRemoved();
  }
}

// Called by the Widget. Used to send messages to the browser.
// We short-circuit the mechanism and handle the messages right here on this
// class.
bool MockRenderThread::Send(IPC::Message* msg) {
  // We need to simulate a synchronous channel, thus we are going to receive
  // through this function messages, messages with reply and reply messages.
  // We can only handle one synchronous message at a time.
  if (msg->is_reply()) {
    if (reply_deserializer_) {
      reply_deserializer_->SerializeOutputParameters(*msg);
      reply_deserializer_.reset();
    }
  } else {
    if (msg->is_sync()) {
      // We actually need to handle deleting the reply deserializer for sync
      // messages.
      reply_deserializer_.reset(
          static_cast<IPC::SyncMessage*>(msg)->GetReplyDeserializer());
    }
    if (msg->routing_id() == MSG_ROUTING_CONTROL)
      OnControlMessageReceived(*msg);
    else
      OnMessageReceived(*msg);
  }
  delete msg;
  return true;
}

base::MessageLoop* MockRenderThread::GetMessageLoop() {
  return NULL;
}

IPC::SyncChannel* MockRenderThread::GetChannel() {
  return NULL;
}

std::string MockRenderThread::GetLocale() {
  return "en-US";
}

IPC::SyncMessageFilter* MockRenderThread::GetSyncMessageFilter() {
  return NULL;
}

scoped_refptr<base::MessageLoopProxy>
    MockRenderThread::GetIOMessageLoopProxy() {
  return scoped_refptr<base::MessageLoopProxy>();
}

void MockRenderThread::AddRoute(int32 routing_id, IPC::Listener* listener) {
}

void MockRenderThread::RemoveRoute(int32 routing_id) {
}

int MockRenderThread::GenerateRoutingID() {
  NOTREACHED();
  return MSG_ROUTING_NONE;
}

void MockRenderThread::AddFilter(IPC::MessageFilter* filter) {
  filter->OnFilterAdded(&sink());
  // Add this filter to a vector so the MockRenderThread::RemoveFilter function
  // can check if this filter is added.
  filters_.push_back(make_scoped_refptr(filter));
}

void MockRenderThread::RemoveFilter(IPC::MessageFilter* filter) {
  // Emulate the IPC::ChannelProxy::OnRemoveFilter function.
  for (size_t i = 0; i < filters_.size(); ++i) {
    if (filters_[i].get() == filter) {
      filter->OnFilterRemoved();
      filters_.erase(filters_.begin() + i);
      return;
    }
  }
  NOTREACHED() << "filter to be removed not found";
}

void MockRenderThread::AddObserver(RenderProcessObserver* observer) {
  observers_.AddObserver(observer);
}

void MockRenderThread::RemoveObserver(RenderProcessObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MockRenderThread::SetResourceDispatcherDelegate(
    ResourceDispatcherDelegate* delegate) {
}

void MockRenderThread::EnsureWebKitInitialized() {
}

void MockRenderThread::RecordAction(const base::UserMetricsAction& action) {
}

void MockRenderThread::RecordComputedAction(const std::string& action) {
}

scoped_ptr<base::SharedMemory>
    MockRenderThread::HostAllocateSharedMemoryBuffer(
        size_t buffer_size) {
  scoped_ptr<base::SharedMemory> shared_buf(new base::SharedMemory);
  if (!shared_buf->CreateAnonymous(buffer_size)) {
    NOTREACHED() << "Cannot map shared memory buffer";
    return scoped_ptr<base::SharedMemory>();
  }

  return scoped_ptr<base::SharedMemory>(shared_buf.release());
}

void MockRenderThread::RegisterExtension(v8::Extension* extension) {
  blink::WebScriptController::registerExtension(extension);
}

void MockRenderThread::ScheduleIdleHandler(int64 initial_delay_ms) {
}

void MockRenderThread::IdleHandler() {
}

int64 MockRenderThread::GetIdleNotificationDelayInMs() const {
  return 0;
}

void MockRenderThread::SetIdleNotificationDelayInMs(
    int64 idle_notification_delay_in_ms) {
}

void MockRenderThread::UpdateHistograms(int sequence_number) {
}

int MockRenderThread::PostTaskToAllWebWorkers(const base::Closure& closure) {
  return 0;
}

bool MockRenderThread::ResolveProxy(const GURL& url, std::string* proxy_list) {
  return false;
}

base::WaitableEvent* MockRenderThread::GetShutdownEvent() {
  return NULL;
}

#if defined(OS_WIN)
void MockRenderThread::PreCacheFont(const LOGFONT& log_font) {
}

void MockRenderThread::ReleaseCachedFonts() {
}

#endif  // OS_WIN

ServiceRegistry* MockRenderThread::GetServiceRegistry() {
  return NULL;
}

void MockRenderThread::SendCloseMessage() {
  ViewMsg_Close msg(routing_id_);
  RenderViewImpl::FromRoutingID(routing_id_)->OnMessageReceived(msg);
}

// The Widget expects to be returned valid route_id.
void MockRenderThread::OnCreateWidget(int opener_id,
                                      blink::WebPopupType popup_type,
                                      int* route_id,
                                      int* surface_id) {
  opener_id_ = opener_id;
  *route_id = routing_id_;
  *surface_id = surface_id_;
}

// The View expects to be returned a valid route_id different from its own.
void MockRenderThread::OnCreateWindow(
    const ViewHostMsg_CreateWindow_Params& params,
    int* route_id,
    int* main_frame_route_id,
    int* surface_id,
    int64* cloned_session_storage_namespace_id) {
  *route_id = new_window_routing_id_;
  *main_frame_route_id = new_window_main_frame_routing_id_;
  *surface_id = surface_id_;
  *cloned_session_storage_namespace_id = 0;
}

// The Frame expects to be returned a valid route_id different from its own.
void MockRenderThread::OnCreateChildFrame(int new_frame_routing_id,
                                          const std::string& frame_name,
                                          int* new_render_frame_id) {
  *new_render_frame_id = new_frame_routing_id_++;
}

bool MockRenderThread::OnControlMessageReceived(const IPC::Message& msg) {
  ObserverListBase<RenderProcessObserver>::Iterator it(observers_);
  RenderProcessObserver* observer;
  while ((observer = it.GetNext()) != NULL) {
    if (observer->OnControlMessageReceived(msg))
      return true;
  }
  return OnMessageReceived(msg);
}

bool MockRenderThread::OnMessageReceived(const IPC::Message& msg) {
  // Save the message in the sink.
  sink_.OnMessageReceived(msg);

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MockRenderThread, msg)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWidget, OnCreateWidget)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWindow, OnCreateWindow)
    IPC_MESSAGE_HANDLER(FrameHostMsg_CreateChildFrame, OnCreateChildFrame)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

#if defined(OS_WIN)
void MockRenderThread::OnDuplicateSection(
    base::SharedMemoryHandle renderer_handle,
    base::SharedMemoryHandle* browser_handle) {
  // We don't have to duplicate the input handles since RenderViewTest does not
  // separate a browser process from a renderer process.
  *browser_handle = renderer_handle;
}
#endif  // defined(OS_WIN)

}  // namespace content
