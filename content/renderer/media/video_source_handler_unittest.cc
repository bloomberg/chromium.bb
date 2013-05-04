// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/utf_string_conversions.h"
#include "content/renderer/media/media_stream_extra_data.h"
#include "content/renderer/media/media_stream_registry_interface.h"
#include "content/renderer/media/mock_media_stream_dependency_factory.h"
#include "content/renderer/media/mock_media_stream_registry.h"
#include "content/renderer/media/video_source_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamTrack.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
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
    dependency_factory_.EnsurePeerConnectionFactory();
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
  cricket::WebRtcVideoFrame test_frame;
  int width = 640;
  int height = 360;
  int64 et = 123456;
  int64 ts = 789012;
  test_frame.InitToBlack(width, height, 1, 1, et, ts);
  cricket::VideoRenderer* receiver = handler_->GetReceiver(&reader);
  ASSERT(receiver != NULL);
  receiver->RenderFrame(&test_frame);

  const VideoFrame* frame = reader.last_frame();
  ASSERT_TRUE(frame != NULL);

  // Compare |frame| to |test_frame|.
  EXPECT_EQ(test_frame.GetWidth(), frame->GetWidth());
  EXPECT_EQ(test_frame.GetHeight(), frame->GetHeight());
  EXPECT_EQ(test_frame.GetElapsedTime(), frame->GetElapsedTime());
  EXPECT_EQ(test_frame.GetTimeStamp(), frame->GetTimeStamp());
  EXPECT_EQ(test_frame.GetYPlane(), frame->GetYPlane());
  EXPECT_EQ(test_frame.GetUPlane(), frame->GetUPlane());
  EXPECT_EQ(test_frame.GetVPlane(), frame->GetVPlane());

  EXPECT_TRUE(handler_->Close(kTestStreamUrl, &reader));
  EXPECT_TRUE(handler_->GetReceiver(&reader) == NULL);
}

}  // namespace content
