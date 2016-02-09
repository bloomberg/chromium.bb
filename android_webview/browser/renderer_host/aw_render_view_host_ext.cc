// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/renderer_host/aw_render_view_host_ext.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/scoped_allow_wait_for_legacy_web_view_api.h"
#include "android_webview/common/render_view_messages.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"

namespace android_webview {

AwRenderViewHostExt::AwRenderViewHostExt(
    AwRenderViewHostExtClient* client, content::WebContents* contents)
    : content::WebContentsObserver(contents),
      client_(client),
      background_color_(SK_ColorWHITE),
      has_new_hit_test_data_(false) {
  DCHECK(client_);
}

AwRenderViewHostExt::~AwRenderViewHostExt() {}

void AwRenderViewHostExt::DocumentHasImages(DocumentHasImagesResult result) {
  DCHECK(CalledOnValidThread());
  if (!web_contents()->GetRenderViewHost()) {
    result.Run(false);
    return;
  }
  static int next_id = 1;
  int this_id = next_id++;
  pending_document_has_images_requests_[this_id] = result;
  // Send the message to the main frame, instead of the whole frame tree,
  // because it only makes sense on the main frame.
  // TODO(hush): deal with the case where the receiving RenderView is gone or
  // inactive.
  // crbug.com/570906
  Send(new AwViewMsg_DocumentHasImages(
      web_contents()->GetMainFrame()->GetRoutingID(), this_id));
}

void AwRenderViewHostExt::ClearCache() {
  DCHECK(CalledOnValidThread());
  Send(new AwViewMsg_ClearCache);
}

bool AwRenderViewHostExt::HasNewHitTestData() const {
  return has_new_hit_test_data_;
}

void AwRenderViewHostExt::MarkHitTestDataRead() {
  has_new_hit_test_data_ = false;
}

void AwRenderViewHostExt::RequestNewHitTestDataAt(
    const gfx::PointF& touch_center,
    const gfx::SizeF& touch_area) {
  DCHECK(CalledOnValidThread());
  // We only need to get blink::WebView on the renderer side to invoke the
  // blink hit test API, so sending this IPC to main frame is enough.
  Send(new AwViewMsg_DoHitTest(web_contents()->GetMainFrame()->GetRoutingID(),
                               touch_center, touch_area));
}

const AwHitTestData& AwRenderViewHostExt::GetLastHitTestData() const {
  DCHECK(CalledOnValidThread());
  return last_hit_test_data_;
}

void AwRenderViewHostExt::SetTextZoomFactor(float factor) {
  DCHECK(CalledOnValidThread());
  Send(new AwViewMsg_SetTextZoomFactor(
      web_contents()->GetMainFrame()->GetRoutingID(), factor));
}

void AwRenderViewHostExt::ResetScrollAndScaleState() {
  DCHECK(CalledOnValidThread());
  Send(new AwViewMsg_ResetScrollAndScaleState(
      web_contents()->GetMainFrame()->GetRoutingID()));
}

void AwRenderViewHostExt::SetInitialPageScale(double page_scale_factor) {
  DCHECK(CalledOnValidThread());
  Send(new AwViewMsg_SetInitialPageScale(
      web_contents()->GetMainFrame()->GetRoutingID(), page_scale_factor));
}

void AwRenderViewHostExt::SetBackgroundColor(SkColor c) {
  if (background_color_ == c)
    return;
  background_color_ = c;
  if (web_contents()->GetRenderViewHost()) {
    Send(new AwViewMsg_SetBackgroundColor(
        web_contents()->GetMainFrame()->GetRoutingID(), background_color_));
  }
}

void AwRenderViewHostExt::SetJsOnlineProperty(bool network_up) {
  Send(new AwViewMsg_SetJsOnlineProperty(network_up));
}

void AwRenderViewHostExt::SmoothScroll(int target_x,
                                       int target_y,
                                       long duration_ms) {
  Send(
      new AwViewMsg_SmoothScroll(web_contents()->GetMainFrame()->GetRoutingID(),
                                 target_x, target_y,
                                 static_cast<int>(duration_ms)));
}

void AwRenderViewHostExt::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  Send(new AwViewMsg_SetBackgroundColor(
      web_contents()->GetMainFrame()->GetRoutingID(), background_color_));
}

void AwRenderViewHostExt::RenderProcessGone(base::TerminationStatus status) {
  DCHECK(CalledOnValidThread());
  for (std::map<int, DocumentHasImagesResult>::iterator pending_req =
           pending_document_has_images_requests_.begin();
       pending_req != pending_document_has_images_requests_.end();
      ++pending_req) {
    pending_req->second.Run(false);
  }
}

void AwRenderViewHostExt::DidNavigateAnyFrame(
    content::RenderFrameHost* render_frame_host,
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  DCHECK(CalledOnValidThread());

  AwBrowserContext::FromWebContents(web_contents())
      ->AddVisitedURLs(params.redirects);
}

void AwRenderViewHostExt::OnPageScaleFactorChanged(float page_scale_factor) {
  client_->OnWebLayoutPageScaleFactorChanged(page_scale_factor);
}

bool AwRenderViewHostExt::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(AwRenderViewHostExt, message,
                                   render_frame_host)
    IPC_MESSAGE_HANDLER(AwViewHostMsg_DocumentHasImagesResponse,
                        OnDocumentHasImagesResponse)
    IPC_MESSAGE_HANDLER(AwViewHostMsg_UpdateHitTestData,
                        OnUpdateHitTestData)
    IPC_MESSAGE_HANDLER(AwViewHostMsg_OnContentsSizeChanged,
                        OnContentsSizeChanged)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled ? true : WebContentsObserver::OnMessageReceived(message);
}

void AwRenderViewHostExt::OnDocumentHasImagesResponse(
    content::RenderFrameHost* render_frame_host,
    int msg_id,
    bool has_images) {
  // Only makes sense coming from the main frame of the current frame tree.
  // This matches the current implementation that only cares about if there is
  // an img child node in the main document, and essentially invokes JS:
  // node.getElementsByTagName("img").
  if (render_frame_host != web_contents()->GetMainFrame())
    return;

  DCHECK(CalledOnValidThread());
  std::map<int, DocumentHasImagesResult>::iterator pending_req =
      pending_document_has_images_requests_.find(msg_id);
  if (pending_req == pending_document_has_images_requests_.end()) {
    DLOG(WARNING) << "unexpected DocumentHasImages Response: " << msg_id;
  } else {
    pending_req->second.Run(has_images);
    pending_document_has_images_requests_.erase(pending_req);
  }
}

void AwRenderViewHostExt::OnUpdateHitTestData(
    content::RenderFrameHost* render_frame_host,
    const AwHitTestData& hit_test_data) {
  content::RenderFrameHost* main_frame_host = render_frame_host;
  while (main_frame_host->GetParent())
    main_frame_host = main_frame_host->GetParent();

  // Make sense from any frame of the current frame tree, because a focused
  // node could be in either the mainframe or a subframe.
  if (main_frame_host != web_contents()->GetMainFrame())
    return;

  DCHECK(CalledOnValidThread());
  last_hit_test_data_ = hit_test_data;
  has_new_hit_test_data_ = true;
}

void AwRenderViewHostExt::OnContentsSizeChanged(
    content::RenderFrameHost* render_frame_host,
    const gfx::Size& contents_size) {
  // Only makes sense coming from the main frame of the current frame tree.
  if (render_frame_host != web_contents()->GetMainFrame())
    return;

  client_->OnWebLayoutContentsSizeChanged(contents_size);
}

}  // namespace android_webview
