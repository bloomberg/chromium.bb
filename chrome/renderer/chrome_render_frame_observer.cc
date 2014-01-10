// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_render_frame_observer.h"

#include "chrome/common/prerender_messages.h"
#include "chrome/renderer/prerender/prerender_helper.h"

ChromeRenderFrameObserver::ChromeRenderFrameObserver(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {
}

ChromeRenderFrameObserver::~ChromeRenderFrameObserver() {
}

bool ChromeRenderFrameObserver::OnMessageReceived(const IPC::Message& message) {
  // Filter only.
  IPC_BEGIN_MESSAGE_MAP(ChromeRenderFrameObserver, message)
    IPC_MESSAGE_HANDLER(PrerenderMsg_SetIsPrerendering, OnSetIsPrerendering);
  IPC_END_MESSAGE_MAP()

  return false;
}

void ChromeRenderFrameObserver::OnSetIsPrerendering(bool is_prerendering) {
  if (is_prerendering) {
    // If the PrerenderHelper for this frame already exists, don't create it. It
    // can already be created for subframes during handling of
    // RenderFrameCreated, if the parent frame was prerendering at time of
    // subframe creation.
    if (prerender::PrerenderHelper::Get(render_frame()))
      return;

    // The PrerenderHelper will destroy itself either after recording histograms
    // or on destruction of the RenderView.
    new prerender::PrerenderHelper(render_frame());
  }
}
