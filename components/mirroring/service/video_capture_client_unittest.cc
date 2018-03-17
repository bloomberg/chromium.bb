// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/video_capture_client.h"

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InvokeWithoutArgs;
using ::testing::_;

namespace mirroring {

namespace {

class FakeVideoCaptureHost final : public media::mojom::VideoCaptureHost {
 public:
  explicit FakeVideoCaptureHost(media::mojom::VideoCaptureHostRequest request)
      : binding_(this, std::move(request)) {}
  ~FakeVideoCaptureHost() override {}

  // mojom::VideoCaptureHost implementations
  MOCK_METHOD1(RequestRefreshFrame, void(int32_t));
  MOCK_METHOD3(ReleaseBuffer, void(int32_t, int32_t, double));
  MOCK_METHOD1(Pause, void(int32_t));
  MOCK_METHOD3(Resume,
               void(int32_t, int32_t, const media::VideoCaptureParams&));
  MOCK_METHOD0(OnStopped, void());

  void Start(int32_t device_id,
             int32_t session_id,
             const media::VideoCaptureParams& params,
             media::mojom::VideoCaptureObserverPtr observer) override {
    client_ = std::move(observer);
    client_->OnStateChanged(media::mojom::VideoCaptureState::STARTED);
  }

  void Stop(int32_t device_id) override {
    client_->OnStateChanged(media::mojom::VideoCaptureState::ENDED);
    client_.reset();
    OnStopped();
  }

  void GetDeviceSupportedFormats(
      int32_t device_id,
      int32_t session_id,
      GetDeviceSupportedFormatsCallback callback) override {}
  void GetDeviceFormatsInUse(int32_t device_id,
                             int32_t session_id,
                             GetDeviceFormatsInUseCallback callback) override {}

 private:
  mojo::Binding<media::mojom::VideoCaptureHost> binding_;
  media::mojom::VideoCaptureObserverPtr client_;

  DISALLOW_COPY_AND_ASSIGN(FakeVideoCaptureHost);
};

media::mojom::VideoFrameInfoPtr GetVideoFrameInfo(const gfx::Size& size) {
  media::VideoFrameMetadata metadata;
  metadata.SetDouble(media::VideoFrameMetadata::FRAME_RATE, 30);
  metadata.SetTimeTicks(media::VideoFrameMetadata::REFERENCE_TIME,
                        base::TimeTicks());
  return media::mojom::VideoFrameInfo::New(
      base::TimeDelta(), metadata.CopyInternalValues(),
      media::PIXEL_FORMAT_I420, media::VideoPixelStorage::CPU, size,
      gfx::Rect(size));
}

}  // namespace

class VideoCaptureClientTest : public ::testing::Test {
 public:
  VideoCaptureClientTest() {
    media::mojom::VideoCaptureHostPtr host;
    host_impl_ =
        std::make_unique<FakeVideoCaptureHost>(mojo::MakeRequest(&host));
    client_ = std::make_unique<VideoCaptureClient>(std::move(host));
  }

  ~VideoCaptureClientTest() override {
    if (client_) {
      base::RunLoop run_loop;
      EXPECT_CALL(*host_impl_, OnStopped())
          .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
      client_->Stop();
      run_loop.Run();
    }
    scoped_task_environment_.RunUntilIdle();
  }

  MOCK_METHOD1(OnFrameReceived, void(const gfx::Size&));
  void OnFrameReady(scoped_refptr<media::VideoFrame> video_frame,
                    base::TimeTicks estimated_capture_time) {
    video_frame->metadata()->SetDouble(
        media::VideoFrameMetadata::RESOURCE_UTILIZATION, 0.6);
    OnFrameReceived(video_frame->coded_size());
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<FakeVideoCaptureHost> host_impl_;
  std::unique_ptr<VideoCaptureClient> client_;
};

TEST_F(VideoCaptureClientTest, Basic) {
  base::MockCallback<base::OnceClosure> error_cb;
  EXPECT_CALL(error_cb, Run()).Times(0);
  {
    base::RunLoop run_loop;
    // Expect to call RequestRefreshFrame() after capturing started.
    EXPECT_CALL(*host_impl_, RequestRefreshFrame(_))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    client_->Start(base::BindRepeating(&VideoCaptureClientTest::OnFrameReady,
                                       base::Unretained(this)),
                   error_cb.Get());
    run_loop.Run();
  }
  scoped_task_environment_.RunUntilIdle();
  client_->OnBufferCreated(0, mojo::SharedBufferHandle::Create(100000));
  scoped_task_environment_.RunUntilIdle();
  {
    base::RunLoop run_loop;
    // Expect to receive one frame.
    EXPECT_CALL(*this, OnFrameReceived(gfx::Size(128, 64))).Times(1);
    // Expect to return the buffer after the frame is consumed.
    EXPECT_CALL(*host_impl_, ReleaseBuffer(_, 0, 0.6))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    client_->OnBufferReady(0, GetVideoFrameInfo(gfx::Size(128, 64)));
    run_loop.Run();
  }
  scoped_task_environment_.RunUntilIdle();

  // Received a smaller size video frame in the same buffer.
  {
    base::RunLoop run_loop;
    // Expect to receive one frame.
    EXPECT_CALL(*this, OnFrameReceived(gfx::Size(64, 32))).Times(1);
    // Expect to return the buffer after the frame is consumed.
    EXPECT_CALL(*host_impl_, ReleaseBuffer(_, 0, 0.6))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    client_->OnBufferReady(0, GetVideoFrameInfo(gfx::Size(64, 32)));
    run_loop.Run();
  }
  scoped_task_environment_.RunUntilIdle();

  // Received a larger size video frame in the same buffer.
  {
    base::RunLoop run_loop;
    // Expect to receive one frame.
    EXPECT_CALL(*this, OnFrameReceived(gfx::Size(320, 180))).Times(1);
    // Expect to return the buffer after the frame is consumed.
    EXPECT_CALL(*host_impl_, ReleaseBuffer(_, 0, 0.6))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    client_->OnBufferReady(0, GetVideoFrameInfo(gfx::Size(320, 180)));
    run_loop.Run();
  }
  scoped_task_environment_.RunUntilIdle();
}

}  // namespace mirroring
