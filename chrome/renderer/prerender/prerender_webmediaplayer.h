// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PRERENDER_PRERENDER_WEBMEDIAPLAYER_H_
#define CHROME_RENDERER_PRERENDER_PRERENDER_WEBMEDIAPLAYER_H_

#include "base/callback.h"
#include "content/public/renderer/render_view_observer.h"

namespace prerender {

// Defers media player loading in prerendered pages until the prerendered page
// is swapped in.
//
// TODO(scherkus): Rename as this class no longer inherits WebMediaPlayer.
class PrerenderWebMediaPlayer : public content::RenderViewObserver {
 public:
  // Will run |closure| to continue loading the media resource once the page is
  // swapped in.
  PrerenderWebMediaPlayer(content::RenderView* render_view,
                          const base::Closure& closure);
  virtual ~PrerenderWebMediaPlayer();

 private:
  // RenderViewObserver method:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnSetIsPrerendering(bool is_prerendering);

  bool is_prerendering_;
  base::Closure continue_loading_cb_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderWebMediaPlayer);
};

}  // namespace prerender

#endif  // CHROME_RENDERER_PRERENDER_PRERENDER_WEBMEDIAPLAYER_H_
