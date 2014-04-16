// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/media_stream_video_capturer_source.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/mock_media_constraint_factory.h"
#include "content/renderer/media/mock_media_stream_dependency_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class MockVideoCapturerDelegate : public VideoCapturerDelegate {
 public:
  explicit MockVideoCapturerDelegate(const StreamDeviceInfo& device_info)
      : VideoCapturerDelegate(device_info) {}

  MOCK_METHOD3(StartDeliver,
               void(const media::VideoCaptureParams& params,
                    const NewFrameCallback& new_frame_callback,
                    const StartedCallback& started_callback));
  MOCK_METHOD0(StopDeliver,void());

 private:
  virtual ~MockVideoCapturerDelegate() {}
};

class MediaStreamVideoCapturerSourceTest : public testing::Test {
 public:
  void InitWithDeviceInfo(const StreamDeviceInfo& device_info) {
    delegate_ = new MockVideoCapturerDelegate(device_info);
    source_ = new MediaStreamVideoCapturerSource(
        device_info,
        MediaStreamSource::SourceStoppedCallback(),
        delegate_,
        &factory_);

    webkit_source_.initialize(base::UTF8ToUTF16("dummy_source_id"),
                              blink::WebMediaStreamSource::TypeVideo,
                              base::UTF8ToUTF16("dummy_source_name"));
    webkit_source_.setExtraData(source_);
  }

  blink::WebMediaStreamTrack StartSource() {
    MockMediaConstraintFactory factory;
    bool enabled = true;
    MediaStreamDependencyFactory* dependency_factory = NULL;
    // CreateVideoTrack will trigger OnConstraintsApplied.
    return MediaStreamVideoTrack::CreateVideoTrack(
        source_, factory.CreateWebMediaConstraints(),
        base::Bind(
            &MediaStreamVideoCapturerSourceTest::OnConstraintsApplied,
            base::Unretained(this)),
            enabled, dependency_factory);
  }

 protected:
  void OnConstraintsApplied(MediaStreamSource* source, bool success) {
  }

  MockMediaStreamDependencyFactory factory_;
  blink::WebMediaStreamSource webkit_source_;
  MediaStreamVideoCapturerSource* source_;  // owned by webkit_source.
  scoped_refptr<MockVideoCapturerDelegate> delegate_;
};

TEST_F(MediaStreamVideoCapturerSourceTest, TabCaptureAllowResolutionChange) {
  StreamDeviceInfo device_info;
  device_info.device.type = MEDIA_TAB_VIDEO_CAPTURE;
  InitWithDeviceInfo(device_info);

  EXPECT_CALL(*delegate_, StartDeliver(
      testing::Field(&media::VideoCaptureParams::allow_resolution_change, true),
      testing::_,
      testing::_)).Times(1);
  blink::WebMediaStreamTrack track = StartSource();
  // When the track goes out of scope, the source will be stopped.
  EXPECT_CALL(*delegate_, StopDeliver());
}

TEST_F(MediaStreamVideoCapturerSourceTest,
       DesktopCaptureAllowResolutionChange) {
  StreamDeviceInfo device_info;
  device_info.device.type = MEDIA_DESKTOP_VIDEO_CAPTURE;
  InitWithDeviceInfo(device_info);

  EXPECT_CALL(*delegate_, StartDeliver(
      testing::Field(&media::VideoCaptureParams::allow_resolution_change, true),
      testing::_,
      testing::_)).Times(1);
  blink::WebMediaStreamTrack track = StartSource();
  // When the track goes out of scope, the source will be stopped.
  EXPECT_CALL(*delegate_, StopDeliver());
}

}  // namespace content
