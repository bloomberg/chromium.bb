// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_SENDER_VIDEO_FRAME_FACTORY_H_
#define MEDIA_CAST_SENDER_VIDEO_FRAME_FACTORY_H_

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"

namespace gfx {
class Size;
}

namespace media {

class VideoFrame;

namespace cast {

// Interface for an object capable of vending video frames. There is no
// requirement for a |VideoFrameFactory| to be concurrent but it must not be
// pinned to a specific thread. Indeed, |VideoFrameFactory| implementations are
// created by cast on the main cast thread then used by unknown client threads
// via the |VideoFrameInput| interface.
//
// Clients are responsible for serialzing access to a |VideoFrameFactory|.
// Generally speaking, it is expected that a client will be using these objects
// from a rendering thread or callback (which may execute on different threads
// but never concurrently with itself). Forcing every implementation to take a
// lock, even with no contention, is an unnecessary cost, especially on mobile
// platforms.
class VideoFrameFactory {
 public:
  virtual ~VideoFrameFactory() {}

  // Creates a |VideoFrame| suitable for input via |InsertRawVideoFrame|. Frames
  // obtained in this manner may provide benefits such memory reuse and affinity
  // with the encoder. The format is guaranteed to be I420 or NV12.
  //
  // This can transiently return null if the encoder is not yet initialized or
  // is re-initializing.
  virtual scoped_refptr<VideoFrame> MaybeCreateFrame(
      const gfx::Size& frame_size, base::TimeDelta timestamp) = 0;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_SENDER_VIDEO_FRAME_FACTORY_H_
