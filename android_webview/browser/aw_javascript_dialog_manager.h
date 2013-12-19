// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_JAVASCRIPT_DIALOG_MANAGER_H_
#define ANDROID_WEBVIEW_BROWSER_AW_JAVASCRIPT_DIALOG_MANAGER_H_

#include "content/public/browser/javascript_dialog_manager.h"

namespace android_webview {

class AwJavaScriptDialogManager : public content::JavaScriptDialogManager {
 public:
  explicit AwJavaScriptDialogManager();
  virtual ~AwJavaScriptDialogManager();

  // Overridden from content::JavaScriptDialogManager:
  virtual void RunJavaScriptDialog(
      content::WebContents* web_contents,
      const GURL& origin_url,
      const std::string& accept_lang,
      content::JavaScriptMessageType message_type,
      const base::string16& message_text,
      const base::string16& default_prompt_text,
      const DialogClosedCallback& callback,
      bool* did_suppress_message) OVERRIDE;
  virtual void RunBeforeUnloadDialog(
      content::WebContents* web_contents,
      const base::string16& message_text,
      bool is_reload,
      const DialogClosedCallback& callback) OVERRIDE;
  virtual void CancelActiveAndPendingDialogs(
      content::WebContents* web_contents) OVERRIDE;
  virtual void WebContentsDestroyed(
      content::WebContents* web_contents) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AwJavaScriptDialogManager);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_JAVASCRIPT_DIALOG_MANAGER_H_
