// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/mock_render_thread.h"

#include <fcntl.h>

#include "base/file_util.h"
#include "base/process_util.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/common/url_constants.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_sync_message.h"
#include "testing/gtest/include/gtest/gtest.h"

MockRenderThread::MockRenderThread()
    : routing_id_(0),
      opener_id_(0),
      widget_(NULL),
      reply_deserializer_(NULL),
      printer_(new MockPrinter),
      is_extension_process_(false) {
}

MockRenderThread::~MockRenderThread() {
}

const ExtensionSet* MockRenderThread::GetExtensions() const {
  return &extensions_;
}

// Called by the Widget. The routing_id must match the routing id assigned
// to the Widget in reply to ViewHostMsg_CreateWidget message.
void MockRenderThread::AddRoute(int32 routing_id,
                                IPC::Channel::Listener* listener) {
  EXPECT_EQ(routing_id_, routing_id);
  widget_ = listener;
}

// Called by the Widget. The routing id must match the routing id of AddRoute.
void MockRenderThread::RemoveRoute(int32 routing_id) {
  EXPECT_EQ(routing_id_, routing_id);
  widget_ = NULL;
}

// Called by, for example, RenderView::Init(), when adding a new message filter.
void MockRenderThread::AddFilter(IPC::ChannelProxy::MessageFilter* filter) {
  filter->OnFilterAdded(&sink());
}

// Called when the filter is removed.
void MockRenderThread::RemoveFilter(IPC::ChannelProxy::MessageFilter* filter) {
  filter->OnFilterRemoved();
}

bool MockRenderThread::IsExtensionProcess() const {
  return is_extension_process_;
}

bool MockRenderThread::IsIncognitoProcess() const {
  return false;
}

// Called by the Widget. Used to send messages to the browser.
// We short-circuit the mechanim and handle the messages right here on this
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

void MockRenderThread::SendCloseMessage() {
  ViewMsg_Close msg(routing_id_);
  widget_->OnMessageReceived(msg);
}

bool MockRenderThread::OnMessageReceived(const IPC::Message& msg) {
  // Save the message in the sink.
  sink_.OnMessageReceived(msg);

  // Some messages we do special handling.
  bool handled = true;
  bool msg_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(MockRenderThread, msg, msg_is_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWidget, OnMsgCreateWidget)
    IPC_MESSAGE_HANDLER(ViewHostMsg_OpenChannelToExtension,
                        OnMsgOpenChannelToExtension)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetDefaultPrintSettings,
                        OnGetDefaultPrintSettings)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ScriptedPrint,
                        OnScriptedPrint)
#if defined(OS_WIN) || defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidGetPrintedPagesCount,
                        OnDidGetPrintedPagesCount)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidPrintPage, OnDidPrintPage)
#endif
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DuplicateSection, OnDuplicateSection)
#endif
#if defined(OS_POSIX)
    IPC_MESSAGE_HANDLER(ViewHostMsg_AllocateSharedMemoryBuffer,
                        OnAllocateSharedMemoryBuffer)
#endif
#if defined(OS_CHROMEOS)
    IPC_MESSAGE_HANDLER(ViewHostMsg_AllocateTempFileForPrinting,
                        OnAllocateTempFileForPrinting)
    IPC_MESSAGE_HANDLER(ViewHostMsg_TempFileForPrintingWritten,
                        OnTempFileForPrintingWritten)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

// The Widget expects to be returned valid route_id.
void MockRenderThread::OnMsgCreateWidget(int opener_id,
                                         WebKit::WebPopupType popup_type,
                                         int* route_id) {
  opener_id_ = opener_id;
  *route_id = routing_id_;
}

void MockRenderThread::OnMsgOpenChannelToExtension(
    int routing_id, const std::string& source_extension_id,
    const std::string& target_extension_id,
    const std::string& channel_name, int* port_id) {
  *port_id = 0;
}

#if defined(OS_WIN)
void MockRenderThread::OnDuplicateSection(
    base::SharedMemoryHandle renderer_handle,
    base::SharedMemoryHandle* browser_handle) {
  // We don't have to duplicate the input handles since RenderViewTest does not
  // separate a browser process from a renderer process.
  *browser_handle = renderer_handle;
}
#endif

#if defined(OS_POSIX)
void MockRenderThread::OnAllocateSharedMemoryBuffer(
    uint32 buffer_size, base::SharedMemoryHandle* handle) {
  base::SharedMemory shared_buf;
  if (!shared_buf.CreateAndMapAnonymous(buffer_size)) {
    *handle = base::SharedMemory::NULLHandle();
    NOTREACHED() << "Cannot map shared memory buffer";
    return;
  }
  shared_buf.GiveToProcess(base::GetCurrentProcessHandle(), handle);
}
#endif

#if defined(OS_CHROMEOS)
void MockRenderThread::OnAllocateTempFileForPrinting(
    base::FileDescriptor* renderer_fd,
    int* browser_fd) {
  renderer_fd->fd = *browser_fd = -1;
  renderer_fd->auto_close = false;

  FilePath path;
  if (file_util::CreateTemporaryFile(&path)) {
    int fd = open(path.value().c_str(), O_WRONLY);
    DCHECK_GE(fd, 0);
    renderer_fd->fd = *browser_fd = fd;
  }
}

void MockRenderThread::OnTempFileForPrintingWritten(int browser_fd) {
  close(browser_fd);
}
#endif  // defined(OS_CHROMEOS)

void MockRenderThread::OnGetDefaultPrintSettings(ViewMsg_Print_Params* params) {
  if (printer_.get())
    printer_->GetDefaultPrintSettings(params);
}

void MockRenderThread::OnScriptedPrint(
    const ViewHostMsg_ScriptedPrint_Params& params,
    ViewMsg_PrintPages_Params* settings) {
  if (printer_.get()) {
    printer_->ScriptedPrint(params.cookie,
                            params.expected_pages_count,
                            params.has_selection,
                            settings);
  }
}

void MockRenderThread::OnDidGetPrintedPagesCount(int cookie, int number_pages) {
  if (printer_.get())
    printer_->SetPrintedPagesCount(cookie, number_pages);
}

void MockRenderThread::OnDidPrintPage(
    const ViewHostMsg_DidPrintPage_Params& params) {
  if (printer_.get())
    printer_->PrintPage(params);
}
