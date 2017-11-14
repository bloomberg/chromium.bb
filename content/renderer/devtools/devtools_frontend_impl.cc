// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/devtools/devtools_frontend_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDevToolsFrontend.h"

namespace content {

// static
void DevToolsFrontendImpl::CreateMojoService(
    RenderFrame* render_frame,
    mojom::DevToolsFrontendAssociatedRequest request) {
  // Self-destructs on render frame deletion.
  new DevToolsFrontendImpl(render_frame, std::move(request));
}

DevToolsFrontendImpl::DevToolsFrontendImpl(
    RenderFrame* render_frame,
    mojom::DevToolsFrontendAssociatedRequest request)
    : RenderFrameObserver(render_frame), binding_(this, std::move(request)) {}

DevToolsFrontendImpl::~DevToolsFrontendImpl() {}

void DevToolsFrontendImpl::DidClearWindowObject() {
  if (!api_script_.empty())
    render_frame()->ExecuteJavaScript(base::UTF8ToUTF16(api_script_));
}

void DevToolsFrontendImpl::OnDestruct() {
  delete this;
}

void DevToolsFrontendImpl::SendMessageToEmbedder(
    const blink::WebString& message) {
  if (host_)
    host_->DispatchEmbedderMessage(message.Utf8());
}

bool DevToolsFrontendImpl::IsUnderTest() {
  return RenderThreadImpl::current()->layout_test_mode();
}

void DevToolsFrontendImpl::SetupDevToolsFrontend(
    const std::string& api_script,
    mojom::DevToolsFrontendHostAssociatedPtrInfo host) {
  DCHECK(render_frame()->IsMainFrame());
  api_script_ = api_script;
  web_devtools_frontend_.reset(
      blink::WebDevToolsFrontend::Create(render_frame()->GetWebFrame(), this));
  host_.Bind(std::move(host));
  host_.set_connection_error_handler(base::BindOnce(
      &DevToolsFrontendImpl::OnDestruct, base::Unretained(this)));
}

void DevToolsFrontendImpl::SetupDevToolsExtensionAPI(
    const std::string& extension_api) {
  DCHECK(!render_frame()->IsMainFrame());
  api_script_ = extension_api;
}

}  // namespace content
