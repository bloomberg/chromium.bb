// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_render_thread.h"

#include "base/process_util.h"
#include "content/common/view_messages.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_sync_message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

MockRenderThread::MockRenderThread()
    : routing_id_(0), opener_id_(0) {
}

MockRenderThread::~MockRenderThread() {
}

void MockRenderThread::VerifyRunJavaScriptMessageSend(
    const string16& expected_alert_message) {
  const IPC::Message* alert_msg =
      sink_.GetUniqueMessageMatching(ViewHostMsg_RunJavaScriptMessage::ID);
  ASSERT_TRUE(alert_msg);
  void* iter = IPC::SyncMessage::GetDataIterator(alert_msg);
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
  return std::string();
}

void MockRenderThread::AddRoute(int32 routing_id,
                                IPC::Channel::Listener* listener) {
  EXPECT_EQ(routing_id_, routing_id);
  widget_ = listener;
}

void MockRenderThread::RemoveRoute(int32 routing_id) {
  EXPECT_EQ(routing_id_, routing_id);
  widget_ = NULL;
}

int MockRenderThread::GenerateRoutingID() {
  NOTREACHED();
  return MSG_ROUTING_NONE;
}

void MockRenderThread::AddFilter(IPC::ChannelProxy::MessageFilter* filter) {
  filter->OnFilterAdded(&sink());
}

void MockRenderThread::RemoveFilter(IPC::ChannelProxy::MessageFilter* filter) {
  filter->OnFilterRemoved();
}

void MockRenderThread::SetOutgoingMessageFilter(
    IPC::ChannelProxy::OutgoingMessageFilter* filter) {
}

void MockRenderThread::AddObserver(content::RenderProcessObserver* observer) {
}

void MockRenderThread::RemoveObserver(
    content::RenderProcessObserver* observer) {
}

void MockRenderThread::SetResourceDispatcherDelegate(
    content::ResourceDispatcherDelegate* delegate) {
}

void MockRenderThread::WidgetHidden() {
}

void MockRenderThread::WidgetRestored() {
}

void MockRenderThread::EnsureWebKitInitialized() {
}

void MockRenderThread::RecordUserMetrics(const std::string& action) {
}

base::SharedMemoryHandle MockRenderThread::HostAllocateSharedMemoryBuffer(
    uint32 buffer_size) {
  base::SharedMemory shared_buf;
  if (!shared_buf.CreateAndMapAnonymous(buffer_size)) {
    NOTREACHED() << "Cannot map shared memory buffer";
    return base::SharedMemory::NULLHandle();
  }
  base::SharedMemoryHandle handle;
  shared_buf.GiveToProcess(base::GetCurrentProcessHandle(), &handle);
  return handle;
}

void MockRenderThread::RegisterExtension(v8::Extension* extension) {
}

bool MockRenderThread::IsRegisteredExtension(
    const std::string& v8_extension_name) const {
  return false;
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
void MockRenderThread::OnMsgCreateWidget(int opener_id,
                                         WebKit::WebPopupType popup_type,
                                         int* route_id) {
  opener_id_ = opener_id;
  *route_id = routing_id_;
}

bool MockRenderThread::OnMessageReceived(const IPC::Message& msg) {
  // Save the message in the sink.
  sink_.OnMessageReceived(msg);

  bool handled = true;
  bool msg_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(MockRenderThread, msg, msg_is_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWidget, OnMsgCreateWidget)
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
