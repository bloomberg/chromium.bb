// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_devtools_proxy.h"

#include "base/trace_event/trace_event.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace content {

namespace {

void AddErrorMessageToConsoleOnUI(
    base::RepeatingCallback<int(void)> frame_tree_node_id_getter,
    std::string error_message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  WebContents* web_contents =
      WebContents::FromFrameTreeNodeId(frame_tree_node_id_getter.Run());
  if (!web_contents)
    return;
  web_contents->GetMainFrame()->AddMessageToConsole(
      content::CONSOLE_MESSAGE_LEVEL_ERROR, error_message);
}

}  // namespace

SignedExchangeDevToolsProxy::SignedExchangeDevToolsProxy(
    base::RepeatingCallback<int(void)> frame_tree_node_id_getter)
    : frame_tree_node_id_getter_(frame_tree_node_id_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

SignedExchangeDevToolsProxy::~SignedExchangeDevToolsProxy() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void SignedExchangeDevToolsProxy::ReportErrorMessage(
    const std::string& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&AddErrorMessageToConsoleOnUI, frame_tree_node_id_getter_,
                     std::move(message)));
}

}  // namespace content
