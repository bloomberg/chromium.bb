// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_video_renderer_sink.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "content/child/child_process.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/mock_media_stream_registry.h"
#include "content/renderer/media/mock_media_stream_video_source.h"
#include "media/base/video_frame.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/mock_gpu_memory_buffer_video_frame_pool.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebHeap.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::Lt;
using ::testing::Mock;

namespace content {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

class MediaStreamVideoRendererSinkTest : public testing::Test {
 public:
  MediaStreamVideoRendererSinkTest()
      : child_process_(new ChildProcess()),
        mock_source_(new MockMediaStreamVideoSource()) {
    blink_source_.Initialize(blink::WebString::FromASCII("dummy_source_id"),
                             blink::WebMediaStreamSource::kTypeVideo,
                             blink::WebString::FromASCII("dummy_source_name"),
                             false /* remote */);
    blink_source_.SetExtraData(mock_source_);
    blink_track_ = MediaStreamVideoTrack::CreateVideoTrack(
        mock_source_, MediaStreamSource::ConstraintsCallback(), true);
    mock_source_->StartMockedSource();
    base::RunLoop().RunUntilIdle();

    media_stream_video_renderer_sink_ = new MediaStreamVideoRendererSink(
        blink_track_,
        base::Bind(&MediaStreamVideoRendererSinkTest::ErrorCallback,
                   base::Unretained(this)),
        base::Bind(&MediaStreamVideoRendererSinkTest::RepaintCallback,
                   base::Unretained(this)),
        child_process_->io_task_runner(),
        scoped_task_environment_.GetMainThreadTaskRunner(),
        scoped_task_environment_.GetMainThreadTaskRunner(),
        nullptr /* gpu_factories */);
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(IsInStoppedState());
  }

  void TearDown() override {
    media_stream_video_renderer_sink_ = nullptr;
    blink_source_.Reset();
    blink_track_.Reset();
    blink::WebHeap::CollectAllGarbageForTesting();

    // Let the message loop run to finish destroying the pool.
    base::RunLoop().RunUntilIdle();
  }

  MOCK_METHOD1(RepaintCallback, void(scoped_refptr<media::VideoFrame>));
  MOCK_METHOD0(ErrorCallback, void(void));

  bool IsInStartedState() const {
    RunIOUntilIdle();
    return media_stream_video_renderer_sink_->GetStateForTesting() ==
           MediaStreamVideoRendererSink::STARTED;
  }
  bool IsInStoppedState() const {
    RunIOUntilIdle();
    return media_stream_video_renderer_sink_->GetStateForTesting() ==
           MediaStreamVideoRendererSink::STOPPED;
  }
  bool IsInPausedState() const {
    RunIOUntilIdle();
    return media_stream_video_renderer_sink_->GetStateForTesting() ==
           MediaStreamVideoRendererSink::PAUSED;
  }

  void OnVideoFrame(scoped_refptr<media::VideoFrame> frame) {
    mock_source_->DeliverVideoFrame(frame);
    base::RunLoop().RunUntilIdle();

    RunIOUntilIdle();
  }

  void SetGpuMemoryBufferVideoForTesting(
      media::GpuMemoryBufferVideoFramePool* gpu_memory_buffer_pool) {
    media_stream_video_renderer_sink_->SetGpuMemoryBufferVideoForTesting(
        gpu_memory_buffer_pool);
  }

  scoped_refptr<MediaStreamVideoRendererSink> media_stream_video_renderer_sink_;

 protected:
  // A ChildProcess is needed to fool the Tracks and Sources into believing they
  // are on the right threads. A ScopedTaskEnvironment must be instantiated
  // before ChildProcess to prevent it from leaking a TaskScheduler.
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  const std::unique_ptr<ChildProcess> child_process_;

  blink::WebMediaStreamTrack blink_track_;

 private:
  void RunIOUntilIdle() const {
    // |blink_track_| uses IO thread to send frames to sinks. Make sure that
    // tasks on IO thread are completed before moving on.
    base::RunLoop run_loop;
    child_process_->io_task_runner()->PostTaskAndReply(
        FROM_HERE, base::BindOnce([] {}), run_loop.QuitClosure());
    run_loop.Run();
    base::RunLoop().RunUntilIdle();
  }

