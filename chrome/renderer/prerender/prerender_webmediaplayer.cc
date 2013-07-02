// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/prerender/prerender_webmediaplayer.h"

#include "base/callback_helpers.h"
#include "chrome/common/prerender_messages.h"
#include "content/public/renderer/render_view.h"

namespace prerender {

PrerenderWebMediaPlayer::PrerenderWebMediaPlayer(
    content::RenderView* render_view,
    const base::Closure& closure)
    : RenderViewObserver(render_view),
      is_prerendering_(true),
      continue_loading_cb_(closure) {
  DCHECK(!continue_loading_cb_.is_null());
}

PrerenderWebMediaPlayer::~PrerenderWebMediaPlayer() {}

bool PrerenderWebMediaPlayer::OnMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(PrerenderWebMediaPlayer, message)
    IPC_MESSAGE_HANDLER(PrerenderMsg_SetIsPrerendering, OnSetIsPrerendering)
  IPC_END_MESSAGE_MAP()

  return false;
}

void PrerenderWebMediaPlayer::OnSetIsPrerendering(bool is_prerendering) {
  // Prerendering can only be enabled prior to a RenderView's first
  // navigation, so no PrerenderWebMediaPlayer should see the notification
  // that enables prerendering.
  DCHECK(!is_prerendering);
  if (!is_prerendering_ || is_prerendering)
    return;

  is_prerendering_ = false;
  base::ResetAndReturn(&continue_loading_cb_).Run();
}

}  // namespace prerender
