// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/renderer_host/view_renderer_host.h"

#include "android_webview/browser/scoped_allow_wait_for_legacy_web_view_api.h"
#include "android_webview/common/aw_switches.h"
#include "android_webview/common/render_view_messages.h"
#include "android_webview/common/renderer_picture_map.h"
#include "base/command_line.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCompositorInputHandler.h"

namespace android_webview {

ViewRendererHost::ViewRendererHost(content::WebContents* contents,
                                   Client* client)
    : content::WebContentsObserver(contents),
      client_(client) {
}

ViewRendererHost::~ViewRendererHost() {
}

void ViewRendererHost::CapturePictureSync() {
  if (!IsRenderViewReady())
    return;

  ScopedAllowWaitForLegacyWebViewApi wait;
  Send(new AwViewMsg_CapturePictureSync(web_contents()->GetRoutingID()));
}

void ViewRendererHost::EnableCapturePictureCallback(bool enabled) {
  Send(new AwViewMsg_EnableCapturePictureCallback(
      web_contents()->GetRoutingID(), enabled));
}

void ViewRendererHost::OnPictureUpdated() {
  if (client_) {
    client_->OnPictureUpdated(web_contents()->GetRenderProcessHost()->GetID(),
        routing_id());
  }
}

void ViewRendererHost::OnDidActivateAcceleratedCompositing(
    int input_handler_id) {

  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kMergeUIAndRendererCompositorThreads)) {
    return;
  }

  // This call is only meaningful and thread-safe when the UI and renderer
  // compositor share the same thread.  Any other case will likely yield
  // terrible, terrible damage.
  WebKit::WebCompositorInputHandler* input_handler =
      WebKit::WebCompositorInputHandler::fromIdentifier(input_handler_id);
  if (!input_handler)
    return;

  content::ContentViewCore* content_view_core
      = content::ContentViewCore::FromWebContents(web_contents());
  if (content_view_core)
    content_view_core->SetInputHandler(input_handler);
}

void ViewRendererHost::RenderViewGone(base::TerminationStatus status) {
  DCHECK(CalledOnValidThread());
  RendererPictureMap::GetInstance()->ClearRendererPicture(
      web_contents()->GetRoutingID());
}

bool ViewRendererHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ViewRendererHost, message)
    IPC_MESSAGE_HANDLER(AwViewHostMsg_PictureUpdated,
                        OnPictureUpdated)
    IPC_MESSAGE_HANDLER(AwViewHostMsg_DidActivateAcceleratedCompositing,
                        OnDidActivateAcceleratedCompositing)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled ? true : WebContentsObserver::OnMessageReceived(message);
}

bool ViewRendererHost::IsRenderViewReady() const {
  return web_contents()->GetRenderProcessHost()->HasConnection() &&
      web_contents()->GetRenderViewHost() &&
      web_contents()->GetRenderViewHost()->IsRenderViewLive();
}

}  // namespace android_webview
