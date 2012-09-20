// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_JAVASCRIPT_DIALOG_CREATOR_H_
#define ANDROID_WEBVIEW_NATIVE_AW_JAVASCRIPT_DIALOG_CREATOR_H_

#include "content/public/browser/javascript_dialogs.h"

namespace android_webview {

class AwJavaScriptDialogCreator : public content::JavaScriptDialogCreator {
 public:
  explicit AwJavaScriptDialogCreator();
  virtual ~AwJavaScriptDialogCreator();

  // Overridden from content::JavaScriptDialogCreator:
  virtual void RunJavaScriptDialog(
      content::WebContents* web_contents,
      const GURL& origin_url,
      const std::string& accept_lang,
      content::JavaScriptMessageType message_type,
      const string16& message_text,
      const string16& default_prompt_text,
      const DialogClosedCallback& callback,
      bool* did_suppress_message) OVERRIDE;
  virtual void RunBeforeUnloadDialog(
      content::WebContents* web_contents,
      const string16& message_text,
      bool is_reload,
      const DialogClosedCallback& callback) OVERRIDE;
  virtual void ResetJavaScriptState(
      content::WebContents* web_contents) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AwJavaScriptDialogCreator);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_AW_JAVASCRIPT_DIALOG_CREATOR_H_
