// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/media_session_service_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "content/browser/media/android/browser_media_session_manager.h"
#include "content/browser/media/android/media_web_contents_observer_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/media_metadata.h"

namespace content {

MediaSessionServiceImpl::MediaSessionServiceImpl(
    RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host) {}

MediaSessionServiceImpl::~MediaSessionServiceImpl() = default;

// static
void MediaSessionServiceImpl::Create(
    RenderFrameHost* render_frame_host,
    blink::mojom::MediaSessionServiceRequest request) {
  MediaSessionServiceImpl* impl =
      new MediaSessionServiceImpl(render_frame_host);
  impl->Bind(std::move(request));
}

void MediaSessionServiceImpl::SetMetadata(
    const base::Optional<content::MediaMetadata>& metadata) {
  WebContentsImpl* contents = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(render_frame_host_));
  DCHECK(contents);
  MediaWebContentsObserverAndroid::FromWebContents(contents)
      ->GetMediaSessionManager(render_frame_host_)
      ->OnSetMetadata(0, metadata);
}

void MediaSessionServiceImpl::Bind(
    blink::mojom::MediaSessionServiceRequest request) {
  binding_.reset(new mojo::Binding<blink::mojom::MediaSessionService>(
      this, std::move(request)));
}

}  // namespace content
