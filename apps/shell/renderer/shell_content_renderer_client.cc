// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/renderer/shell_content_renderer_client.h"

#include "apps/shell/common/shell_extensions_client.h"
#include "chrome/renderer/extensions/dispatcher.h"
#include "chrome/renderer/extensions/extension_helper.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/extensions_client.h"

using blink::WebFrame;
using blink::WebString;
using content::RenderThread;

namespace apps {

ShellContentRendererClient::ShellContentRendererClient() {}

ShellContentRendererClient::~ShellContentRendererClient() {}

void ShellContentRendererClient::RenderThreadStarted() {
  RenderThread* thread = RenderThread::Get();

  extension_dispatcher_.reset(new extensions::Dispatcher());
  thread->AddObserver(extension_dispatcher_.get());

  // TODO(jamescook): Init WebSecurityPolicy for chrome-extension: schemes.
  // See ChromeContentRendererClient for details.

  extensions_client_.reset(new ShellExtensionsClient);
  extensions::ExtensionsClient::Set(extensions_client_.get());
}

void ShellContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  // TODO(jamescook): Create ExtensionFrameHelper? This might be needed for
  // Pepper plugins like Flash.
}

void ShellContentRendererClient::RenderViewCreated(
    content::RenderView* render_view) {
  new extensions::ExtensionHelper(render_view, extension_dispatcher_.get());
}

bool ShellContentRendererClient::WillSendRequest(
    blink::WebFrame* frame,
    content::PageTransition transition_type,
    const GURL& url,
    const GURL& first_party_for_cookies,
    GURL* new_url) {
  // TODO(jamescook): Cause an error for bad extension scheme requests?
  return false;
}

void ShellContentRendererClient::DidCreateScriptContext(
    WebFrame* frame, v8::Handle<v8::Context> context, int extension_group,
    int world_id) {
  extension_dispatcher_->DidCreateScriptContext(
      frame, context, extension_group, world_id);
}

void ShellContentRendererClient::WillReleaseScriptContext(
    WebFrame* frame, v8::Handle<v8::Context> context, int world_id) {
  extension_dispatcher_->WillReleaseScriptContext(frame, context, world_id);
}

bool ShellContentRendererClient::ShouldEnableSiteIsolationPolicy() const {
  // Extension renderers don't need site isolation.
  return false;
}

}  // namespace apps
