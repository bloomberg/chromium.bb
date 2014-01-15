// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "content/renderer/media/mock_media_stream_dependency_factory.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class DummyMediaStreamVideoSource : public MediaStreamVideoSource {
 public:
  DummyMediaStreamVideoSource(MediaStreamDependencyFactory* factory)
      : MediaStreamVideoSource(factory) {
    Init();
    SetVideoSource(GetAdapter());
    SetReadyState(blink::WebMediaStreamSource::ReadyStateLive);
  }

  virtual ~DummyMediaStreamVideoSource() {
    SetReadyState(blink::WebMediaStreamSource::ReadyStateEnded);
  }

  void OnNewFrame(const scoped_refptr<media::VideoFrame>& frame) {
    MediaStreamVideoSource::DeliverVideoFrame(frame);
  }
};

class MediaStreamVideoSourceTest
    : public ::testing::Test {
 public:
  MediaStreamVideoSourceTest() {
    factory_.EnsurePeerConnectionFactory();
    webkit_source_.initialize(base::UTF8ToUTF16("dummy_source_id"),
                              blink::WebMediaStreamSource::TypeVideo,
                              base::UTF8ToUTF16("dummy_source_name"));
    webkit_source_.setExtraData(new DummyMediaStreamVideoSource(&factory_));
  }

 protected:
  // Create a track that's associated with |webkit_source_|.
  blink::WebMediaStreamTrack CreateTrack(const std::string& id) {
    blink::WebMediaStreamTrack track;
    track.initialize(base::UTF8ToUTF16(id), webkit_source_);
    return track;
  }

  void VerifyFrame(int width, int height, int num) {
    DummyMediaStreamVideoSource* source =
      static_cast<DummyMediaStreamVideoSource*>(webkit_source_.extraData());
    MockVideoSource* adapter =
        static_cast<MockVideoSource*>(source->GetAdapter());
    EXPECT_EQ(width, adapter->GetLastFrameWidth());
    EXPECT_EQ(height, adapter->GetLastFrameHeight());
    EXPECT_EQ(num, adapter->GetFrameNum());
  }
 private:
  MockMediaStreamDependencyFactory factory_;
  blink::WebMediaStreamSource webkit_source_;
};

TEST_F(MediaStreamVideoSourceTest, DeliverVideoFrame) {
  blink::WebMediaConstraints constraints;
  blink::WebMediaStreamTrack track = CreateTrack("123");
  DummyMediaStreamVideoSource* source =
      static_cast<DummyMediaStreamVideoSource*>(track.source().extraData());
  source->AddTrack(track, constraints);
  VerifyFrame(0, 0, 0);
  const int kWidth = 640;
  const int kHeight = 480;
  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::CreateBlackFrame(gfx::Size(kWidth, kHeight));
  ASSERT_TRUE(frame.get());
  source->OnNewFrame(frame);
  VerifyFrame(640, 480, 1);
  source->OnNewFrame(frame);
  VerifyFrame(640, 480, 2);
  source->RemoveTrack(track);
}

}  // namespace content
