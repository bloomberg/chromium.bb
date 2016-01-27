// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RENDERER_WEBMEDIAPLAYER_DELEGATE_H_
#define CONTENT_RENDERER_MEDIA_RENDERER_WEBMEDIAPLAYER_DELEGATE_H_

#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
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
  int AddObserver(Observer* observer) override;
  void RemoveObserver(int delegate_id) override;
  void DidPlay(int delegate_id,
               bool has_video,
               bool has_audio,
               bool is_remote,
               base::TimeDelta duration) override;
  void DidPause(int delegate_id, bool reached_end_of_stream) override;
  void PlayerGone(int delegate_id) override;
  bool IsHidden() override;

  // content::RenderFrameObserver overrides.
  void WasHidden() override;
  void WasShown() override;
  bool OnMessageReceived(const IPC::Message& msg) override;

 private:
  void OnMediaDelegatePause(int delegate_id);
  void OnMediaDelegatePlay(int delegate_id);
  void OnMediaDelegateVolumeMultiplierUpdate(int delegate_id,
                                             double multiplier);

  bool has_played_media_ = false;
  IDMap<Observer> id_map_;

  DISALLOW_COPY_AND_ASSIGN(RendererWebMediaPlayerDelegate);
};

}  // namespace media

#endif  // CONTENT_RENDERER_MEDIA_RENDERER_WEBMEDIAPLAYER_DELEGATE_H_
