// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_printing_message_filter.h"

#include "android_webview/browser/renderer_host/print_manager.h"
#include "components/printing/common/print_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;
using content::WebContents;


namespace android_webview {

namespace {

PrintManager* GetPrintManager(int render_process_id, int render_view_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::RenderViewHost* rvh = content::RenderViewHost::FromID(
      render_process_id, render_view_id);
  if (rvh == nullptr)
    return nullptr;
  WebContents* web_contents = WebContents::FromRenderViewHost(rvh);
  if (web_contents == nullptr)
    return nullptr;
  return PrintManager::FromWebContents(web_contents);
}

} // namespace

AwPrintingMessageFilter::AwPrintingMessageFilter(int render_process_id)
    : BrowserMessageFilter(PrintMsgStart),
      render_process_id_(render_process_id) {
}

AwPrintingMessageFilter::~AwPrintingMessageFilter() {
}

void AwPrintingMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
  if (message.type() == PrintHostMsg_AllocateTempFileForPrinting::ID ||
      message.type() == PrintHostMsg_TempFileForPrintingWritten::ID) {
    *thread = BrowserThread::UI;
  }
}

bool AwPrintingMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AwPrintingMessageFilter, message)
    IPC_MESSAGE_HANDLER(PrintHostMsg_AllocateTempFileForPrinting,
                        OnAllocateTempFileForPrinting)
    IPC_MESSAGE_HANDLER(PrintHostMsg_TempFileForPrintingWritten,
                        OnTempFileForPrintingWritten)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AwPrintingMessageFilter::OnAllocateTempFileForPrinting(
    int render_view_id,
    base::FileDescriptor* temp_file_fd,
    int* sequence_number) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PrintManager* print_manager =
      GetPrintManager(render_process_id_, render_view_id);
  if (print_manager == nullptr)
    return;
  print_manager->OnAllocateTempFileForPrinting(temp_file_fd, sequence_number);
}

void AwPrintingMessageFilter::OnTempFileForPrintingWritten(
    int render_view_id,
    int sequence_number) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PrintManager* print_manager =
      GetPrintManager(render_process_id_, render_view_id);
  if (print_manager == nullptr)
    return;
  print_manager->OnTempFileForPrintingWritten(sequence_number);
}

}  // namespace android_webview
