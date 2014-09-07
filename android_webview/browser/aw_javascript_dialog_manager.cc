// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_javascript_dialog_manager.h"

#include "android_webview/browser/aw_contents_client_bridge_base.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/web_contents.h"

namespace android_webview {

AwJavaScriptDialogManager::AwJavaScriptDialogManager() {}

AwJavaScriptDialogManager::~AwJavaScriptDialogManager() {}

void AwJavaScriptDialogManager::RunJavaScriptDialog(
    content::WebContents* web_contents,
    const GURL& origin_url,
    const std::string& accept_lang,
    content::JavaScriptMessageType message_type,
    const base::string16& message_text,
    const base::string16& default_prompt_text,
    const DialogClosedCallback& callback,
    bool* did_suppress_message) {
  AwContentsClientBridgeBase* bridge =
      AwContentsClientBridgeBase::FromWebContents(web_contents);
  if (!bridge) {
    callback.Run(false, base::string16());
    return;
  }

  bridge->RunJavaScriptDialog(message_type,
                              origin_url,
                              message_text,
                              default_prompt_text,
                              callback);
}

void AwJavaScriptDialogManager::RunBeforeUnloadDialog(
    content::WebContents* web_contents,
    const base::string16& message_text,
    bool is_reload,
    const DialogClosedCallback& callback) {
  AwContentsClientBridgeBase* bridge =
      AwContentsClientBridgeBase::FromWebContents(web_contents);
  if (!bridge) {
    callback.Run(false, base::string16());
    return;
  }

  bridge->RunBeforeUnloadDialog(web_contents->GetURL(),
                                message_text,
                                callback);
}

void AwJavaScriptDialogManager::CancelActiveAndPendingDialogs(
    content::WebContents* web_contents) {
}

void AwJavaScriptDialogManager::WebContentsDestroyed(
    content::WebContents* web_contents) {
}

}  // namespace android_webview
