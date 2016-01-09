// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_WEBMEDIAPLAYER_DELEGATE_H_
#define MEDIA_BLINK_WEBMEDIAPLAYER_DELEGATE_H_

namespace blink {
class WebMediaPlayer;
}
namespace media {

// An interface to allow a WebMediaPlayerImpl to communicate changes of state
// to objects that need to know.
class WebMediaPlayerDelegate {
 public:
  class Observer {
   public:
    virtual void OnHidden() = 0;
    virtual void OnShown() = 0;
  };

  WebMediaPlayerDelegate() {}

  // The specified player started playing media.
  virtual void DidPlay(blink::WebMediaPlayer* player) = 0;

  // The specified player stopped playing media.
  virtual void DidPause(blink::WebMediaPlayer* player) = 0;

  // The specified player was destroyed. Do not call any methods on it.
  // (RemoveObserver() is still necessary if the player is also an observer.)
  virtual void PlayerGone(blink::WebMediaPlayer* player) = 0;

  // Subscribe to observer callbacks.
  virtual void AddObserver(Observer* observer) = 0;

  // Unsubscribe from observer callbacks.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns whether the render frame is currently hidden.
  virtual bool IsHidden() = 0;

 protected:
  virtual ~WebMediaPlayerDelegate() {}
};

}  // namespace media

#endif  // MEDIA_BLINK_WEBMEDIAPLAYER_DELEGATE_H_
