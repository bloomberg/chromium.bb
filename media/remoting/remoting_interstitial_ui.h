// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_REMOTING_INTERSTITIAL_UI_H_
#define MEDIA_REMOTING_REMOTING_INTERSTITIAL_UI_H_

#include "third_party/skia/include/core/SkBitmap.h"

namespace gfx {
class Size;
}

namespace media {

class VideoRendererSink;

enum RemotingInterstitialType {
  BETWEEN_SESSIONS,             // Show background image only.
  IN_SESSION,                   // Show MEDIA_REMOTING_CASTING_VIDEO_TEXT.
  ENCRYPTED_MEDIA_FATAL_ERROR,  // Show MEDIA_REMOTING_CAST_ERROR_TEXT.
};

// Paint an interstitial frame and send it to the given VideoRendererSink.
// |background_image| may be scaled accordingly without changing its aspect
// ratio. When has different aspect ratio with |natural_size|, scaled
// |background_image| will be centered in the canvas. When |background_image|
// is empty, interstitial will be drawn on a blank and black background. When
// |interstial_type| is BETWEEN_SESSIONS, show background image only.
void PaintRemotingInterstitial(const SkBitmap& background_image,
                               const gfx::Size& natural_size,
                               RemotingInterstitialType interstitial_type,
                               VideoRendererSink* video_renderer_sink);

}  // namespace media

#endif  // MEDIA_REMOTING_REMOTING_INTERSTITIAL_UI_H_
