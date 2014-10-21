// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_JAVASCRIPT_DIALOG_HELPER_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_JAVASCRIPT_DIALOG_HELPER_H_

#include "content/public/browser/javascript_dialog_manager.h"

namespace extensions {

class WebViewGuest;

class JavaScriptDialogHelper : public content::JavaScriptDialogManager {
 public:
  explicit JavaScriptDialogHelper(WebViewGuest* guest);
  ~JavaScriptDialogHelper() override;

  // JavaScriptDialogManager implementation.
  void RunJavaScriptDialog(
      content::WebContents* web_contents,
      const GURL& origin_url,
      const std::string& accept_lang,
      content::JavaScriptMessageType javascript_message_type,
      const base::string16& message_text,
      const base::string16& default_prompt_text,
      const DialogClosedCallback& callback,
      bool* did_suppress_message) override;
  void RunBeforeUnloadDialog(content::WebContents* web_contents,
                             const base::string16& message_text,
                             bool is_reload,
                             const DialogClosedCallback& callback) override;
  bool HandleJavaScriptDialog(content::WebContents* web_contents,
                              bool accept,
                              const base::string16* prompt_override) override;
  void CancelActiveAndPendingDialogs(
      content::WebContents* web_contents) override;
  void WebContentsDestroyed(content::WebContents* web_contents) override;

 private:
  void OnPermissionResponse(
      const DialogClosedCallback& callback,
      bool allow,
      const std::string& user_input);

  // Pointer to the webview that is being helped.
  WebViewGuest* const web_view_guest_;

  DISALLOW_COPY_AND_ASSIGN(JavaScriptDialogHelper);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_JAVASCRIPT_DIALOG_HELPER_H_
