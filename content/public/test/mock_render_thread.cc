// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_render_thread.h"

#include "base/process_util.h"
#include "base/message_loop_proxy.h"
#include "content/common/view_messages.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_sync_message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

MockRenderThread::MockRenderThread()
    : routing_id_(0), surface_id_(0), opener_id_(0), new_window_routing_id_(0) {
}

MockRenderThread::~MockRenderThread() {
}

void MockRenderThread::VerifyRunJavaScriptMessageSend(
    const string16& expected_alert_message) {
  const IPC::Message* alert_msg =
      sink_.GetUniqueMessageMatching(ViewHostMsg_RunJavaScriptMessage::ID);
  ASSERT_TRUE(alert_msg);
  PickleIterator iter = IPC::SyncMessage::GetDataIterator(alert_msg);
  ViewHostMsg_RunJavaScriptMessage::SendParam alert_param;
  ASSERT_TRUE(IPC::ReadParam(alert_msg, &iter, &alert_param));
  EXPECT_EQ(expected_alert_message, alert_param.a);
}

// Called by the Widget. Used to send messages to the browser.
// We short-circuit the mechanism and handle the messages right here on this
// class.
bool MockRenderThread::Send(IPC::Message* msg) {
  // We need to simulate a synchronous channel, thus we are going to receive
  // through this function messages, messages with reply and reply messages.
  // We can only handle one synchronous message at a time.
  if (msg->is_reply()) {
    if (reply_deserializer_.get()) {
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
    OnMessageReceived(*msg);
  }
  delete msg;
  return true;
}

MessageLoop* MockRenderThread::GetMessageLoop() {
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
  // We may hear this for views created from OnCreateWindow as well,
  // in which case we don't want to track the new widget.
  if (routing_id_ == routing_id)
    widget_ = listener;
}

void MockRenderThread::RemoveRoute(int32 routing_id) {
  // We may hear this for views created from OnCreateWindow as well,
  // in which case we don't want to track the new widget.
  if (routing_id_ == routing_id)
    widget_ = NULL;
}

int MockRenderThread::GenerateRoutingID() {
  NOTREACHED();
  return MSG_ROUTING_NONE;
}

void MockRenderThread::AddFilter(IPC::ChannelProxy::MessageFilter* filter) {
  filter->OnFilterAdded(&sink());
  // Add this filter to a vector so the MockRenderThread::RemoveFilter function
  // can check if this filter is added.
  filters_.push_back(make_scoped_refptr(filter));
}

void MockRenderThread::RemoveFilter(IPC::ChannelProxy::MessageFilter* filter) {
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

void MockRenderThread::SetOutgoingMessageFilter(
    IPC::ChannelProxy::OutgoingMessageFilter* filter) {
}

void MockRenderThread::AddObserver(RenderProcessObserver* observer) {
}

void MockRenderThread::RemoveObserver(RenderProcessObserver* observer) {
}

void MockRenderThread::SetResourceDispatcherDelegate(
    ResourceDispatcherDelegate* delegate) {
}

void MockRenderThread::WidgetHidden() {
}

void MockRenderThread::WidgetRestored() {
}

void MockRenderThread::EnsureWebKitInitialized() {
}

void MockRenderThread::RecordUserMetrics(const std::string& action) {
}

scoped_ptr<base::SharedMemory>
    MockRenderThread::HostAllocateSharedMemoryBuffer(
        size_t buffer_size) {
  scoped_ptr<base::SharedMemory> shared_buf(new base::SharedMemory);
  if (!shared_buf->CreateAndMapAnonymous(buffer_size)) {
    NOTREACHED() << "Cannot map shared memory buffer";
    return scoped_ptr<base::SharedMemory>();
  }

  return scoped_ptr<base::SharedMemory>(shared_buf.release());
}

void MockRenderThread::RegisterExtension(v8::Extension* extension) {
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

void MockRenderThread::ToggleWebKitSharedTimer(bool suspend) {
}

void MockRenderThread::UpdateHistograms(int sequence_number) {
}

bool MockRenderThread::ResolveProxy(const GURL& url, std::string* proxy_list) {
  return false;
}

#if defined(OS_WIN)
void MockRenderThread::PreCacheFont(const LOGFONT& log_font) {
}

void MockRenderThread::ReleaseCachedFonts() {
}

#endif  // OS_WIN

void MockRenderThread::SendCloseMessage() {
  ViewMsg_Close msg(routing_id_);
  widget_->OnMessageReceived(msg);
}

// The Widget expects to be returned valid route_id.
void MockRenderThread::OnCreateWidget(int opener_id,
                                      WebKit::WebPopupType popup_type,
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
    int* surface_id,
    int64* cloned_session_storage_namespace_id) {
  *route_id = new_window_routing_id_;
  *surface_id = surface_id_;
  *cloned_session_storage_namespace_id = 0;
}

bool MockRenderThread::OnMessageReceived(const IPC::Message& msg) {
  // Save the message in the sink.
  sink_.OnMessageReceived(msg);

  bool handled = true;
  bool msg_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(MockRenderThread, msg, msg_is_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWidget, OnCreateWidget)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWindow, OnCreateWindow)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
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
