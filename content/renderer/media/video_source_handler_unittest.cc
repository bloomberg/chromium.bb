// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_registry_interface.h"
#include "content/renderer/media/mock_media_stream_dependency_factory.h"
#include "content/renderer/media/mock_media_stream_registry.h"
#include "content/renderer/media/video_source_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/libjingle/source/talk/media/base/videocapturer.h"
#include "third_party/libjingle/source/talk/media/base/videorenderer.h"
#include "third_party/libjingle/source/talk/media/webrtc/webrtcvideoframe.h"

using cricket::VideoFrame;

namespace content {

static const std::string kTestStreamUrl = "stream_url";
static const std::string kTestVideoTrackId = "video_track_id";
static const std::string kUnknownStreamUrl = "unknown_stream_url";

class FakeFrameReader : public FrameReaderInterface {
 public:
  virtual bool GotFrame(VideoFrame* frame) OVERRIDE {
    last_frame_.reset(frame);
    return true;
  }

  const VideoFrame* last_frame() {
    return last_frame_.get();
  }

 private:
  scoped_ptr<VideoFrame> last_frame_;
};

class VideoSourceHandlerTest : public ::testing::Test {
 public:
  VideoSourceHandlerTest() : registry_(&dependency_factory_) {
    handler_.reset(new VideoSourceHandler(&registry_));
    registry_.Init(kTestStreamUrl);
    registry_.AddVideoTrack(kTestVideoTrackId);
  }

 protected:
  scoped_ptr<VideoSourceHandler> handler_;
  MockMediaStreamDependencyFactory dependency_factory_;
  MockMediaStreamRegistry registry_;
};

TEST_F(VideoSourceHandlerTest, OpenClose) {
  FakeFrameReader reader;
  // Unknow url will return false.
  EXPECT_FALSE(handler_->Open(kUnknownStreamUrl, &reader));
  EXPECT_TRUE(handler_->Open(kTestStreamUrl, &reader));

  size_t width = 640;
  size_t height = 360;
  int64 et = 123456;
  int64 ts = 789012;
  size_t size = VideoFrame::SizeOf(width, height);
  std::vector<uint8_t> test_buffer(size);
  for (size_t i = 0; i < size; ++i) {
    test_buffer[i] = (i & 0xFF);
  }

  // A new frame is captured.
  cricket::CapturedFrame captured_frame;
  captured_frame.width = width;
  captured_frame.height = height;
  captured_frame.fourcc = cricket::FOURCC_I420;
  // cricket::CapturedFrame time is in nanoseconds.
  captured_frame.elapsed_time = et;
  captured_frame.time_stamp = ts;
  captured_frame.data = &test_buffer[0];
  captured_frame.data_size = size;
  captured_frame.pixel_height = 1;
  captured_frame.pixel_width = 1;

  // The frame is delivered to VideoSourceHandler as cricket::VideoFrame.
  cricket::WebRtcVideoFrame i420_frame;
  EXPECT_TRUE(i420_frame.Alias(&captured_frame, width, height));
  cricket::VideoRenderer* receiver = handler_->GetReceiver(&reader);
  ASSERT(receiver != NULL);
  receiver->RenderFrame(&i420_frame);

  // Compare |frame| to |captured_frame|.
  const VideoFrame* frame = reader.last_frame();
  ASSERT_TRUE(frame != NULL);
  EXPECT_EQ(width, frame->GetWidth());
  EXPECT_EQ(height, frame->GetHeight());
  EXPECT_EQ(et, frame->GetElapsedTime());
  EXPECT_EQ(ts, frame->GetTimeStamp());
  std::vector<uint8_t> tmp_buffer1(size);
  EXPECT_EQ(size, frame->CopyToBuffer(&tmp_buffer1[0], size));
  EXPECT_TRUE(std::equal(test_buffer.begin(), test_buffer.end(),
                         tmp_buffer1.begin()));

  // Invalid the original frame
  memset(&test_buffer[0], 0, size);
  test_buffer.clear();

  // We should still have the same |frame|.
  std::vector<uint8_t> tmp_buffer2(size);
  EXPECT_EQ(size, frame->CopyToBuffer(&tmp_buffer2[0], size));
  EXPECT_TRUE(std::equal(tmp_buffer1.begin(), tmp_buffer1.end(),
                         tmp_buffer2.begin()));

  EXPECT_FALSE(handler_->Close(NULL));
  EXPECT_TRUE(handler_->Close(&reader));
  EXPECT_TRUE(handler_->GetReceiver(&reader) == NULL);
}

TEST_F(VideoSourceHandlerTest, OpenWithoutClose) {
  FakeFrameReader reader;
  EXPECT_TRUE(handler_->Open(kTestStreamUrl, &reader));
}

}  // namespace content
