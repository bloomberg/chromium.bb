// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/prerender/prerender_media_load_deferrer.h"

#include "base/callback_helpers.h"
#include "chrome/common/prerender_messages.h"

namespace prerender {

PrerenderMediaLoadDeferrer::PrerenderMediaLoadDeferrer(
    content::RenderFrame* render_frame,
    const base::Closure& closure)
    : RenderFrameObserver(render_frame),
      is_prerendering_(true),
      continue_loading_cb_(closure) {
  DCHECK(!continue_loading_cb_.is_null());
}

PrerenderMediaLoadDeferrer::~PrerenderMediaLoadDeferrer() {}

bool PrerenderMediaLoadDeferrer::OnMessageReceived(
    const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(PrerenderMediaLoadDeferrer, message)
    IPC_MESSAGE_HANDLER(PrerenderMsg_SetIsPrerendering, OnSetIsPrerendering)
  IPC_END_MESSAGE_MAP()

  return false;
}

void PrerenderMediaLoadDeferrer::OnSetIsPrerendering(bool is_prerendering) {
  // Prerendering can only be enabled prior to a RenderFrame's first
  // navigation, so no PrerenderMediaLoadDeferrer should see the notification
  // that enables prerendering.
  DCHECK(!is_prerendering);
  if (!is_prerendering_ || is_prerendering)
    return;

  is_prerendering_ = false;
  base::ResetAndReturn(&continue_loading_cb_).Run();
}

}  // namespace prerender
