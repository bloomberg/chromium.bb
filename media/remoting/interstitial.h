// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_INTERSTITIAL_H_
#define MEDIA_REMOTING_INTERSTITIAL_H_

#include "base/memory/ref_counted.h"

class SkBitmap;

namespace gfx {
class Size;
}

namespace media {

class VideoFrame;

namespace remoting {

enum InterstitialType {
  BETWEEN_SESSIONS,             // Show background image only.
  IN_SESSION,                   // Show MEDIA_REMOTING_CASTING_VIDEO_TEXT.
  ENCRYPTED_MEDIA_FATAL_ERROR,  // Show MEDIA_REMOTING_CAST_ERROR_TEXT.
};

// Render an interstitial frame--a combination of |image| in the background
// along with a text message describing what is going on--and return it in a
// VideoFrame. |image| may be scaled accordingly without changing its aspect
// ratio. When it has a different aspect ratio than |natural_size|, a scaled
// |background_image| will be centered in the frame. When |image| is empty, a
// blank black background will be used.
//
// Threading note: This *must* be called on the main thread, because it uses
// Skia's font rendering facility, which loads/caches fonts on the main thread.
// http://crbug.com/687473.
scoped_refptr<VideoFrame> RenderInterstitialFrame(const SkBitmap& image,
                                                  const gfx::Size& natural_size,
                                                  InterstitialType type);

}  // namespace remoting
}  // namespace media

#endif  // MEDIA_REMOTING_INTERSTITIAL_H_
