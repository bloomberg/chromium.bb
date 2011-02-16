// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_MEDIA_FAKE_GL_VIDEO_DECODE_ENGINE_H_
#define CHROME_GPU_MEDIA_FAKE_GL_VIDEO_DECODE_ENGINE_H_

#include <queue>
#include <vector>

#include "base/scoped_ptr.h"
#include "media/base/pipeline.h"
#include "media/video/video_decode_engine.h"

namespace media {
class VideoDecodeContext;
class VideoFrame;
}  // namespace media

class FakeGlVideoDecodeEngine : public media::VideoDecodeEngine {
 public:
  FakeGlVideoDecodeEngine();
  virtual ~FakeGlVideoDecodeEngine();

  virtual void Initialize(
      MessageLoop* message_loop,
      media::VideoDecodeEngine::EventHandler* event_handler,
      media::VideoDecodeContext* context,
      const media::VideoCodecConfig& config);

  virtual void Uninitialize();
  virtual void Flush();
  virtual void Seek();
  virtual void ConsumeVideoSample(scoped_refptr<media::Buffer> buffer);
  virtual void ProduceVideoFrame(scoped_refptr<media::VideoFrame> frame);

 private:
  // This method is called when video frames allocation is completed by
  // VideoDecodeContext.
  void AllocationCompleteTask();

  // This method is called by VideoDecodeContext when uploading to a VideoFrame
  // has completed.
  void UploadCompleteTask(scoped_refptr<media::VideoFrame> frame);

  int width_;
  int height_;
  media::VideoDecodeEngine::EventHandler* handler_;
  media::VideoDecodeContext* context_;

  // Internal video frame that is to be uploaded through VideoDecodeContext.
  scoped_refptr<media::VideoFrame> internal_frame_;

  // VideoFrame(s) allocated through VideoDecodeContext. These frames are
  // opaque to us. And we need an extra upload step.
  std::vector<scoped_refptr<media::VideoFrame> > external_frames_;

  // These are the video frames that are waiting for input buffer to generate
  // fake pattern in them.
  std::queue<scoped_refptr<media::VideoFrame> > pending_frames_;

  // Dummy statistics.
  media::PipelineStatistics dummy_stats_;

  DISALLOW_COPY_AND_ASSIGN(FakeGlVideoDecodeEngine);
};

#endif  // CHROME_GPU_MEDIA_FAKE_GL_VIDEO_DECODE_ENGINE_H_
