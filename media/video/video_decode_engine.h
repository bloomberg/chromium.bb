// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_VIDEO_DECODE_ENGINE_H_
#define MEDIA_VIDEO_VIDEO_DECODE_ENGINE_H_

#include "base/memory/ref_counted.h"
#include "media/base/media_export.h"

namespace media {

class Buffer;
class VideoDecoderConfig;
class VideoFrame;

class MEDIA_EXPORT VideoDecodeEngine {
 public:
  virtual ~VideoDecodeEngine() {}

  // Initialize the engine with specified configuration, returning true if
  // successful.
  virtual bool Initialize(const VideoDecoderConfig& config) = 0;

  // Uninitialize the engine, freeing all resources. Calls to Flush() or
  // Decode() will have no effect afterwards.
  virtual void Uninitialize() = 0;

  // Decode the encoded video data and store the result (if any) into
  // |video_frame|. Note that a frame may not always be produced if the
  // decode engine has insufficient encoded data. In such circumstances,
  // additional calls to Decode() may be required.
  //
  // Returns true if decoding was successful (includes zero length input and end
  // of stream), false if a decoding error occurred.
  virtual bool Decode(const scoped_refptr<Buffer>& buffer,
                      scoped_refptr<VideoFrame>* video_frame) = 0;

  // Discard all pending data that has yet to be returned via Decode().
  virtual void Flush() = 0;
};

}  // namespace media

#endif  // MEDIA_VIDEO_VIDEO_DECODE_ENGINE_H_
