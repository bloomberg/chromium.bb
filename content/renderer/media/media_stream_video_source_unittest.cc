// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/mock_media_constraint_factory.h"
#include "content/renderer/media/mock_media_stream_dependency_factory.h"
#include "content/renderer/media/mock_media_stream_video_sink.h"
#include "content/renderer/media/mock_media_stream_video_source.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class MediaStreamVideoSourceTest
    : public ::testing::Test {
 public:
  MediaStreamVideoSourceTest()
      : number_of_successful_constraints_applied_(0),
        number_of_failed_constraints_applied_(0),
        mock_source_(new MockMediaStreamVideoSource(&factory_, true)) {
    media::VideoCaptureFormats formats;
    formats.push_back(media::VideoCaptureFormat(
        gfx::Size(1280, 720), 30, media::PIXEL_FORMAT_I420));
    formats.push_back(media::VideoCaptureFormat(
        gfx::Size(640, 480), 30, media::PIXEL_FORMAT_I420));
    formats.push_back(media::VideoCaptureFormat(
        gfx::Size(352, 288), 30, media::PIXEL_FORMAT_I420));
    formats.push_back(media::VideoCaptureFormat(
        gfx::Size(320, 240), 30, media::PIXEL_FORMAT_I420));
    mock_source_->SetSupportedFormats(formats);
    webkit_source_.initialize(base::UTF8ToUTF16("dummy_source_id"),
                              blink::WebMediaStreamSource::TypeVideo,
                              base::UTF8ToUTF16("dummy_source_name"));
    webkit_source_.setExtraData(mock_source_);
  }

 protected:
  // Create a track that's associated with |webkit_source_|.
  blink::WebMediaStreamTrack CreateTrack(
      const std::string& id,
      const blink::WebMediaConstraints& constraints) {
    bool enabled = true;
    MediaStreamDependencyFactory* factory = NULL;
    return MediaStreamVideoTrack::CreateVideoTrack(
        mock_source_, constraints,
        base::Bind(
            &MediaStreamVideoSourceTest::OnConstraintsApplied,
            base::Unretained(this)),
        enabled, factory);
  }

  blink::WebMediaStreamTrack CreateTrackAndStartSource(
      const blink::WebMediaConstraints& constraints,
      int expected_width,
      int expected_height,
      int expected_frame_rate) {
    blink::WebMediaStreamTrack track = CreateTrack("123", constraints);

    mock_source_->CompleteGetSupportedFormats();
    const media::VideoCaptureParams& format = mock_source()->start_params();
    EXPECT_EQ(expected_width, format.requested_format.frame_size.width());
    EXPECT_EQ(expected_height, format.requested_format.frame_size.height());
    EXPECT_EQ(expected_frame_rate, format.requested_format.frame_rate);

    MediaStreamVideoSource* source =
        static_cast<MediaStreamVideoSource*>(track.source().extraData());
    EXPECT_TRUE(source->GetAdapter() != NULL);

    EXPECT_EQ(0, NumberOfSuccessConstraintsCallbacks());
    mock_source_->StartMockedSource();
    // Once the source has started successfully we expect that the
    // ConstraintsCallback in MediaStreamSource::AddTrack completes.
    EXPECT_EQ(1, NumberOfSuccessConstraintsCallbacks());
    return track;
  }

  int NumberOfSuccessConstraintsCallbacks() const {
    return number_of_successful_constraints_applied_;
  }

  int NumberOfFailedConstraintsCallbacks() const {
    return number_of_failed_constraints_applied_;
  }

  MockMediaStreamVideoSource* mock_source() { return mock_source_; }

  // Test that the source crops to the requested max width and
  // height even though the camera delivers a larger frame.
  // TODO(perkj): Frame resolution should be verified in MediaStreamVideoTrack
  // and not in the adapter.
  void TestSourceCropFrame(int capture_width,
                           int capture_height,
                           const blink::WebMediaConstraints& constraints,
                           int expected_height,
                           int expected_width) {
    // Expect the source to start capture with the supported resolution.
    blink::WebMediaStreamTrack track = CreateTrackAndStartSource(
        constraints, capture_width, capture_height , 30);

    ASSERT_TRUE(mock_source()->GetAdapter());
    MockVideoSource* adapter = static_cast<MockVideoSource*>(
        mock_source()->GetAdapter());
    EXPECT_EQ(0, adapter->GetFrameNum());

    scoped_refptr<media::VideoFrame> frame =
        media::VideoFrame::CreateBlackFrame(gfx::Size(capture_width,
                                                      capture_height));
    mock_source()->DeliverVideoFrame(frame);
    EXPECT_EQ(1, adapter->GetFrameNum());

    // Expect the delivered frame to be cropped.
    EXPECT_EQ(expected_height, adapter->GetLastFrameWidth());
    EXPECT_EQ(expected_width, adapter->GetLastFrameHeight());
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
  // |mock_source_| is owned by |webkit_source_|.
  MockMediaStreamVideoSource* mock_source_;
};

TEST_F(MediaStreamVideoSourceTest, AddTrackAndStartSource) {
  blink::WebMediaConstraints constraints;
  constraints.initialize();
  blink::WebMediaStreamTrack track = CreateTrack("123", constraints);
  mock_source()->CompleteGetSupportedFormats();
  mock_source()->StartMockedSource();
  EXPECT_EQ(1, NumberOfSuccessConstraintsCallbacks());
}

TEST_F(MediaStreamVideoSourceTest, AddTwoTracksBeforeSourceStarts) {
  blink::WebMediaConstraints constraints;
  constraints.initialize();
  blink::WebMediaStreamTrack track1 = CreateTrack("123", constraints);
  mock_source()->CompleteGetSupportedFormats();
  blink::WebMediaStreamTrack track2 = CreateTrack("123", constraints);
  EXPECT_EQ(0, NumberOfSuccessConstraintsCallbacks());
  mock_source()->StartMockedSource();
  EXPECT_EQ(2, NumberOfSuccessConstraintsCallbacks());
}

TEST_F(MediaStreamVideoSourceTest, AddTrackAfterSourceStarts) {
  blink::WebMediaConstraints constraints;
  constraints.initialize();
  blink::WebMediaStreamTrack track1 = CreateTrack("123", constraints);
  mock_source()->CompleteGetSupportedFormats();
  mock_source()->StartMockedSource();
  EXPECT_EQ(1, NumberOfSuccessConstraintsCallbacks());
  blink::WebMediaStreamTrack track2 = CreateTrack("123", constraints);
  EXPECT_EQ(2, NumberOfSuccessConstraintsCallbacks());
}

TEST_F(MediaStreamVideoSourceTest, AddTrackAndFailToStartSource) {
  blink::WebMediaConstraints constraints;
  constraints.initialize();
  blink::WebMediaStreamTrack track = CreateTrack("123", constraints);
  mock_source()->CompleteGetSupportedFormats();
  mock_source()->FailToStartMockedSource();
  EXPECT_EQ(1, NumberOfFailedConstraintsCallbacks());
}

TEST_F(MediaStreamVideoSourceTest, AddTwoTracksBeforeGetSupportedFormats) {
  blink::WebMediaConstraints constraints;
  constraints.initialize();
  blink::WebMediaStreamTrack track1 = CreateTrack("123", constraints);
  blink::WebMediaStreamTrack track2 = CreateTrack("123", constraints);
  mock_source()->CompleteGetSupportedFormats();
  mock_source()->StartMockedSource();
  EXPECT_EQ(2, NumberOfSuccessConstraintsCallbacks());
}

// Test that the capture output is CIF if we set max constraints to CIF.
// and the capture device support CIF.
TEST_F(MediaStreamVideoSourceTest, MandatoryConstraintCif5Fps) {
  MockMediaConstraintFactory factory;
  factory.AddMandatory(MediaStreamVideoSource::kMaxWidth, 352);
  factory.AddMandatory(MediaStreamVideoSource::kMaxHeight, 288);
  factory.AddMandatory(MediaStreamVideoSource::kMaxFrameRate, 5);

  CreateTrackAndStartSource(factory.CreateWebMediaConstraints(), 352, 288, 5);
}

// Test that the capture output is 720P if the camera support it and the
// optional constraint is set to 720P.
TEST_F(MediaStreamVideoSourceTest, MandatoryMinVgaOptional720P) {
  MockMediaConstraintFactory factory;
  factory.AddMandatory(MediaStreamVideoSource::kMinWidth, 640);
  factory.AddMandatory(MediaStreamVideoSource::kMinHeight, 480);
  factory.AddOptional(MediaStreamVideoSource::kMinWidth, 1280);
  factory.AddOptional(MediaStreamVideoSource::kMinAspectRatio,
                      1280.0 / 720);

  CreateTrackAndStartSource(factory.CreateWebMediaConstraints(), 1280, 720, 30);
}

// Test that the capture output have aspect ratio 4:3 if a mandatory constraint
// require it even if an optional constraint request a higher resolution
// that don't have this aspect ratio.
TEST_F(MediaStreamVideoSourceTest, MandatoryAspectRatio4To3) {
  MockMediaConstraintFactory factory;
  factory.AddMandatory(MediaStreamVideoSource::kMinWidth, 640);
  factory.AddMandatory(MediaStreamVideoSource::kMinHeight, 480);
  factory.AddMandatory(MediaStreamVideoSource::kMaxAspectRatio,
                       640.0 / 480);
  factory.AddOptional(MediaStreamVideoSource::kMinWidth, 1280);

  CreateTrackAndStartSource(factory.CreateWebMediaConstraints(), 640, 480, 30);
}

// Test that AddTrack fail if the mandatory aspect ratio
// is set higher than supported.
TEST_F(MediaStreamVideoSourceTest, MandatoryAspectRatioTooHigh) {
  MockMediaConstraintFactory factory;
  factory.AddMandatory(MediaStreamVideoSource::kMinAspectRatio, 2);
  blink::WebMediaStreamTrack track = CreateTrack(
      "123", factory.CreateWebMediaConstraints());
  mock_source()->CompleteGetSupportedFormats();
  EXPECT_EQ(1, NumberOfFailedConstraintsCallbacks());
}

// Test that the source ignores an optional aspect ratio that is higher than
// supported.
TEST_F(MediaStreamVideoSourceTest, OptionalAspectRatioTooHigh) {
  MockMediaConstraintFactory factory;
  factory.AddOptional(MediaStreamVideoSource::kMinAspectRatio, 2);
  blink::WebMediaStreamTrack track = CreateTrack(
      "123", factory.CreateWebMediaConstraints());
  mock_source()->CompleteGetSupportedFormats();

  const media::VideoCaptureParams& params = mock_source()->start_params();
  double aspect_ratio =
      static_cast<double>(params.requested_format.frame_size.width()) /
      params.requested_format.frame_size.height();
  EXPECT_LT(aspect_ratio, 2);
}

// Test that the source starts video with the default resolution if the
// that is the only supported.
TEST_F(MediaStreamVideoSourceTest, DefaultCapability) {
  media::VideoCaptureFormats formats;
  formats.push_back(media::VideoCaptureFormat(
      gfx::Size(MediaStreamVideoSource::kDefaultWidth,
                MediaStreamVideoSource::kDefaultHeight),
      MediaStreamVideoSource::kDefaultFrameRate,
      media::PIXEL_FORMAT_I420));
  mock_source()->SetSupportedFormats(formats);

  blink::WebMediaConstraints constraints;
  constraints.initialize();
  CreateTrackAndStartSource(constraints,
                            MediaStreamVideoSource::kDefaultWidth,
                            MediaStreamVideoSource::kDefaultHeight,
                            30);
}

TEST_F(MediaStreamVideoSourceTest, InvalidMandatoryConstraint) {
  MockMediaConstraintFactory factory;
  factory.AddMandatory("weird key", 640);
  blink::WebMediaStreamTrack track = CreateTrack(
      "123", factory.CreateWebMediaConstraints());
  mock_source()->CompleteGetSupportedFormats();
  EXPECT_EQ(1, NumberOfFailedConstraintsCallbacks());
}

// Test that the source ignores an unknown optional constraint.
TEST_F(MediaStreamVideoSourceTest, InvalidOptionalConstraint) {
  MockMediaConstraintFactory factory;
  factory.AddOptional("weird key", 640);

  CreateTrackAndStartSource(factory.CreateWebMediaConstraints(),
                            MediaStreamVideoSource::kDefaultWidth,
                            MediaStreamVideoSource::kDefaultHeight,
                            30);
}

// Tests that the source starts video with the max width and height set by
// constraints for screencast.
TEST_F(MediaStreamVideoSourceTest, ScreencastResolutionWithConstraint) {
  media::VideoCaptureFormats formats;
      formats.push_back(media::VideoCaptureFormat(
          gfx::Size(480, 270), 30, media::PIXEL_FORMAT_I420));
  mock_source()->SetSupportedFormats(formats);
  MockMediaConstraintFactory factory;
  factory.AddMandatory(MediaStreamVideoSource::kMaxWidth, 480);
  factory.AddMandatory(MediaStreamVideoSource::kMaxHeight, 270);

  blink::WebMediaStreamTrack track = CreateTrackAndStartSource(
      factory.CreateWebMediaConstraints(), 480, 270, 30);
  EXPECT_EQ(480, mock_source()->max_requested_height());
  EXPECT_EQ(270, mock_source()->max_requested_width());
}

// Test that optional constraints are applied in order.
TEST_F(MediaStreamVideoSourceTest, OptionalConstraints) {
  MockMediaConstraintFactory factory;
  // Min width of 2056 pixels can not be fulfilled.
  factory.AddOptional(MediaStreamVideoSource::kMinWidth, 2056);
  factory.AddOptional(MediaStreamVideoSource::kMinWidth, 641);
  // Since min width is set to 641 pixels, max width 640 can not be fulfilled.
  factory.AddOptional(MediaStreamVideoSource::kMaxWidth, 640);
  CreateTrackAndStartSource(factory.CreateWebMediaConstraints(), 1280, 720, 30);
}

// Test that the webrtc video adapter can be created and that it received
// video frames if the source deliver video frames.
TEST_F(MediaStreamVideoSourceTest, AdapterReceiveVideoFrame) {
  MockMediaConstraintFactory factory;
  blink::WebMediaStreamTrack track = CreateTrackAndStartSource(
      factory.CreateWebMediaConstraints(),
      MediaStreamVideoSource::kDefaultWidth,
      MediaStreamVideoSource::kDefaultHeight,
      MediaStreamVideoSource::kDefaultFrameRate);
  ASSERT_TRUE(mock_source()->GetAdapter());
  MockVideoSource* adapter = static_cast<MockVideoSource*>(
      mock_source()->GetAdapter());
  EXPECT_EQ(0, adapter->GetFrameNum());

  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::CreateBlackFrame(
          gfx::Size(MediaStreamVideoSource::kDefaultWidth,
                    MediaStreamVideoSource::kDefaultHeight));
   mock_source()->DeliverVideoFrame(frame);
   EXPECT_EQ(1, adapter->GetFrameNum());
   EXPECT_EQ(MediaStreamVideoSource::kDefaultWidth,
             adapter->GetLastFrameWidth());
   EXPECT_EQ(MediaStreamVideoSource::kDefaultHeight,
             adapter->GetLastFrameHeight());
}

// Test that the source crops to the requested max width and
// height even though the camera delivers a larger frame.
TEST_F(MediaStreamVideoSourceTest, DeliverCroppedVideoFrameOptional640360) {
  MockMediaConstraintFactory factory;
  factory.AddOptional(MediaStreamVideoSource::kMaxWidth, 640);
  factory.AddOptional(MediaStreamVideoSource::kMaxHeight, 360);
  TestSourceCropFrame(640, 480, factory.CreateWebMediaConstraints(), 640, 360);
}

TEST_F(MediaStreamVideoSourceTest, DeliverCroppedVideoFrameMandatory640360) {
  MockMediaConstraintFactory factory;
  factory.AddMandatory(MediaStreamVideoSource::kMaxWidth, 640);
  factory.AddMandatory(MediaStreamVideoSource::kMaxHeight, 360);
  TestSourceCropFrame(640, 480, factory.CreateWebMediaConstraints(), 640, 360);
}

TEST_F(MediaStreamVideoSourceTest, DeliverCroppedVideoFrameMandatory732489) {
  MockMediaConstraintFactory factory;
  factory.AddMandatory(MediaStreamVideoSource::kMaxWidth, 732);
  factory.AddMandatory(MediaStreamVideoSource::kMaxHeight, 489);
  factory.AddMandatory(MediaStreamVideoSource::kMinWidth, 732);
  factory.AddMandatory(MediaStreamVideoSource::kMinWidth, 489);
  TestSourceCropFrame(1280, 720, factory.CreateWebMediaConstraints(), 732, 489);
}

// Test that the source crops to the requested max width and
// height even though the requested frame has odd size.
TEST_F(MediaStreamVideoSourceTest, DeliverCroppedVideoFrame637359) {
  MockMediaConstraintFactory factory;
  factory.AddOptional(MediaStreamVideoSource::kMaxWidth, 637);
  factory.AddOptional(MediaStreamVideoSource::kMaxHeight, 359);
  TestSourceCropFrame(640, 480, factory.CreateWebMediaConstraints(), 637, 359);
}

TEST_F(MediaStreamVideoSourceTest, DeliverSmallerSizeWhenTooLargeMax) {
  MockMediaConstraintFactory factory;
  factory.AddOptional(MediaStreamVideoSource::kMaxWidth, 1920);
  factory.AddOptional(MediaStreamVideoSource::kMaxHeight, 1080);
  factory.AddOptional(MediaStreamVideoSource::kMinWidth, 1280);
  factory.AddOptional(MediaStreamVideoSource::kMinHeight, 720);
  TestSourceCropFrame(1280, 720, factory.CreateWebMediaConstraints(),
                      1280, 720);
}

// Test that a source can change the frame resolution on the fly and that
// tracks sinks get the new frame size unless constraints force the frame to be
// cropped.
TEST_F(MediaStreamVideoSourceTest, SourceChangeFrameSize) {
  MockMediaConstraintFactory factory;
  factory.AddOptional(MediaStreamVideoSource::kMaxWidth, 800);
  factory.AddOptional(MediaStreamVideoSource::kMaxHeight, 700);

  // Expect the source to start capture with the supported resolution.
  blink::WebMediaStreamTrack track =
      CreateTrackAndStartSource(factory.CreateWebMediaConstraints(),
                                640, 480, 30);

  MockMediaStreamVideoSink sink;
  MediaStreamVideoSink::AddToVideoTrack(&sink, track);
  EXPECT_EQ(0, sink.number_of_frames());

  {
    scoped_refptr<media::VideoFrame> frame1 =
        media::VideoFrame::CreateBlackFrame(gfx::Size(320, 240));
    mock_source()->DeliverVideoFrame(frame1);
  }
  EXPECT_EQ(1, sink.number_of_frames());
  // Expect the delivered frame to be passed unchanged since its smaller than
  // max requested.
  EXPECT_EQ(320, sink.frame_size().width());
  EXPECT_EQ(240, sink.frame_size().height());

  {
    scoped_refptr<media::VideoFrame> frame2 =
          media::VideoFrame::CreateBlackFrame(gfx::Size(640, 480));
    mock_source()->DeliverVideoFrame(frame2);
  }
  EXPECT_EQ(2, sink.number_of_frames());
  // Expect the delivered frame to be passed unchanged since its smaller than
  // max requested.
  EXPECT_EQ(640, sink.frame_size().width());
  EXPECT_EQ(480, sink.frame_size().height());

  {
    scoped_refptr<media::VideoFrame> frame3 =
          media::VideoFrame::CreateBlackFrame(gfx::Size(1280, 720));
    mock_source()->DeliverVideoFrame(frame3);
  }
  EXPECT_EQ(3, sink.number_of_frames());
  // Expect a frame to be cropped since its larger than max requested.
  EXPECT_EQ(800, sink.frame_size().width());
  EXPECT_EQ(700, sink.frame_size().height());

  MediaStreamVideoSink::RemoveFromVideoTrack(&sink, track);
}

TEST_F(MediaStreamVideoSourceTest, IsConstraintSupported) {
  EXPECT_TRUE(MediaStreamVideoSource::IsConstraintSupported(
          MediaStreamVideoSource::kMaxFrameRate));
  EXPECT_TRUE(MediaStreamVideoSource::IsConstraintSupported(
        MediaStreamVideoSource::kMinFrameRate));
  EXPECT_TRUE(MediaStreamVideoSource::IsConstraintSupported(
      MediaStreamVideoSource::kMaxWidth));
  EXPECT_TRUE(MediaStreamVideoSource::IsConstraintSupported(
        MediaStreamVideoSource::kMinWidth));
  EXPECT_TRUE(MediaStreamVideoSource::IsConstraintSupported(
        MediaStreamVideoSource::kMaxHeight));
  EXPECT_TRUE(MediaStreamVideoSource::IsConstraintSupported(
      MediaStreamVideoSource::kMinHeight));
  EXPECT_TRUE(MediaStreamVideoSource::IsConstraintSupported(
        MediaStreamVideoSource::kMaxAspectRatio));
  EXPECT_TRUE(MediaStreamVideoSource::IsConstraintSupported(
      MediaStreamVideoSource::kMinAspectRatio));

  EXPECT_FALSE(MediaStreamVideoSource::IsConstraintSupported(
      "something unsupported"));
}

}  // namespace content
