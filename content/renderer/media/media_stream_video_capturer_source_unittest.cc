// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_video_capturer_source.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "content/child/child_process.h"
#include "content/public/renderer/media_stream_video_sink.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/mock_mojo_media_stream_dispatcher_host.h"
#include "content/renderer/media/video_track_adapter.h"
#include "media/base/bind_to_current_loop.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebHeap.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::WithArgs;

namespace content {

namespace {

class MockVideoCapturerSource : public media::VideoCapturerSource {
 public:
  MockVideoCapturerSource() {}

  MOCK_METHOD0(RequestRefreshFrame, void());
  MOCK_METHOD0(GetPreferredFormats, media::VideoCaptureFormats());
  MOCK_METHOD3(StartCapture,
               void(const media::VideoCaptureParams& params,
                    const VideoCaptureDeliverFrameCB& new_frame_callback,
                    const RunningCallback& running_callback));
  MOCK_METHOD0(StopCapture, void());
};

class FakeMediaStreamVideoSink : public MediaStreamVideoSink {
 public:
  FakeMediaStreamVideoSink(base::TimeTicks* capture_time,
                           media::VideoFrameMetadata* metadata,
                           base::Closure got_frame_cb)
      : capture_time_(capture_time),
        metadata_(metadata),
        got_frame_cb_(got_frame_cb) {}

  void ConnectToTrack(const blink::WebMediaStreamTrack& track) {
    MediaStreamVideoSink::ConnectToTrack(
        track,
        base::Bind(&FakeMediaStreamVideoSink::OnVideoFrame,
                   base::Unretained(this)),
        true);
  }

  void DisconnectFromTrack() { MediaStreamVideoSink::DisconnectFromTrack(); }

  void OnVideoFrame(const scoped_refptr<media::VideoFrame>& frame,
                    base::TimeTicks capture_time) {
    *capture_time_ = capture_time;
    metadata_->Clear();
    metadata_->MergeMetadataFrom(frame->metadata());
    base::ResetAndReturn(&got_frame_cb_).Run();
  }

 private:
  base::TimeTicks* const capture_time_;
  media::VideoFrameMetadata* const metadata_;
  base::Closure got_frame_cb_;
};

}  // namespace

class MediaStreamVideoCapturerSourceTest : public testing::Test {
 public:
  MediaStreamVideoCapturerSourceTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI),
        child_process_(new ChildProcess()),
        source_(nullptr),
        delegate_(nullptr),
        source_stopped_(false) {}

  void TearDown() override {
    webkit_source_.Reset();
    blink::WebHeap::CollectAllGarbageForTesting();
  }

  void InitWithDeviceInfo(const StreamDeviceInfo& device_info) {
    auto delegate = base::MakeUnique<MockVideoCapturerSource>();
    delegate_ = delegate.get();
    source_ = new MediaStreamVideoCapturerSource(
        base::Bind(&MediaStreamVideoCapturerSourceTest::OnSourceStopped,
                   base::Unretained(this)),
        std::move(delegate));
    mojom::MediaStreamDispatcherHostPtr dispatcher_host =
        mock_dispatcher_host_.CreateInterfacePtrAndBind();
    source_->dispatcher_host_ = std::move(dispatcher_host);
    source_->SetDeviceInfo(device_info);

    webkit_source_.Initialize(blink::WebString::FromASCII("dummy_source_id"),
                              blink::WebMediaStreamSource::kTypeVideo,
                              blink::WebString::FromASCII("dummy_source_name"),
                              false /* remote */);
    webkit_source_.SetExtraData(source_);
    webkit_source_id_ = webkit_source_.Id();
  }

  blink::WebMediaStreamTrack StartSource(
      const VideoTrackAdapterSettings& adapter_settings,
      const base::Optional<bool>& noise_reduction,
      bool is_screencast,
      double min_frame_rate) {
    bool enabled = true;
    // CreateVideoTrack will trigger OnConstraintsApplied.
    return MediaStreamVideoTrack::CreateVideoTrack(
        source_, adapter_settings, noise_reduction, is_screencast,
        min_frame_rate,
        base::Bind(&MediaStreamVideoCapturerSourceTest::OnConstraintsApplied,
                   base::Unretained(this)),
        enabled);
  }

  MockVideoCapturerSource& mock_delegate() { return *delegate_; }

  void OnSourceStopped(const blink::WebMediaStreamSource& source) {
    source_stopped_ = true;
    EXPECT_EQ(source.Id(), webkit_source_id_);
  }
  void OnStarted(bool result) { source_->OnRunStateChanged(result); }

 protected:
  void OnConstraintsApplied(MediaStreamSource* source,
                            MediaStreamRequestResult result,
                            const blink::WebString& result_name) {}

  // A ChildProcess and a MessageLoopForUI are both needed to fool the Tracks
  // and Sources below into believing they are on the right threads.
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<ChildProcess> child_process_;

