// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PRERENDER_PRERENDER_WEBMEDIAPLAYER_H_
#define CHROME_RENDERER_PRERENDER_PRERENDER_WEBMEDIAPLAYER_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/renderer/render_view_observer.h"
#include "webkit/media/webmediaplayer_impl.h"

namespace webkit_media {
class MediaStreamClient;
class WebMediaPlayerDelegate;
}

namespace prerender {

// Substitute for WebMediaPlayerImpl to be used in prerendered pages. Defers
// the loading of the media till the prerendered page is swapped in.
class PrerenderWebMediaPlayer
    : public content::RenderViewObserver,
      public webkit_media::WebMediaPlayerImpl {
 public:
  PrerenderWebMediaPlayer(
      content::RenderView* render_view,
      WebKit::WebFrame* frame,
      WebKit::WebMediaPlayerClient* client,
      base::WeakPtr<webkit_media::WebMediaPlayerDelegate> delegate,
      media::FilterCollection* collection,
      WebKit::WebAudioSourceProvider* audio_source_provider,
      media::MessageLoopFactory* message_loop_factory,
      webkit_media::MediaStreamClient* media_stream_client,
      media::MediaLog* media_log);
  virtual ~PrerenderWebMediaPlayer();

  // WebMediaPlayerImpl methods:
  virtual void load(const WebKit::WebURL& url, CORSMode cors_mode) OVERRIDE;
  virtual void cancelLoad() OVERRIDE;

 private:
  // RenderViewObserver method:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnSetIsPrerendering(bool is_prerendering);

  bool is_prerendering_;
  bool url_loaded_;
  scoped_ptr<WebKit::WebURL> url_to_load_;
  CORSMode cors_mode_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderWebMediaPlayer);
};

}  // namespace prerender

#endif  // CHROME_RENDERER_PRERENDER_PRERENDER_WEBMEDIAPLAYER_H_
