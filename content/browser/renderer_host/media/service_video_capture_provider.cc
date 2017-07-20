// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/service_video_capture_provider.h"

#include "content/browser/renderer_host/media/service_video_capture_device_launcher.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/video_capture/public/interfaces/constants.mojom.h"
#include "services/video_capture/public/uma/video_capture_service_event.h"

namespace content {

ServiceVideoCaptureProvider::ServiceVideoCaptureProvider()
    : has_created_device_launcher_(false) {
  sequence_checker_.DetachFromSequence();
  DCHECK(BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  connector_ =
      ServiceManagerConnection::GetForProcess()->GetConnector()->Clone();
}

ServiceVideoCaptureProvider::~ServiceVideoCaptureProvider() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  UninitializeInternal(ReasonForUninitialize::kShutdown);
}

void ServiceVideoCaptureProvider::Uninitialize() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  UninitializeInternal(ReasonForUninitialize::kClientRequest);
}

void ServiceVideoCaptureProvider::GetDeviceInfosAsync(
    const base::Callback<void(
        const std::vector<media::VideoCaptureDeviceInfo>&)>& result_callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  LazyConnectToService();
  device_factory_->GetDeviceInfos(result_callback);
}

std::unique_ptr<VideoCaptureDeviceLauncher>
ServiceVideoCaptureProvider::CreateDeviceLauncher() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  LazyConnectToService();
  has_created_device_launcher_ = true;
  return base::MakeUnique<ServiceVideoCaptureDeviceLauncher>(&device_factory_);
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

  connector_->BindInterface(video_capture::mojom::kServiceName,
                            &device_factory_provider_);
  device_factory_provider_->ConnectToDeviceFactory(
      mojo::MakeRequest(&device_factory_));
  // Unretained |this| is safe, because |this| owns |device_factory_|.
  device_factory_.set_connection_error_handler(
      base::Bind(&ServiceVideoCaptureProvider::OnLostConnectionToDeviceFactory,
                 base::Unretained(this)));
}

void ServiceVideoCaptureProvider::OnLostConnectionToDeviceFactory() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  // This may indicate that the video capture service has crashed. Uninitialize
  // here, so that a new connection will be established when clients try to
  // reconnect.
  UninitializeInternal(ReasonForUninitialize::kConnectionLost);
}

void ServiceVideoCaptureProvider::UninitializeInternal(
    ReasonForUninitialize reason) {
  if (!device_factory_.is_bound()) {
    return;
  }
  base::TimeDelta duration_since_last_connect(base::TimeTicks::Now() -
                                              time_of_last_connect_);
  switch (reason) {
    case ReasonForUninitialize::kShutdown:
    case ReasonForUninitialize::kClientRequest:
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
