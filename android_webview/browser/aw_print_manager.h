// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_PRINT_MANAGER_H_
#define ANDROID_WEBVIEW_BROWSER_PRINT_MANAGER_H_

#include "components/printing/browser/print_manager.h"
#include "content/public/browser/web_contents_user_data.h"
#include "printing/print_settings.h"

namespace android_webview {

class AwPrintManager : public printing::PrintManager,
    public content::WebContentsUserData<AwPrintManager> {
 public:
  // Creates a AwPrintManager for the provided webcontents. If the
  // AwPrintManager already exists, it is destroyed and a new one is created.
  static AwPrintManager* CreateForWebContents(
      content::WebContents* contents,
      const printing::PrintSettings& settings,
      const base::FileDescriptor& file_descriptor,
      const PdfWritingDoneCallback& callback);

  ~AwPrintManager() override;

  bool PrintNow();

 private:
  friend class content::WebContentsUserData<AwPrintManager>;
  AwPrintManager(content::WebContents* contents,
                 const printing::PrintSettings& settings,
                 const base::FileDescriptor& file_descriptor,
                 const PdfWritingDoneCallback& callback);

  bool OnMessageReceived(const IPC::Message& message) override;
  void OnGetDefaultPrintSettings(IPC::Message* reply_msg);

  printing::PrintSettings settings_;

  DISALLOW_COPY_AND_ASSIGN(AwPrintManager);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_PRINT_MANAGER_H_
