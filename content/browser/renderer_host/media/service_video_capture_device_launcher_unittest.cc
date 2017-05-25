// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/service_video_capture_device_launcher.h"

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/video_capture/public/interfaces/device_factory.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::_;

namespace content {

static const std::string kStubDeviceId = "StubDevice";
static const media::VideoCaptureParams kArbitraryParams;
static const base::WeakPtr<media::VideoFrameReceiver> kNullReceiver;

class MockDeviceFactory : public video_capture::mojom::DeviceFactory {
 public:
  void CreateDevice(const std::string& device_id,
                    video_capture::mojom::DeviceRequest device_request,
                    const CreateDeviceCallback& callback) override {
    DoCreateDevice(device_id, &device_request, callback);
  }

  MOCK_METHOD1(GetDeviceInfos, void(const GetDeviceInfosCallback& callback));
  MOCK_METHOD3(DoCreateDevice,
               void(const std::string& device_id,
                    video_capture::mojom::DeviceRequest* device_request,
                    const CreateDeviceCallback& callback));
};

class MockVideoCaptureDeviceLauncherCallbacks
    : public VideoCaptureDeviceLauncher::Callbacks {
 public:
  void OnDeviceLaunched(
      std::unique_ptr<LaunchedVideoCaptureDevice> device) override {
    DoOnDeviceLaunched(&device);
  }

  MOCK_METHOD1(DoOnDeviceLaunched,
               void(std::unique_ptr<LaunchedVideoCaptureDevice>* device));
  MOCK_METHOD0(OnDeviceLaunchFailed, void());
  MOCK_METHOD0(OnDeviceLaunchAborted, void());
};

class ServiceVideoCaptureDeviceLauncherTest : public testing::Test {
 public:
  ServiceVideoCaptureDeviceLauncherTest() {}
  ~ServiceVideoCaptureDeviceLauncherTest() override {}

 protected:
  void SetUp() override {
    factory_binding_ =
        base::MakeUnique<mojo::Binding<video_capture::mojom::DeviceFactory>>(
            &mock_device_factory_, mojo::MakeRequest(&device_factory_));
    launcher_ =
        base::MakeUnique<ServiceVideoCaptureDeviceLauncher>(&device_factory_);
  }

  void TearDown() override {}

  void RunLaunchingDeviceIsAbortedTest(
      video_capture::mojom::DeviceAccessResultCode service_result_code);

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  MockDeviceFactory mock_device_factory_;
  MockVideoCaptureDeviceLauncherCallbacks mock_callbacks_;
  video_capture::mojom::DeviceFactoryPtr device_factory_;
  std::unique_ptr<mojo::Binding<video_capture::mojom::DeviceFactory>>
      factory_binding_;
  std::unique_ptr<ServiceVideoCaptureDeviceLauncher> launcher_;
  base::MockCallback<base::OnceClosure> done_cb;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceVideoCaptureDeviceLauncherTest);
};

TEST_F(ServiceVideoCaptureDeviceLauncherTest, LaunchingDeviceSucceeds) {
  base::RunLoop run_loop;

  EXPECT_CALL(mock_device_factory_, DoCreateDevice(kStubDeviceId, _, _))
      .WillOnce(Invoke([](const std::string& device_id,
                          video_capture::mojom::DeviceRequest* device_request,
                          const video_capture::mojom::DeviceFactory::
                              CreateDeviceCallback& callback) {
        // Note: We must keep |device_request| alive at least until we have
        // sent out the callback. Otherwise, |launcher_| may interpret this
        // as the connection having been lost before receiving the callback.
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::Bind(
                [](video_capture::mojom::DeviceRequest device_request,
                   const video_capture::mojom::DeviceFactory::
                       CreateDeviceCallback& callback) {
                  callback.Run(
                      video_capture::mojom::DeviceAccessResultCode::SUCCESS);
                },
                base::Passed(device_request), callback));
      }));
  EXPECT_CALL(mock_callbacks_, DoOnDeviceLaunched(_)).Times(1);
  EXPECT_CALL(mock_callbacks_, OnDeviceLaunchAborted()).Times(0);
  EXPECT_CALL(mock_callbacks_, OnDeviceLaunchFailed()).Times(0);
  EXPECT_CALL(done_cb, Run()).WillOnce(InvokeWithoutArgs([&run_loop]() {
    run_loop.Quit();
  }));

  // Exercise
  launcher_->LaunchDeviceAsync(
      kStubDeviceId, content::MEDIA_DEVICE_VIDEO_CAPTURE, kArbitraryParams,
      kNullReceiver, &mock_callbacks_, done_cb.Get());

  run_loop.Run();
}

