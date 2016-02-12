// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/shell_content_renderer_client.h"

#include "base/command_line.h"
#include "components/web_cache/renderer/web_cache_render_process_observer.h"
#include "content/public/renderer/render_thread.h"
#include "content/shell/renderer/shell_render_view_observer.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "v8/include/v8.h"

#if defined(ENABLE_PLUGINS)
#include "ppapi/shared_impl/ppapi_switches.h"
#endif

namespace content {

namespace {

// This is the public key which the content shell will use to enable origin
// trial features.
// TODO(iclelland): Update this comment with the location of the public and
// private key files when the command-line tool CL lands
static const uint8_t kOriginTrialPublicKey[] = {
    0x75, 0x10, 0xac, 0xf9, 0x3a, 0x1c, 0xb8, 0xa9, 0x28, 0x70, 0xd2,
    0x9a, 0xd0, 0x0b, 0x59, 0xe1, 0xac, 0x2b, 0xb7, 0xd5, 0xca, 0x1f,
    0x64, 0x90, 0x08, 0x8e, 0xa8, 0xe0, 0x56, 0x3a, 0x04, 0xd0,
};
}  // namespace

ShellContentRendererClient::ShellContentRendererClient()
    : origin_trial_public_key_(base::StringPiece(
          reinterpret_cast<const char*>(kOriginTrialPublicKey),
          arraysize(kOriginTrialPublicKey))) {}

ShellContentRendererClient::~ShellContentRendererClient() {
}

void ShellContentRendererClient::RenderThreadStarted() {
  RenderThread* thread = RenderThread::Get();
  web_cache_observer_.reset(new web_cache::WebCacheRenderProcessObserver());
  thread->AddObserver(web_cache_observer_.get());
}

void ShellContentRendererClient::RenderViewCreated(RenderView* render_view) {
  new ShellRenderViewObserver(render_view);
}

bool ShellContentRendererClient::IsPluginAllowedToUseCompositorAPI(
    const GURL& url) {
#if defined(ENABLE_PLUGINS)
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnablePepperTesting);
#else
  return false;
#endif
}

bool ShellContentRendererClient::IsPluginAllowedToUseDevChannelAPIs() {
#if defined(ENABLE_PLUGINS)
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnablePepperTesting);
#else
  return false;
#endif
}

base::StringPiece ShellContentRendererClient::GetOriginTrialPublicKey() {
  return origin_trial_public_key_;
}

}  // namespace content
