// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/mock_render_thread.h"

#include <fcntl.h>

#include "base/file_util.h"
#include "base/process_util.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/print_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/common/view_messages.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_sync_message.h"
#include "printing/print_job_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

MockRenderThread::MockRenderThread()
    : routing_id_(0),
      opener_id_(0),
      widget_(NULL),
      reply_deserializer_(NULL),
      printer_(new MockPrinter),
      print_dialog_user_response_(true),
      print_preview_cancel_page_number_(-1),
      print_preview_pages_remaining_(0) {
}

MockRenderThread::~MockRenderThread() {
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


bool MockRenderThread::IsIncognitoProcess() const {
  return false;
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
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_OpenChannelToExtension,
                        OnMsgOpenChannelToExtension)
    IPC_MESSAGE_HANDLER(PrintHostMsg_GetDefaultPrintSettings,
                        OnGetDefaultPrintSettings)
    IPC_MESSAGE_HANDLER(PrintHostMsg_ScriptedPrint, OnScriptedPrint)
    IPC_MESSAGE_HANDLER(PrintHostMsg_UpdatePrintSettings, OnUpdatePrintSettings)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidGetPrintedPagesCount,
                        OnDidGetPrintedPagesCount)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidPrintPage, OnDidPrintPage)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidGetPreviewPageCount,
                        OnDidGetPreviewPageCount)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidPreviewPage, OnDidPreviewPage)
    IPC_MESSAGE_HANDLER(PrintHostMsg_CheckForCancel, OnCheckForCancel)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DuplicateSection, OnDuplicateSection)
#endif
    IPC_MESSAGE_HANDLER(ViewHostMsg_AllocateSharedMemoryBuffer,
                        OnAllocateSharedMemoryBuffer)
#if defined(OS_CHROMEOS)
    IPC_MESSAGE_HANDLER(PrintHostMsg_AllocateTempFileForPrinting,
                        OnAllocateTempFileForPrinting)
    IPC_MESSAGE_HANDLER(PrintHostMsg_TempFileForPrintingWritten,
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
#endif  // defined(OS_WIN)

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

void MockRenderThread::OnGetDefaultPrintSettings(
    PrintMsg_Print_Params* params) {
  if (printer_.get())
    printer_->GetDefaultPrintSettings(params);
}

void MockRenderThread::OnScriptedPrint(
    const PrintHostMsg_ScriptedPrint_Params& params,
    PrintMsg_PrintPages_Params* settings) {
  if (print_dialog_user_response_ && printer_.get()) {
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
    const PrintHostMsg_DidPrintPage_Params& params) {
  if (printer_.get())
    printer_->PrintPage(params);
}

void MockRenderThread::OnDidGetPreviewPageCount(
    const PrintHostMsg_DidGetPreviewPageCount_Params& params) {
  print_preview_pages_remaining_ = params.page_count;
}

void MockRenderThread::OnDidPreviewPage(
    const PrintHostMsg_DidPreviewPage_Params& params) {
  DCHECK(params.page_number >= printing::FIRST_PAGE_INDEX);
  print_preview_pages_remaining_--;
}

void MockRenderThread::OnCheckForCancel(const std::string& preview_ui_addr,
                                        int preview_request_id,
                                        bool* cancel) {
  *cancel =
      (print_preview_pages_remaining_ == print_preview_cancel_page_number_);
}

void MockRenderThread::OnUpdatePrintSettings(
    int document_cookie,
    const DictionaryValue& job_settings,
    PrintMsg_PrintPages_Params* params) {
  // Check and make sure the required settings are all there.
  // We don't actually care about the values.
  std::string dummy_string;
  if (!job_settings.GetBoolean(printing::kSettingLandscape, NULL) ||
      !job_settings.GetBoolean(printing::kSettingCollate, NULL) ||
      !job_settings.GetBoolean(printing::kSettingColor, NULL) ||
      !job_settings.GetBoolean(printing::kSettingPrintToPDF, NULL) ||
      !job_settings.GetBoolean(printing::kIsFirstRequest, NULL) ||
      !job_settings.GetString(printing::kSettingDeviceName, &dummy_string) ||
      !job_settings.GetInteger(printing::kSettingDuplexMode, NULL) ||
      !job_settings.GetInteger(printing::kSettingCopies, NULL) ||
      !job_settings.GetString(printing::kPreviewUIAddr, &dummy_string) ||
      !job_settings.GetInteger(printing::kPreviewRequestID, NULL)) {
    return;
  }

  // Just return the default settings.
  if (printer_.get())
    printer_->UpdateSettings(document_cookie, params);
}

void MockRenderThread::set_print_dialog_user_response(bool response) {
  print_dialog_user_response_ = response;
}

void MockRenderThread::set_print_preview_cancel_page_number(int page) {
  print_preview_cancel_page_number_ = page;
}

int MockRenderThread::print_preview_pages_remaining() {
  return print_preview_pages_remaining_;
}
