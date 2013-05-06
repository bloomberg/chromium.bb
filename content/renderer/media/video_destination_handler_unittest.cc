// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/utf_string_conversions.h"
#include "content/renderer/media/media_stream_extra_data.h"
#include "content/renderer/media/mock_media_stream_dependency_factory.h"
#include "content/renderer/media/mock_media_stream_registry.h"
#include "content/renderer/media/video_destination_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamTrack.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"

using cricket::CapturedFrame;
using cricket::CaptureState;
using cricket::VideoCapturer;
using cricket::VideoFormat;
using cricket::VideoFormatPod;

namespace content {

static const std::string kTestStreamUrl = "stream_url";
static const std::string kUnknownStreamUrl = "unknown_stream_url";
static const VideoFormatPod kTestFormat = {
  640, 360, FPS_TO_INTERVAL(30), cricket::FOURCC_ANY
};

class PpFrameWriterTest
    : public ::testing::Test,
      public sigslot::has_slots<> {
 public:
  PpFrameWriterTest()
      : last_capture_state_(cricket::CS_FAILED),
        captured_frame_count_(0),
        captured_frame_(NULL) {
    writer_.SignalStateChange.connect(this, &PpFrameWriterTest::OnStateChange);
    writer_.SignalFrameCaptured.connect(
        this, &PpFrameWriterTest::OnFrameCaptured);
  }

  void OnStateChange(VideoCapturer* capturer, CaptureState state) {
    last_capture_state_ = state;
  }

  void OnFrameCaptured(VideoCapturer* capturer, const CapturedFrame* frame) {
    ++captured_frame_count_;
    captured_frame_ = const_cast<CapturedFrame*>(frame);
  }

 protected:
  PpFrameWriter writer_;
  CaptureState last_capture_state_;
  int captured_frame_count_;
  CapturedFrame* captured_frame_;
};

class VideoDestinationHandlerTest : public ::testing::Test {
 public:
  VideoDestinationHandlerTest() : registry_(&factory_) {
    factory_.EnsurePeerConnectionFactory();
    registry_.Init(kTestStreamUrl);
  }

 protected:
  MockMediaStreamDependencyFactory factory_;
  MockMediaStreamRegistry registry_;
};

TEST_F(PpFrameWriterTest, StartStop) {
  EXPECT_FALSE(writer_.IsRunning());
  EXPECT_EQ(cricket::CS_STARTING, writer_.Start(VideoFormat(kTestFormat)));
  EXPECT_TRUE(writer_.IsRunning());
  EXPECT_EQ(cricket::CS_FAILED, writer_.Start(VideoFormat(kTestFormat)));
  writer_.Stop();
  EXPECT_EQ(cricket::CS_STOPPED, last_capture_state_);
}

TEST_F(PpFrameWriterTest, GetPreferredFourccs) {
  std::vector<uint32> fourccs;
  EXPECT_TRUE(writer_.GetPreferredFourccs(&fourccs));
  EXPECT_EQ(1u, fourccs.size());
  EXPECT_EQ(cricket::FOURCC_BGRA, fourccs[0]);
}

TEST_F(PpFrameWriterTest, GetBestCaptureFormat) {
  VideoFormat desired(kTestFormat);
  VideoFormat best_format;
  EXPECT_FALSE(writer_.GetBestCaptureFormat(desired, NULL));
  EXPECT_TRUE(writer_.GetBestCaptureFormat(desired, &best_format));
  EXPECT_EQ(cricket::FOURCC_BGRA, best_format.fourcc);

  desired.fourcc = best_format.fourcc;
  EXPECT_EQ(desired, best_format);
}

TEST_F(VideoDestinationHandlerTest, Open) {
  FrameWriterInterface* frame_writer = NULL;
  // Unknow url will return false.
  EXPECT_FALSE(VideoDestinationHandler::Open(&factory_, &registry_,
                                             kUnknownStreamUrl, &frame_writer));
  EXPECT_TRUE(VideoDestinationHandler::Open(&factory_, &registry_,
                                            kTestStreamUrl, &frame_writer));
  EXPECT_TRUE(frame_writer);

  // Verify the video track has been added.
  const WebKit::WebMediaStream test_stream = registry_.test_stream();
  WebKit::WebVector<WebKit::WebMediaStreamTrack> video_tracks;
  test_stream.videoSources(video_tracks);
  EXPECT_EQ(1u, video_tracks.size());

  // Verify the native video track has been added.
  MediaStreamExtraData* extra_data =
      static_cast<MediaStreamExtraData*>(test_stream.extraData());
  DCHECK(extra_data);
  webrtc::MediaStreamInterface* native_stream = extra_data->stream();
  DCHECK(native_stream);
  webrtc::VideoTrackVector native_video_tracks =
      native_stream->GetVideoTracks();
  EXPECT_EQ(1u, native_video_tracks.size());

  delete frame_writer;
}

}  // namespace content
