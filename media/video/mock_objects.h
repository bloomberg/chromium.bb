// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_MOCK_OBJECTS_H_
#define MEDIA_VIDEO_MOCK_OBJECTS_H_

#include "media/video/video_decode_context.h"
#include "media/video/video_decode_engine.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockVideoDecodeEngine : public VideoDecodeEngine {
 public:
  MockVideoDecodeEngine() {}
  virtual ~MockVideoDecodeEngine() {}

  MOCK_METHOD4(Initialize, void(MessageLoop* message_loop,
                                EventHandler* event_handler,
                                VideoDecodeContext* context,
                                const VideoCodecConfig& config));
  MOCK_METHOD0(Uninitialize, void());
  MOCK_METHOD0(Flush, void());
  MOCK_METHOD0(Seek, void());
  MOCK_METHOD1(ConsumeVideoSample, void(scoped_refptr<Buffer> buffer));
  MOCK_METHOD1(ProduceVideoFrame, void(scoped_refptr<VideoFrame> frame));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoDecodeEngine);
};

class MockVideoDecodeContext : public VideoDecodeContext {
 public:
  MockVideoDecodeContext() {}
  virtual ~MockVideoDecodeContext() {}

  MOCK_METHOD0(GetDevice, void*());
  MOCK_METHOD6(AllocateVideoFrames, void(
      int n, size_t width, size_t height, VideoFrame::Format format,
      std::vector<scoped_refptr<VideoFrame> >* frames,
      Task* task));
  MOCK_METHOD0(ReleaseAllVideoFrames, void());
  MOCK_METHOD3(ConvertToVideoFrame,  void(
      void* buffer, scoped_refptr<VideoFrame> frame, Task* task));
  MOCK_METHOD1(Destroy, void(Task* task));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoDecodeContext);
};

}  // namespace media

#endif  // MEDIA_VIDEO_MOCK_OBJECTS_H_
