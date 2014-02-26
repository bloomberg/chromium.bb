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
  }

  virtual ~DummyMediaStreamVideoSource() {
  }

  void OnNewFrame(const scoped_refptr<media::VideoFrame>& frame) {
    MediaStreamVideoSource::DeliverVideoFrame(frame);
  }
};

class MediaStreamVideoSourceTest
    : public ::testing::Test {
 public:
  MediaStreamVideoSourceTest()
      : number_of_successful_constraints_applied_(0),
        number_of_failed_constraints_applied_(0) {
    webkit_source_.initialize(base::UTF8ToUTF16("dummy_source_id"),
                              blink::WebMediaStreamSource::TypeVideo,
                              base::UTF8ToUTF16("dummy_source_name"));
    webkit_source_.setExtraData(new DummyMediaStreamVideoSource(&factory_));
  }

 protected:
  // Create a track that's associated with |webkit_source_|.
  blink::WebMediaStreamTrack CreateTrack(
      const std::string& id,
      const blink::WebMediaConstraints& constraints) {
    blink::WebMediaStreamTrack track;
    track.initialize(base::UTF8ToUTF16(id), webkit_source_);

    DummyMediaStreamVideoSource* source =
        static_cast<DummyMediaStreamVideoSource*>(track.source().extraData());

    source->AddTrack(track,
                     constraints,
                     base::Bind(
                         &MediaStreamVideoSourceTest::OnConstraintsApplied,
                         base::Unretained(this)));
    return track;
  }

  // Simulate that the underlying device start successfully.
  void StartSource() {
    factory_.last_video_source()->SetLive();
  }

  // Simulate that the underlying device fail to start.
  void FailToStartSource() {
    factory_.last_video_source()->SetEnded();
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

  int NumberOfSuccessConstraintsCallbacks() const {
    return number_of_successful_constraints_applied_;
  }

  int NumberOfFailedConstraintsCallbacks() const {
    return number_of_failed_constraints_applied_;
  }

 private:
  void OnConstraintsApplied(MediaStreamSource* source, bool success) {
    ASSERT_EQ(source, webkit_source_.extraData());

    if (success)
      ++number_of_successful_constraints_applied_;
    else
      ++number_of_failed_constraints_applied_;
  }

  int number_of_successful_constraints_applied_;
  int number_of_failed_constraints_applied_;
  MockMediaStreamDependencyFactory factory_;
  blink::WebMediaStreamSource webkit_source_;
};

TEST_F(MediaStreamVideoSourceTest, AddTrackAndStartAdapter) {
  blink::WebMediaConstraints constraints;
  blink::WebMediaStreamTrack track = CreateTrack("123", constraints);
  StartSource();
  EXPECT_EQ(1, NumberOfSuccessConstraintsCallbacks());
}

TEST_F(MediaStreamVideoSourceTest, AddTwoTracksBeforeAdapterStart) {
  blink::WebMediaConstraints constraints;
  blink::WebMediaStreamTrack track1 = CreateTrack("123", constraints);
  blink::WebMediaStreamTrack track2 = CreateTrack("123", constraints);
  EXPECT_EQ(0, NumberOfSuccessConstraintsCallbacks());
  StartSource();
  EXPECT_EQ(2, NumberOfSuccessConstraintsCallbacks());
}

TEST_F(MediaStreamVideoSourceTest, AddTrackAfterAdapterStart) {
  blink::WebMediaConstraints constraints;
  blink::WebMediaStreamTrack track1 = CreateTrack("123", constraints);
  StartSource();
  EXPECT_EQ(1, NumberOfSuccessConstraintsCallbacks());
  blink::WebMediaStreamTrack track2 = CreateTrack("123", constraints);
  EXPECT_EQ(2, NumberOfSuccessConstraintsCallbacks());
}

TEST_F(MediaStreamVideoSourceTest, AddTrackAndFailToStartAdapter) {
  blink::WebMediaConstraints constraints;
  blink::WebMediaStreamTrack track = CreateTrack("123", constraints);
  FailToStartSource();
  EXPECT_EQ(1, NumberOfFailedConstraintsCallbacks());
}

TEST_F(MediaStreamVideoSourceTest, DeliverVideoFrame) {
  blink::WebMediaConstraints constraints;
  blink::WebMediaStreamTrack track = CreateTrack("123", constraints);
  StartSource();
  DummyMediaStreamVideoSource* source =
      static_cast<DummyMediaStreamVideoSource*>(track.source().extraData());
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
