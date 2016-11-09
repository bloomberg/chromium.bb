// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_REMOTING_INTERSTITIAL_UI_H_
#define MEDIA_REMOTING_REMOTING_INTERSTITIAL_UI_H_

#include "base/memory/ref_counted.h"
#include "media/base/video_frame.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace media {

// Gets an 'interstitial' VideoFrame to paint on the media player when the
// video is being played remotely.
scoped_refptr<VideoFrame> GetInterstitial(SkCanvas* existing_frame_canvas,
                                          bool is_remoting_successful);

}  // namespace media

#endif  // MEDIA_REMOTING_REMOTING_INTERSTITIAL_UI_H_