TEST_F(ServiceVideoCaptureDeviceLauncherTest,
       LaunchingDeviceIsAbortedBeforeServiceRespondsWithSuccess) {
  RunLaunchingDeviceIsAbortedTest(
      video_capture::mojom::DeviceAccessResultCode::SUCCESS);
}

TEST_F(ServiceVideoCaptureDeviceLauncherTest,
       LaunchingDeviceIsAbortedBeforeServiceRespondsWithNotFound) {
  RunLaunchingDeviceIsAbortedTest(
      video_capture::mojom::DeviceAccessResultCode::ERROR_DEVICE_NOT_FOUND);
}

TEST_F(ServiceVideoCaptureDeviceLauncherTest,
       LaunchingDeviceIsAbortedBeforeServiceRespondsWithNotInitialized) {
  RunLaunchingDeviceIsAbortedTest(
      video_capture::mojom::DeviceAccessResultCode::NOT_INITIALIZED);
}

void ServiceVideoCaptureDeviceLauncherTest::RunLaunchingDeviceIsAbortedTest(
    video_capture::mojom::DeviceAccessResultCode service_result_code) {
  base::RunLoop step_1_run_loop;
  base::RunLoop step_2_run_loop;

  base::Closure create_device_success_answer_cb;
  EXPECT_CALL(mock_device_factory_, DoCreateDevice(kStubDeviceId, _, _))
      .WillOnce(Invoke([&create_device_success_answer_cb, &step_1_run_loop,
                        service_result_code](
                           const std::string& device_id,
                           video_capture::mojom::DeviceRequest* device_request,
                           const video_capture::mojom::DeviceFactory::
                               CreateDeviceCallback& callback) {
        // Prepare the callback, but save it for now instead of invoking it.
        create_device_success_answer_cb = base::Bind(
            [](video_capture::mojom::DeviceRequest device_request,
               const video_capture::mojom::DeviceFactory::CreateDeviceCallback&
                   callback,
               video_capture::mojom::DeviceAccessResultCode
                   service_result_code) { callback.Run(service_result_code); },
            base::Passed(device_request), callback, service_result_code);
        step_1_run_loop.Quit();
      }));
  EXPECT_CALL(mock_callbacks_, DoOnDeviceLaunched(_)).Times(0);
  EXPECT_CALL(mock_callbacks_, OnDeviceLaunchAborted()).Times(1);
  EXPECT_CALL(mock_callbacks_, OnDeviceLaunchFailed()).Times(0);
  EXPECT_CALL(done_cb, Run()).WillOnce(InvokeWithoutArgs([&step_2_run_loop]() {
    step_2_run_loop.Quit();
  }));

  // Exercise
  launcher_->LaunchDeviceAsync(
      kStubDeviceId, content::MEDIA_DEVICE_VIDEO_CAPTURE, kArbitraryParams,
      kNullReceiver, &mock_callbacks_, done_cb.Get());
  step_1_run_loop.Run();
  launcher_->AbortLaunch();

  create_device_success_answer_cb.Run();
  step_2_run_loop.Run();
}

