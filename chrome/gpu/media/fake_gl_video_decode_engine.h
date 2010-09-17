// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_MEDIA_FAKE_GL_VIDEO_DECODE_ENGINE_H_
#define CHROME_GPU_MEDIA_FAKE_GL_VIDEO_DECODE_ENGINE_H_

#include "base/scoped_ptr.h"
#include "media/video/video_decode_engine.h"

namespace media {
class VideoFrame;
}  // namespace media

class FakeGlVideoDecodeEngine : public media::VideoDecodeEngine {
 public:
  FakeGlVideoDecodeEngine();
  virtual ~FakeGlVideoDecodeEngine();

  virtual void Initialize(
      MessageLoop* message_loop,
      media::VideoDecodeEngine::EventHandler* event_handler,
      const media::VideoCodecConfig& config);

  virtual void Uninitialize();
  virtual void Flush();
  virtual void Seek();
  virtual void ConsumeVideoSample(scoped_refptr<media::Buffer> buffer);
  virtual void ProduceVideoFrame(scoped_refptr<media::VideoFrame> frame);

 private:
  int width_;
  int height_;
  media::VideoDecodeEngine::EventHandler* handler_;

  DISALLOW_COPY_AND_ASSIGN(FakeGlVideoDecodeEngine);
};

#endif  // CHROME_GPU_MEDIA_FAKE_GL_VIDEO_DECODE_ENGINE_H_
