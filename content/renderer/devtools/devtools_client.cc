// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/devtools/devtools_client.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/devtools_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDevToolsFrontend.h"
#include "ui/base/ui_base_switches.h"

using blink::WebDevToolsFrontend;
using blink::WebString;

namespace content {

DevToolsClient::DevToolsClient(RenderFrame* render_frame,
                               const std::string& api_script)
    : RenderFrameObserver(render_frame), api_script_(api_script) {
  if (render_frame->IsMainFrame()) {
    web_tools_frontend_.reset(
        WebDevToolsFrontend::Create(render_frame->GetWebFrame(), this));
  }
}

DevToolsClient::~DevToolsClient() {
}

void DevToolsClient::DidClearWindowObject() {
  if (!api_script_.empty())
    render_frame()->ExecuteJavaScript(base::UTF8ToUTF16(api_script_));
}

void DevToolsClient::OnDestruct() {
  delete this;
}

void DevToolsClient::SendMessageToEmbedder(const WebString& message) {
  Send(new DevToolsHostMsg_DispatchOnEmbedder(routing_id(), message.Utf8()));
}

bool DevToolsClient::IsUnderTest() {
  return RenderThreadImpl::current()->layout_test_mode();
}

}  // namespace content
