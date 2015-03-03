// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/child/child_process.h"
#include "content/public/renderer/media_stream_video_sink.h"
#include "content/renderer/media/media_stream_video_capturer_source.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/mock_media_constraint_factory.h"
#include "media/base/bind_to_current_loop.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebHeap.h"

namespace content {

class MockVideoCapturerDelegate : public VideoCapturerDelegate {
 public:
  explicit MockVideoCapturerDelegate(const StreamDeviceInfo& device_info)
      : VideoCapturerDelegate(device_info) {}

  MOCK_METHOD4(
      StartCapture,
      void(const media::VideoCaptureParams& params,
           const VideoCaptureDeliverFrameCB& new_frame_callback,
           scoped_refptr<base::SingleThreadTaskRunner>
           frame_callback_task_runner,
           const RunningCallback& running_callback));
  MOCK_METHOD0(StopCapture, void());
};

class MediaStreamVideoCapturerSourceTest : public testing::Test {
 public:
  MediaStreamVideoCapturerSourceTest()
     : child_process_(new ChildProcess()),
       source_(NULL),
       delegate_(NULL),
       source_stopped_(false) {
  }

  void TearDown() override {
    webkit_source_.reset();
    blink::WebHeap::collectAllGarbageForTesting();
  }

  void InitWithDeviceInfo(const StreamDeviceInfo& device_info) {
    scoped_ptr<MockVideoCapturerDelegate> delegate(
        new MockVideoCapturerDelegate(device_info));
    delegate_ = delegate.get();
    source_ = new MediaStreamVideoCapturerSource(
        base::Bind(&MediaStreamVideoCapturerSourceTest::OnSourceStopped,
                    base::Unretained(this)),
        delegate.Pass());
    source_->SetDeviceInfo(device_info);

    webkit_source_.initialize(base::UTF8ToUTF16("dummy_source_id"),
                              blink::WebMediaStreamSource::TypeVideo,
                              base::UTF8ToUTF16("dummy_source_name"),
                              false /* remote */ , true /* readonly */);
    webkit_source_.setExtraData(source_);
    webkit_source_id_ = webkit_source_.id();
  }

  blink::WebMediaStreamTrack StartSource() {
    MockMediaConstraintFactory factory;
    bool enabled = true;
    // CreateVideoTrack will trigger OnConstraintsApplied.
    return MediaStreamVideoTrack::CreateVideoTrack(
        source_, factory.CreateWebMediaConstraints(),
        base::Bind(
            &MediaStreamVideoCapturerSourceTest::OnConstraintsApplied,
            base::Unretained(this)),
            enabled);
  }

  MockVideoCapturerDelegate& mock_delegate() {
    return *delegate_;
  }

  void OnSourceStopped(const blink::WebMediaStreamSource& source) {
    source_stopped_ =  true;
    EXPECT_EQ(source.id(), webkit_source_id_);
  }

 protected:
  void OnConstraintsApplied(MediaStreamSource* source,
                            MediaStreamRequestResult result,
                            const blink::WebString& result_name) {
  }

  base::MessageLoopForUI message_loop_;
  scoped_ptr<ChildProcess> child_process_;
  blink::WebMediaStreamSource webkit_source_;
  MediaStreamVideoCapturerSource* source_;  // owned by |webkit_source_|.
  MockVideoCapturerDelegate* delegate_; // owned by |source|.
  blink::WebString webkit_source_id_;
  bool source_stopped_;
};

TEST_F(MediaStreamVideoCapturerSourceTest, TabCaptureAllowResolutionChange) {
  StreamDeviceInfo device_info;
  device_info.device.type = MEDIA_TAB_VIDEO_CAPTURE;
  InitWithDeviceInfo(device_info);

  EXPECT_CALL(mock_delegate(), StartCapture(
      testing::Field(&media::VideoCaptureParams::resolution_change_policy,
                     media::RESOLUTION_POLICY_DYNAMIC_WITHIN_LIMIT),
      testing::_,
      testing::_,
      testing::_)).Times(1);
  blink::WebMediaStreamTrack track = StartSource();
  // When the track goes out of scope, the source will be stopped.
  EXPECT_CALL(mock_delegate(), StopCapture());
}

TEST_F(MediaStreamVideoCapturerSourceTest,
       DesktopCaptureAllowResolutionChange) {
  StreamDeviceInfo device_info;
  device_info.device.type = MEDIA_DESKTOP_VIDEO_CAPTURE;
  InitWithDeviceInfo(device_info);

  EXPECT_CALL(mock_delegate(), StartCapture(
      testing::Field(&media::VideoCaptureParams::resolution_change_policy,
                     media::RESOLUTION_POLICY_DYNAMIC_WITHIN_LIMIT),
      testing::_,
      testing::_,
      testing::_)).Times(1);
  blink::WebMediaStreamTrack track = StartSource();
  // When the track goes out of scope, the source will be stopped.
  EXPECT_CALL(mock_delegate(), StopCapture());
}

TEST_F(MediaStreamVideoCapturerSourceTest, Ended) {
  StreamDeviceInfo device_info;
  device_info.device.type = MEDIA_DESKTOP_VIDEO_CAPTURE;
  scoped_ptr<VideoCapturerDelegate> delegate(
      new VideoCapturerDelegate(device_info));
  VideoCapturerDelegate* delegate_ptr = delegate.get();
  source_ = new MediaStreamVideoCapturerSource(
      base::Bind(&MediaStreamVideoCapturerSourceTest::OnSourceStopped,
                 base::Unretained(this)),
      delegate.Pass());
  source_->SetDeviceInfo(device_info);
  webkit_source_.initialize(base::UTF8ToUTF16("dummy_source_id"),
                            blink::WebMediaStreamSource::TypeVideo,
                            base::UTF8ToUTF16("dummy_source_name"),
                            false /* remote */ , true /* readonly */);
  webkit_source_.setExtraData(source_);
  webkit_source_id_ = webkit_source_.id();
  blink::WebMediaStreamTrack track = StartSource();
  message_loop_.RunUntilIdle();

  delegate_ptr->OnStateUpdateOnRenderThread(VIDEO_CAPTURE_STATE_STARTED);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateLive,
            webkit_source_.readyState());