TEST_F(ServiceVideoCaptureDeviceLauncherTest,
       LaunchingDeviceFailsBecauseDeviceNotFound) {
  base::RunLoop run_loop;

  EXPECT_CALL(mock_device_factory_, DoCreateDevice(kStubDeviceId, _, _))
      .WillOnce(Invoke(
          [](const std::string& device_id,
             video_capture::mojom::DeviceRequest* device_request,
             const video_capture::mojom::DeviceFactory::CreateDeviceCallback&
                 callback) {
            // Note: We must keep |device_request| alive at least until we have
            // sent out the callback. Otherwise, |launcher_| may interpret this
            // as the connection having been lost before receiving the callback.
            base::ThreadTaskRunnerHandle::Get()->PostTask(
                FROM_HERE,
                base::Bind(
                    [](video_capture::mojom::DeviceRequest device_request,
                       const video_capture::mojom::DeviceFactory::
                           CreateDeviceCallback& callback) {
                      callback.Run(
                          video_capture::mojom::DeviceAccessResultCode::
                              ERROR_DEVICE_NOT_FOUND);
                    },
                    base::Passed(device_request), callback));
          }));
  EXPECT_CALL(mock_callbacks_, DoOnDeviceLaunched(_)).Times(0);
  EXPECT_CALL(mock_callbacks_, OnDeviceLaunchAborted()).Times(0);
  EXPECT_CALL(mock_callbacks_, OnDeviceLaunchFailed()).Times(1);
  EXPECT_CALL(done_cb, Run()).WillOnce(InvokeWithoutArgs([&run_loop]() {
    run_loop.Quit();
  }));

  // Exercise
  launcher_->LaunchDeviceAsync(
      kStubDeviceId, content::MEDIA_DEVICE_VIDEO_CAPTURE, kArbitraryParams,
      kNullReceiver, &mock_callbacks_, done_cb.Get());

  run_loop.Run();
}

TEST_F(ServiceVideoCaptureDeviceLauncherTest,
       LaunchingDeviceFailsBecauseFactoryIsUnbound) {
  base::RunLoop run_loop;

  EXPECT_CALL(mock_callbacks_, DoOnDeviceLaunched(_)).Times(0);
  EXPECT_CALL(mock_callbacks_, OnDeviceLaunchAborted()).Times(0);
  EXPECT_CALL(mock_callbacks_, OnDeviceLaunchFailed()).Times(1);
  EXPECT_CALL(done_cb, Run()).WillOnce(InvokeWithoutArgs([&run_loop]() {
    run_loop.Quit();
  }));

  // Exercise
  device_factory_.reset();
  launcher_->LaunchDeviceAsync(
      kStubDeviceId, content::MEDIA_DEVICE_VIDEO_CAPTURE, kArbitraryParams,
      kNullReceiver, &mock_callbacks_, done_cb.Get());

  run_loop.Run();
}

TEST_F(ServiceVideoCaptureDeviceLauncherTest,
       LaunchingDeviceFailsBecauseConnectionLostWhileLaunching) {
  base::RunLoop run_loop;

  video_capture::mojom::DeviceFactory::CreateDeviceCallback create_device_cb;
  EXPECT_CALL(mock_device_factory_, DoCreateDevice(kStubDeviceId, _, _))
      .WillOnce(Invoke(
          [&create_device_cb](
              const std::string& device_id,
              video_capture::mojom::DeviceRequest* device_request,
              const video_capture::mojom::DeviceFactory::CreateDeviceCallback&
                  callback) {
            // Simulate connection lost by not invoking |callback| and releasing
            // |device_request|. We have to save |callback| and invoke it later
            // to avoid hitting a DCHECK.
            create_device_cb = callback;
          }));
  EXPECT_CALL(mock_callbacks_, DoOnDeviceLaunched(_)).Times(0);
  EXPECT_CALL(mock_callbacks_, OnDeviceLaunchAborted()).Times(0);
  EXPECT_CALL(mock_callbacks_, OnDeviceLaunchFailed()).Times(1);
  EXPECT_CALL(done_cb, Run()).WillOnce(InvokeWithoutArgs([&run_loop]() {
    run_loop.Quit();
  }));

  // Exercise
  launcher_->LaunchDeviceAsync(
      kStubDeviceId, content::MEDIA_DEVICE_VIDEO_CAPTURE, kArbitraryParams,
      kNullReceiver, &mock_callbacks_, done_cb.Get());

  run_loop.Run();

  // Cut the connection to the factory, so that the outstanding
  // |create_device_cb| will be dropped.
  factory_binding_.reset();
  // We have to invoke the callback, because not doing so triggers a DCHECK.
  const video_capture::mojom::DeviceAccessResultCode arbitrary_result_code =
      video_capture::mojom::DeviceAccessResultCode::SUCCESS;
  create_device_cb.Run(arbitrary_result_code);
}

}  // namespace content