  blink::WebMediaStreamSource blink_source_;
  MockMediaStreamVideoSource* mock_source_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamVideoRendererSinkTest);
};

// Checks that the initialization-destruction sequence works fine.
TEST_F(MediaStreamVideoRendererSinkTest, StartStop) {
  EXPECT_TRUE(IsInStoppedState());

  media_stream_video_renderer_sink_->Start();
  EXPECT_TRUE(IsInStartedState());

  media_stream_video_renderer_sink_->Pause();
  EXPECT_TRUE(IsInPausedState());

  media_stream_video_renderer_sink_->Resume();
  EXPECT_TRUE(IsInStartedState());

  media_stream_video_renderer_sink_->Stop();
  EXPECT_TRUE(IsInStoppedState());
}

// Sends 2 frames and expect them as WebM contained encoded data in writeData().
TEST_F(MediaStreamVideoRendererSinkTest, EncodeVideoFrames) {
  media_stream_video_renderer_sink_->Start();

  InSequence s;
  const scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateBlackFrame(gfx::Size(160, 80));

  EXPECT_CALL(*this, RepaintCallback(video_frame)).Times(1);
  OnVideoFrame(video_frame);

  media_stream_video_renderer_sink_->Stop();
}

class MediaStreamVideoRendererSinkAsyncAddFrameReadyTest
    : public MediaStreamVideoRendererSinkTest {
 public:
  MediaStreamVideoRendererSinkAsyncAddFrameReadyTest() {
    media_stream_video_renderer_sink_->Start();
    SetGpuMemoryBufferVideoForTesting(
        new media::MockGpuMemoryBufferVideoFramePool(&frame_ready_cbs_));
    base::RunLoop().RunUntilIdle();
  }

 protected:
  std::vector<base::Closure> frame_ready_cbs_;
};

TEST_F(MediaStreamVideoRendererSinkAsyncAddFrameReadyTest,
       CreateHardwareFrames) {
  InSequence s;
  const scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateBlackFrame(gfx::Size(160, 80));
  OnVideoFrame(video_frame);
  ASSERT_EQ(1u, frame_ready_cbs_.size());

  EXPECT_CALL(*this, RepaintCallback(video_frame)).Times(1);
  frame_ready_cbs_[0].Run();
  base::RunLoop().RunUntilIdle();

  media_stream_video_renderer_sink_->Stop();
}

class MediaStreamVideoRendererSinkTransparencyTest
    : public MediaStreamVideoRendererSinkTest {
 public:
  MediaStreamVideoRendererSinkTransparencyTest() {
    media_stream_video_renderer_sink_ = new MediaStreamVideoRendererSink(
        blink_track_,
        base::Bind(&MediaStreamVideoRendererSinkTest::ErrorCallback,
                   base::Unretained(this)),
        base::Bind(&MediaStreamVideoRendererSinkTransparencyTest::
                       VerifyTransparentFrame,
                   base::Unretained(this)),
        child_process_->io_task_runner(),
        scoped_task_environment_.GetMainThreadTaskRunner(),
        scoped_task_environment_.GetMainThreadTaskRunner(),
        nullptr /* gpu_factories */);
  }

  void VerifyTransparentFrame(scoped_refptr<media::VideoFrame> frame) {
    EXPECT_EQ(media::PIXEL_FORMAT_YV12A, frame->format());
  }
};

TEST_F(MediaStreamVideoRendererSinkTransparencyTest,
       SendTransparentFrame) {
  media_stream_video_renderer_sink_->Start();

  InSequence s;
  const gfx::Size kSize(10, 10);
  const base::TimeDelta kTimestamp = base::TimeDelta();
  const scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateFrame(media::PIXEL_FORMAT_YV12A, kSize,
                                     gfx::Rect(kSize), kSize, kTimestamp);
  OnVideoFrame(video_frame);
  base::RunLoop().RunUntilIdle();

  media_stream_video_renderer_sink_->Stop();
}

}  // namespace content
