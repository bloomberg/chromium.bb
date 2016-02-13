// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_WEBMEDIAPLAYER_DELEGATE_H_
#define MEDIA_BLINK_WEBMEDIAPLAYER_DELEGATE_H_

namespace blink {
class WebMediaPlayer;
}
namespace media {

// An interface to allow a WebMediaPlayer to communicate changes of state to
// objects that need to know.
class WebMediaPlayerDelegate {
 public:
  class Observer {
   public:
    // Called when the WebMediaPlayer is no longer in the foreground.  Audio may
    // continue in the background unless |must_suspend| is true.
    virtual void OnHidden(bool must_suspend) = 0;

    virtual void OnShown() = 0;
    virtual void OnPlay() = 0;
    virtual void OnPause() = 0;

    // Playout volume should be set to current_volume * multiplier.  The range
    // is [0, 1] and is typically 1.
    virtual void OnVolumeMultiplierUpdate(double multiplier) = 0;
  };

  WebMediaPlayerDelegate() {}

  // Subscribe or unsubscribe from observer callbacks respectively. A client
  // must use the delegate id returned by AddObserver() for all other calls.
  virtual int AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(int delegate_id) = 0;

  // The specified player started playing media.
  virtual void DidPlay(int delegate_id,
                       bool has_video,
                       bool has_audio,
                       bool is_remote,
                       base::TimeDelta duration) = 0;

  // The specified player stopped playing media.
  virtual void DidPause(int delegate_id, bool reached_end_of_stream) = 0;

  // The specified player was destroyed or suspended.  This may be called
  // multiple times in row. Note: Clients must still call RemoveObserver() to
  // unsubscribe from callbacks.
  virtual void PlayerGone(int delegate_id) = 0;

  // Returns whether the render frame is currently hidden.
  virtual bool IsHidden() = 0;

 protected:
  virtual ~WebMediaPlayerDelegate() {}
};

}  // namespace media

#endif  // MEDIA_BLINK_WEBMEDIAPLAYER_DELEGATE_H_
