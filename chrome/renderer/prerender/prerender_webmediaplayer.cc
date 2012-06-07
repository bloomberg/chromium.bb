// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/prerender/prerender_webmediaplayer.h"

#include "chrome/common/prerender_messages.h"
#include "content/public/renderer/render_view.h"
#include "media/base/filter_collection.h"
#include "media/base/media_log.h"
#include "media/base/pipeline.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "webkit/media/webmediaplayer_delegate.h"

namespace prerender {

PrerenderWebMediaPlayer::PrerenderWebMediaPlayer(
    content::RenderView* render_view,
    WebKit::WebFrame* frame,
    WebKit::WebMediaPlayerClient* client,
    base::WeakPtr<webkit_media::WebMediaPlayerDelegate> delegate,
    media::FilterCollection* collection,
    WebKit::WebAudioSourceProvider* audio_source_provider,
    media::MessageLoopFactory* message_loop_factory,
    webkit_media::MediaStreamClient* media_stream_client,
    media::MediaLog* media_log)
    : RenderViewObserver(render_view),
      WebMediaPlayerImpl(frame,
                         client,
                         delegate,
                         collection,
                         audio_source_provider,
                         message_loop_factory,
                         media_stream_client,
                         media_log),
      is_prerendering_(true),
      url_loaded_(false),
      cors_mode_(CORSModeUnspecified) {
}

PrerenderWebMediaPlayer::~PrerenderWebMediaPlayer() {}

void PrerenderWebMediaPlayer::load(const WebKit::WebURL& url,
                                   CORSMode cors_mode) {
  DCHECK(!url_loaded_);
  if (is_prerendering_) {
    url_to_load_.reset(new WebKit::WebURL(url));
    cors_mode_ = cors_mode;
    return;
  }
  url_loaded_ = true;
  WebMediaPlayerImpl::load(url, cors_mode);
}

void PrerenderWebMediaPlayer::cancelLoad() {
  if (is_prerendering_) {
    url_to_load_.reset(NULL);
    cors_mode_ = CORSModeUnspecified;
    return;
  }
  WebMediaPlayerImpl::cancelLoad();
}

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
  if (is_prerendering_ && !is_prerendering) {
    is_prerendering_ = false;
    if (url_to_load_.get())
      load(*url_to_load_, cors_mode_);
  }
}

} // namespace prerender
