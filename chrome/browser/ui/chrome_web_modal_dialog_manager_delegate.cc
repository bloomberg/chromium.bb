// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"

#include "chrome/browser/platform_util.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

ChromeWebModalDialogManagerDelegate::ChromeWebModalDialogManagerDelegate() {
}

ChromeWebModalDialogManagerDelegate::~ChromeWebModalDialogManagerDelegate() {
}

void ChromeWebModalDialogManagerDelegate::SetWebContentsBlocked(
    content::WebContents* web_contents,
    bool blocked) {
  // RenderViewHost may be NULL during shutdown.
  content::RenderViewHost* host = web_contents->GetRenderViewHost();
  if (host) {
    host->Send(new ChromeViewMsg_SetVisuallyDeemphasized(
        host->GetRoutingID(), blocked));
  }
}

bool ChromeWebModalDialogManagerDelegate::IsWebContentsVisible(
    content::WebContents* web_contents) {
  return platform_util::IsVisible(web_contents->GetView()->GetNativeView());
}
