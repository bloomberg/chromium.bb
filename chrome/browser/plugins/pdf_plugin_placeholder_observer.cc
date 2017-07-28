// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/pdf_plugin_placeholder_observer.h"

#include "chrome/common/render_messages.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(PDFPluginPlaceholderObserver);

PDFPluginPlaceholderObserver::PDFPluginPlaceholderObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

PDFPluginPlaceholderObserver::~PDFPluginPlaceholderObserver() {}

bool PDFPluginPlaceholderObserver::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(PDFPluginPlaceholderObserver, message,
                                   render_frame_host)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_OpenPDF, OnOpenPDF)
    IPC_MESSAGE_UNHANDLED(return false)
  IPC_END_MESSAGE_MAP()

  return true;
}

void PDFPluginPlaceholderObserver::OnOpenPDF(
    content::RenderFrameHost* render_frame_host,
    const GURL& url) {
  if (!content::ChildProcessSecurityPolicy::GetInstance()->CanRequestURL(
          render_frame_host->GetRoutingID(), url)) {
    return;
  }

  web_contents()->OpenURL(content::OpenURLParams(
      url,
      content::Referrer::SanitizeForRequest(
          url, content::Referrer(web_contents()->GetURL(),
                                 blink::kWebReferrerPolicyDefault)),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_AUTO_BOOKMARK,
      false));
}