  EXPECT_FALSE(source_stopped_);
  delegate_ptr->OnStateUpdateOnRenderThread(VIDEO_CAPTURE_STATE_ERROR);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateEnded,
            webkit_source_.readyState());
  // Verify that MediaStreamSource::SourceStoppedCallback has been triggered.
  EXPECT_TRUE(source_stopped_);
}

class FakeMediaStreamVideoSink : public MediaStreamVideoSink {
 public:
  FakeMediaStreamVideoSink(base::TimeTicks* capture_time,
                           media::VideoFrameMetadata* metadata,
                           base::Closure got_frame_cb)
      : capture_time_(capture_time),
        metadata_(metadata),
        got_frame_cb_(got_frame_cb) {
  }

  void OnVideoFrame(const scoped_refptr<media::VideoFrame>& frame,
                    const base::TimeTicks& capture_time) {
    *capture_time_ = capture_time;
    metadata_->Clear();
    base::DictionaryValue tmp;
    frame->metadata()->MergeInternalValuesInto(&tmp);
    metadata_->MergeInternalValuesFrom(tmp);
    base::ResetAndReturn(&got_frame_cb_).Run();
  }

 private:
  base::TimeTicks* const capture_time_;
  media::VideoFrameMetadata* const metadata_;
  base::Closure got_frame_cb_;
};

TEST_F(MediaStreamVideoCapturerSourceTest, CaptureTimeAndMetadataPlumbing) {
  StreamDeviceInfo device_info;
  device_info.device.type = MEDIA_DESKTOP_VIDEO_CAPTURE;
  InitWithDeviceInfo(device_info);

  VideoCaptureDeliverFrameCB deliver_frame_cb;
  VideoCapturerDelegate::RunningCallback running_cb;

  EXPECT_CALL(mock_delegate(), StartCapture(
      testing::_,
      testing::_,
      testing::_,
      testing::_))
      .Times(1)
      .WillOnce(testing::DoAll(testing::SaveArg<1>(&deliver_frame_cb),
                               testing::SaveArg<3>(&running_cb)));
  EXPECT_CALL(mock_delegate(), StopCapture());
  blink::WebMediaStreamTrack track = StartSource();
  running_cb.Run(true);

  base::RunLoop run_loop;
  base::TimeTicks reference_capture_time =
      base::TimeTicks::FromInternalValue(60013);
  base::TimeTicks capture_time;
  media::VideoFrameMetadata metadata;
  FakeMediaStreamVideoSink fake_sink(
      &capture_time,
      &metadata,
      media::BindToCurrentLoop(run_loop.QuitClosure()));
  FakeMediaStreamVideoSink::AddToVideoTrack(
      &fake_sink,
      base::Bind(&FakeMediaStreamVideoSink::OnVideoFrame,
                 base::Unretained(&fake_sink)),
      track);
  const scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::CreateBlackFrame(gfx::Size(2, 2));
  frame->metadata()->SetDouble(media::VideoFrameMetadata::FRAME_RATE, 30.0);
  child_process_->io_message_loop()->PostTask(
      FROM_HERE, base::Bind(deliver_frame_cb, frame, reference_capture_time));
  run_loop.Run();
  FakeMediaStreamVideoSink::RemoveFromVideoTrack(&fake_sink, track);
  EXPECT_EQ(reference_capture_time, capture_time);
  double metadata_value;
  EXPECT_TRUE(metadata.GetDouble(media::VideoFrameMetadata::FRAME_RATE,
                                 &metadata_value));
  EXPECT_EQ(30.0, metadata_value);
}

}  // namespace content
