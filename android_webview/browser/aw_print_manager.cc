// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_print_manager.h"

#include "components/printing/browser/print_manager_utils.h"
#include "components/printing/common/print_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(android_webview::AwPrintManager);

namespace android_webview {

// static
AwPrintManager* AwPrintManager::CreateForWebContents(
    content::WebContents* contents,
    const printing::PrintSettings& settings,
    const base::FileDescriptor& file_descriptor,
    const PrintManager::PdfWritingDoneCallback& callback) {
  AwPrintManager* print_manager =
      new AwPrintManager(contents, settings, file_descriptor, callback);
  contents->SetUserData(UserDataKey(), print_manager);
  return print_manager;
}

AwPrintManager::AwPrintManager(
    content::WebContents* contents,
    const printing::PrintSettings& settings,
    const base::FileDescriptor& file_descriptor,
    const PdfWritingDoneCallback& callback)
    : PrintManager(contents),
      settings_(settings) {
  set_file_descriptor(file_descriptor);
  pdf_writing_done_callback_ = callback;
  cookie_ = 1;
}

AwPrintManager::~AwPrintManager() {
}

bool AwPrintManager::PrintNow() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* rfh = web_contents()->GetMainFrame();
  return rfh->Send(new PrintMsg_PrintPages(rfh->GetRoutingID()));
}

bool AwPrintManager::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(AwPrintManager, message, render_frame_host)
    IPC_MESSAGE_HANDLER_WITH_PARAM_DELAY_REPLY(
        PrintHostMsg_GetDefaultPrintSettings, OnGetDefaultPrintSettings)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled ? true
                 : PrintManager::OnMessageReceived(message, render_frame_host);
}

void AwPrintManager::OnGetDefaultPrintSettings(
    content::RenderFrameHost* render_frame_host,
    IPC::Message* reply_msg) {
  // Unlike the printing_message_filter, we do process this in UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PrintMsg_Print_Params params;
  printing::RenderParamsFromPrintSettings(settings_, &params);
  params.document_cookie = cookie_;
  PrintHostMsg_GetDefaultPrintSettings::WriteReplyParams(reply_msg, params);
  render_frame_host->Send(reply_msg);
}

}  // namespace android_webview
