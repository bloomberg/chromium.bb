// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/service_video_capture_provider.h"

#include "content/browser/renderer_host/media/service_video_capture_device_launcher.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "media/base/scoped_callback_runner.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/video_capture/public/interfaces/constants.mojom.h"
#include "services/video_capture/public/uma/video_capture_service_event.h"

namespace {

class ServiceConnectorImpl
    : public content::ServiceVideoCaptureProvider::ServiceConnector {
 public:
  ServiceConnectorImpl() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    connector_ = content::ServiceManagerConnection::GetForProcess()
                     ->GetConnector()
                     ->Clone();
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  void BindFactoryProvider(
      video_capture::mojom::DeviceFactoryProviderPtr* provider) override {
    connector_->BindInterface(video_capture::mojom::kServiceName, provider);
  }

 private:
  std::unique_ptr<service_manager::Connector> connector_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // anonymous namespace

namespace content {

ServiceVideoCaptureProvider::ServiceVideoCaptureProvider()
    : ServiceVideoCaptureProvider(base::MakeUnique<ServiceConnectorImpl>()) {}

ServiceVideoCaptureProvider::ServiceVideoCaptureProvider(
    std::unique_ptr<ServiceConnector> service_connector)
    : service_connector_(std::move(service_connector)),
      usage_count_(0),
      has_created_device_launcher_(false),
      weak_ptr_factory_(this) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ServiceVideoCaptureProvider::~ServiceVideoCaptureProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UninitializeInternal(ReasonForUninitialize::kShutdown);
}

void ServiceVideoCaptureProvider::GetDeviceInfosAsync(
    GetDeviceInfosCallback result_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  IncreaseUsageCount();
  LazyConnectToService();
  // Use a ScopedCallbackRunner to make sure that |result_callback| gets
  // invoked with an empty result in case that the service drops the request.
  device_factory_->GetDeviceInfos(media::ScopedCallbackRunner(
      base::BindOnce(&ServiceVideoCaptureProvider::OnDeviceInfosReceived,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(result_callback)),
      std::vector<media::VideoCaptureDeviceInfo>()));
}

std::unique_ptr<VideoCaptureDeviceLauncher>
ServiceVideoCaptureProvider::CreateDeviceLauncher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  IncreaseUsageCount();
  LazyConnectToService();
  has_created_device_launcher_ = true;
  return base::MakeUnique<ServiceVideoCaptureDeviceLauncher>(
      &device_factory_,
      base::BindOnce(&ServiceVideoCaptureProvider::DecreaseUsageCount,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ServiceVideoCaptureProvider::LazyConnectToService() {
  if (device_factory_provider_.is_bound())
    return;

  video_capture::uma::LogVideoCaptureServiceEvent(
      video_capture::uma::BROWSER_CONNECTING_TO_SERVICE);
  if (time_of_last_uninitialize_ != base::TimeTicks()) {
    if (has_created_device_launcher_) {
      video_capture::uma::LogDurationUntilReconnectAfterCapture(
          base::TimeTicks::Now() - time_of_last_uninitialize_);
    } else {
      video_capture::uma::LogDurationUntilReconnectAfterEnumerationOnly(
          base::TimeTicks::Now() - time_of_last_uninitialize_);
    }
  }

  has_created_device_launcher_ = false;
  time_of_last_connect_ = base::TimeTicks::Now();

  service_connector_->BindFactoryProvider(&device_factory_provider_);
  device_factory_provider_->ConnectToDeviceFactory(
      mojo::MakeRequest(&device_factory_));
  // Unretained |this| is safe, because |this| owns |device_factory_|.
  device_factory_.set_connection_error_handler(base::BindOnce(
      &ServiceVideoCaptureProvider::OnLostConnectionToDeviceFactory,
      base::Unretained(this)));
}

void ServiceVideoCaptureProvider::OnDeviceInfosReceived(
    GetDeviceInfosCallback result_callback,
    const std::vector<media::VideoCaptureDeviceInfo>& infos) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ResetAndReturn(&result_callback).Run(infos);
  DecreaseUsageCount();
}

void ServiceVideoCaptureProvider::OnLostConnectionToDeviceFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This may indicate that the video capture service has crashed. Uninitialize
  // here, so that a new connection will be established when clients try to
  // reconnect.
  UninitializeInternal(ReasonForUninitialize::kConnectionLost);
}

void ServiceVideoCaptureProvider::IncreaseUsageCount() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  usage_count_++;
}

void ServiceVideoCaptureProvider::DecreaseUsageCount() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  usage_count_--;
  DCHECK_GE(usage_count_, 0);
  if (usage_count_ == 0)
    UninitializeInternal(ReasonForUninitialize::kUnused);
}

void ServiceVideoCaptureProvider::UninitializeInternal(
    ReasonForUninitialize reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!device_factory_.is_bound()) {
    return;
  }
  base::TimeDelta duration_since_last_connect(base::TimeTicks::Now() -
                                              time_of_last_connect_);
  switch (reason) {
    case ReasonForUninitialize::kShutdown:
    case ReasonForUninitialize::kUnused:
      if (has_created_device_launcher_) {
        video_capture::uma::LogVideoCaptureServiceEvent(
            video_capture::uma::
                BROWSER_CLOSING_CONNECTION_TO_SERVICE_AFTER_CAPTURE);
        video_capture::uma::
            LogDurationFromLastConnectToClosingConnectionAfterCapture(
                duration_since_last_connect);
      } else {
        video_capture::uma::LogVideoCaptureServiceEvent(
            video_capture::uma::
                BROWSER_CLOSING_CONNECTION_TO_SERVICE_AFTER_ENUMERATION_ONLY);
        video_capture::uma::
            LogDurationFromLastConnectToClosingConnectionAfterEnumerationOnly(
                duration_since_last_connect);
      }
      break;
    case ReasonForUninitialize::kConnectionLost:
      video_capture::uma::LogVideoCaptureServiceEvent(
          video_capture::uma::BROWSER_LOST_CONNECTION_TO_SERVICE);
      video_capture::uma::LogDurationFromLastConnectToConnectionLost(
          duration_since_last_connect);
      break;
  }
  device_factory_.reset();
  device_factory_provider_.reset();
  time_of_last_uninitialize_ = base::TimeTicks::Now();
}

}  // namespace content
