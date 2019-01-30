// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/media_web_contents_observer_android.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/media/media_player_delegate_messages.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/android/media_player_android.h"

namespace content {

static void SuspendAllMediaPlayersInRenderFrame(
    RenderFrameHost* render_frame_host) {
  render_frame_host->Send(new MediaPlayerDelegateMsg_SuspendAllMediaPlayers(
      render_frame_host->GetRoutingID()));
}

MediaWebContentsObserverAndroid::MediaWebContentsObserverAndroid(
    WebContents* web_contents)
    : MediaWebContentsObserver(web_contents) {}

MediaWebContentsObserverAndroid::~MediaWebContentsObserverAndroid() {}

// static
MediaWebContentsObserverAndroid*
MediaWebContentsObserverAndroid::FromWebContents(WebContents* web_contents) {
  return static_cast<MediaWebContentsObserverAndroid*>(
      static_cast<WebContentsImpl*>(web_contents)
          ->media_web_contents_observer());
}

void MediaWebContentsObserverAndroid::SuspendAllMediaPlayers() {
  web_contents()->ForEachFrame(
      base::BindRepeating(&SuspendAllMediaPlayersInRenderFrame));
}
}  // namespace content
