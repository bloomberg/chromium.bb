// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_AUDIO_FOCUS_TEST_UTIL_H_
#define CONTENT_BROWSER_MEDIA_SESSION_AUDIO_FOCUS_TEST_UTIL_H_

#include "base/run_loop.h"
#include "content/browser/media/session/audio_focus_observer.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"

namespace content {
namespace test {

class TestAudioFocusObserver : public AudioFocusObserver {
 public:
  TestAudioFocusObserver();
  ~TestAudioFocusObserver() override;

  void OnFocusGained(media_session::mojom::MediaSessionInfoPtr,
                     media_session::mojom::AudioFocusType) override;

  void OnFocusLost(media_session::mojom::MediaSessionInfoPtr) override;

  void WaitForGainedEvent();
  void WaitForLostEvent();

  media_session::mojom::AudioFocusType focus_gained_type() const {
    DCHECK(!focus_gained_session_.is_null());
    return focus_gained_type_;
  }

  // These store the values we received.
  media_session::mojom::MediaSessionInfoPtr focus_gained_session_;
  media_session::mojom::MediaSessionInfoPtr focus_lost_session_;

 private:
  media_session::mojom::AudioFocusType focus_gained_type_;

  // If either of these are true we will quit the run loop if we observe a gain
  // or lost event.
  bool wait_for_gained_ = false;
  bool wait_for_lost_ = false;

  base::RunLoop run_loop_;
};

media_session::mojom::MediaSessionInfoPtr GetMediaSessionInfoSync(
    media_session::mojom::MediaSession*);

}  // namespace test
}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SESSION_AUDIO_FOCUS_TEST_UTIL_H_
