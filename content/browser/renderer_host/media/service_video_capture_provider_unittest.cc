// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/service_video_capture_provider.h"

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/video_capture/public/interfaces/device_factory.mojom.h"
#include "services/video_capture/public/interfaces/device_factory_provider.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;
using testing::Invoke;
using testing::_;

namespace content {

class MockServiceConnector
    : public ServiceVideoCaptureProvider::ServiceConnector {
 public:
  MockServiceConnector() {}

  MOCK_METHOD1(BindFactoryProvider,
               void(video_capture::mojom::DeviceFactoryProviderPtr* provider));
};

class MockDeviceFactoryProvider
    : public video_capture::mojom::DeviceFactoryProvider {
 public:
  void ConnectToDeviceFactory(
      video_capture::mojom::DeviceFactoryRequest request) override {
    DoConnectToDeviceFactory(request);
  }

  MOCK_METHOD1(SetShutdownDelayInSeconds, void(float seconds));
  MOCK_METHOD1(DoConnectToDeviceFactory,
               void(video_capture::mojom::DeviceFactoryRequest& request));
};

class MockDeviceFactory : public video_capture::mojom::DeviceFactory {
 public:
  void GetDeviceInfos(GetDeviceInfosCallback callback) override {
    DoGetDeviceInfos(callback);
  }
  void CreateDevice(const std::string& device_id,
                    video_capture::mojom::DeviceRequest device_request,
                    CreateDeviceCallback callback) override {
    DoCreateDevice(device_id, &device_request, callback);
  }

  MOCK_METHOD1(DoGetDeviceInfos, void(GetDeviceInfosCallback& callback));
  MOCK_METHOD3(DoCreateDevice,
               void(const std::string& device_id,
                    video_capture::mojom::DeviceRequest* device_request,
                    CreateDeviceCallback& callback));
};

class ServiceVideoCaptureProviderTest : public testing::Test {
 public:
  ServiceVideoCaptureProviderTest()
      : factory_provider_binding_(&mock_device_factory_provider_),
        device_factory_binding_(&mock_device_factory_) {}
  ~ServiceVideoCaptureProviderTest() override {}

 protected:
  void SetUp() override {
    auto mock_service_connector = base::MakeUnique<MockServiceConnector>();
    mock_service_connector_ = mock_service_connector.get();
    provider_ = base::MakeUnique<ServiceVideoCaptureProvider>(
        std::move(mock_service_connector));
  }

  void TearDown() override {}

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  MockServiceConnector* mock_service_connector_;
  MockDeviceFactoryProvider mock_device_factory_provider_;
  mojo::Binding<video_capture::mojom::DeviceFactoryProvider>
      factory_provider_binding_;
  MockDeviceFactory mock_device_factory_;
  mojo::Binding<video_capture::mojom::DeviceFactory> device_factory_binding_;
  std::unique_ptr<ServiceVideoCaptureProvider> provider_;
  base::MockCallback<VideoCaptureProvider::GetDeviceInfosCallback> results_cb_;
  base::MockCallback<
      video_capture::mojom::DeviceFactory::GetDeviceInfosCallback>
      service_cb_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceVideoCaptureProviderTest);
};

// Tests that if connection to the service is lost during an outstanding call
// to GetDeviceInfos(), the callback passed into GetDeviceInfos() still gets
// invoked.
TEST_F(ServiceVideoCaptureProviderTest,
       GetDeviceInfosAsyncInvokesCallbackWhenLosingConnection) {
  base::RunLoop run_loop;

  EXPECT_CALL(*mock_service_connector_, BindFactoryProvider(_))
      .WillOnce(Invoke([this](video_capture::mojom::DeviceFactoryProviderPtr*
                                  device_factory_provider) {
        factory_provider_binding_.Bind(
            mojo::MakeRequest(device_factory_provider));
      }));
  EXPECT_CALL(mock_device_factory_provider_, DoConnectToDeviceFactory(_))
      .WillOnce(
          Invoke([this](video_capture::mojom::DeviceFactoryRequest& request) {
            device_factory_binding_.Bind(std::move(request));
          }));
  video_capture::mojom::DeviceFactory::GetDeviceInfosCallback
      callback_to_be_called_by_service;
  base::RunLoop wait_for_call_to_arrive_at_service;
  EXPECT_CALL(mock_device_factory_, DoGetDeviceInfos(_))
      .WillOnce(Invoke(
          [&callback_to_be_called_by_service,
           &wait_for_call_to_arrive_at_service](
              video_capture::mojom::DeviceFactory::GetDeviceInfosCallback&
                  callback) {
            // Hold on to the callback so we can drop it later.
            callback_to_be_called_by_service = std::move(callback);
            wait_for_call_to_arrive_at_service.Quit();
          }));
  base::RunLoop wait_for_callback_from_service;
  EXPECT_CALL(results_cb_, Run(_))
      .WillOnce(Invoke(
          [&wait_for_callback_from_service](
              const std::vector<media::VideoCaptureDeviceInfo>& results) {
            EXPECT_EQ(0u, results.size());
            wait_for_callback_from_service.Quit();
          }));

  // Exercise
  provider_->GetDeviceInfosAsync(results_cb_.Get());
  wait_for_call_to_arrive_at_service.Run();

  // Simulate that the service goes down by cutting the connections.
  device_factory_binding_.Close();
  factory_provider_binding_.Close();

  wait_for_callback_from_service.Run();
}

}  // namespace content
