// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RENDERER_WEBMEDIAPLAYER_DELEGATE_H_
#define CONTENT_RENDERER_MEDIA_RENDERER_WEBMEDIAPLAYER_DELEGATE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "content/public/renderer/render_frame_observer.h"
#include "media/blink/webmediaplayer_delegate.h"

namespace blink {
class WebMediaPlayer;
}

namespace media {

// An interface to allow a WebMediaPlayerImpl to communicate changes of state
// to objects that need to know.
class RendererWebMediaPlayerDelegate
    : public content::RenderFrameObserver,
      public WebMediaPlayerDelegate,
      public base::SupportsWeakPtr<RendererWebMediaPlayerDelegate> {
 public:
  explicit RendererWebMediaPlayerDelegate(content::RenderFrame* render_frame);
  ~RendererWebMediaPlayerDelegate() override;

  // Returns true if this RenderFrame has ever seen media playback before.
  bool has_played_media() const { return has_played_media_; }

  // WebMediaPlayerDelegate implementation.
  void DidPlay(blink::WebMediaPlayer* player) override;
  void DidPause(blink::WebMediaPlayer* player) override;
  void PlayerGone(blink::WebMediaPlayer* player) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool IsHidden() override;

  // content::RenderFrameObserver overrides.
  void WasHidden() override;
  void WasShown() override;

 private:
  bool has_played_media_ = false;
  base::ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(RendererWebMediaPlayerDelegate);
};

}  // namespace media

#endif  // CONTENT_RENDERER_MEDIA_RENDERER_WEBMEDIAPLAYER_DELEGATE_H_
