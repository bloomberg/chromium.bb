// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_PRINT_MANAGER_H_
#define ANDROID_WEBVIEW_BROWSER_AW_PRINT_MANAGER_H_

#include "base/macros.h"
#include "components/printing/browser/print_manager.h"
#include "components/printing/common/print_messages.h"
#include "content/public/browser/web_contents_user_data.h"
#include "printing/print_settings.h"

namespace android_webview {

class AwPrintManager : public printing::PrintManager,
    public content::WebContentsUserData<AwPrintManager> {
 public:
  // Creates an AwPrintManager for the provided WebContents. If the
  // AwPrintManager already exists, it is destroyed and a new one is created.
  // The returned pointer is owned by |contents|.
  static AwPrintManager* CreateForWebContents(
      content::WebContents* contents,
      const printing::PrintSettings& settings,
      int file_descriptor,
      PdfWritingDoneCallback callback);

  ~AwPrintManager() override;

  // printing::PrintManager:
  void PdfWritingDone(int page_count) override;

  bool PrintNow();

 private:
  friend class content::WebContentsUserData<AwPrintManager>;
  struct FrameDispatchHelper;

  AwPrintManager(content::WebContents* contents,
                 const printing::PrintSettings& settings,
                 int file_descriptor,
                 PdfWritingDoneCallback callback);

  // printing::PrintManager:
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;

  // IPC Handlers
  void OnGetDefaultPrintSettings(content::RenderFrameHost* render_frame_host,
                                 IPC::Message* reply_msg);
  void OnScriptedPrint(content::RenderFrameHost* render_frame_host,
                       const PrintHostMsg_ScriptedPrint_Params& params,
                       IPC::Message* reply_msg);
  void OnDidPrintDocument(content::RenderFrameHost* render_frame_host,
                          const PrintHostMsg_DidPrintDocument_Params& params);

  printing::PrintSettings settings_;

  // The file descriptor into which the PDF of the document will be written.
  int fd_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(AwPrintManager);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_PRINT_MANAGER_H_