  blink::WebMediaStreamSource webkit_source_;
  MockMojoMediaStreamDispatcherHost mock_dispatcher_host_;
  MediaStreamVideoCapturerSource* source_;  // owned by |webkit_source_|.
  MockVideoCapturerSource* delegate_;     // owned by |source|.
  blink::WebString webkit_source_id_;
  bool source_stopped_;
};

TEST_F(MediaStreamVideoCapturerSourceTest, StartAndStop) {
  auto delegate = base::MakeUnique<MockVideoCapturerSource>();
  delegate_ = delegate.get();
  EXPECT_CALL(*delegate_, GetPreferredFormats());
  source_ = new MediaStreamVideoCapturerSource(
      base::Bind(&MediaStreamVideoCapturerSourceTest::OnSourceStopped,
                 base::Unretained(this)),
      std::move(delegate));
  mojom::MediaStreamDispatcherHostPtr dispatcher_host =
      mock_dispatcher_host_.CreateInterfacePtrAndBind();
  source_->dispatcher_host_ = std::move(dispatcher_host);
  webkit_source_.Initialize(blink::WebString::FromASCII("dummy_source_id"),
                            blink::WebMediaStreamSource::kTypeVideo,
                            blink::WebString::FromASCII("dummy_source_name"),
                            false /* remote */);
  webkit_source_.SetExtraData(source_);
  webkit_source_id_ = webkit_source_.Id();

  InSequence s;
  EXPECT_CALL(mock_delegate(), StartCapture(_, _, _));
  blink::WebMediaStreamTrack track = StartSource(
      VideoTrackAdapterSettings(), base::Optional<bool>(), false, 0.0);
  base::RunLoop().RunUntilIdle();

  OnStarted(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(blink::WebMediaStreamSource::kReadyStateLive,
            webkit_source_.GetReadyState());

  EXPECT_FALSE(source_stopped_);

  EXPECT_CALL(mock_delegate(), StopCapture());
  OnStarted(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(blink::WebMediaStreamSource::kReadyStateEnded,
            webkit_source_.GetReadyState());
  // Verify that MediaStreamSource::SourceStoppedCallback has been triggered.
  EXPECT_TRUE(source_stopped_);
}

TEST_F(MediaStreamVideoCapturerSourceTest, CaptureTimeAndMetadataPlumbing) {
  auto delegate = base::MakeUnique<MockVideoCapturerSource>();
  delegate_ = delegate.get();
  EXPECT_CALL(*delegate_, GetPreferredFormats());
  source_ = new MediaStreamVideoCapturerSource(
      base::Bind(&MediaStreamVideoCapturerSourceTest::OnSourceStopped,
                 base::Unretained(this)),
      std::move(delegate));
  mojom::MediaStreamDispatcherHostPtr dispatcher_host =
      mock_dispatcher_host_.CreateInterfacePtrAndBind();
  source_->dispatcher_host_ = std::move(dispatcher_host);
  webkit_source_.Initialize(blink::WebString::FromASCII("dummy_source_id"),
                            blink::WebMediaStreamSource::kTypeVideo,
                            blink::WebString::FromASCII("dummy_source_name"),
                            false /* remote */);
  webkit_source_.SetExtraData(source_);
  webkit_source_id_ = webkit_source_.Id();

  VideoCaptureDeliverFrameCB deliver_frame_cb;
  media::VideoCapturerSource::RunningCallback running_cb;

  InSequence s;
  //  EXPECT_CALL(mock_delegate(), GetCurrentSupportedFormats(_, _, _, _));
  EXPECT_CALL(mock_delegate(), StartCapture(_, _, _))
      .WillOnce(testing::DoAll(testing::SaveArg<1>(&deliver_frame_cb),
                               testing::SaveArg<2>(&running_cb)));
  EXPECT_CALL(mock_delegate(), RequestRefreshFrame());
  EXPECT_CALL(mock_delegate(), StopCapture());
  blink::WebMediaStreamTrack track = StartSource(
      VideoTrackAdapterSettings(), base::Optional<bool>(), false, 0.0);
  running_cb.Run(true);

  base::RunLoop run_loop;
  base::TimeTicks reference_capture_time =
      base::TimeTicks::FromInternalValue(60013);
  base::TimeTicks capture_time;
  media::VideoFrameMetadata metadata;
  FakeMediaStreamVideoSink fake_sink(
      &capture_time, &metadata,
      media::BindToCurrentLoop(run_loop.QuitClosure()));
  fake_sink.ConnectToTrack(track);
  const scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::CreateBlackFrame(gfx::Size(2, 2));
  frame->metadata()->SetDouble(media::VideoFrameMetadata::FRAME_RATE, 30.0);
  child_process_->io_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(deliver_frame_cb, frame, reference_capture_time));
  run_loop.Run();
  fake_sink.DisconnectFromTrack();
  EXPECT_EQ(reference_capture_time, capture_time);
  double metadata_value;
  EXPECT_TRUE(metadata.GetDouble(media::VideoFrameMetadata::FRAME_RATE,
                                 &metadata_value));
  EXPECT_EQ(30.0, metadata_value);
}

}  // namespace content
