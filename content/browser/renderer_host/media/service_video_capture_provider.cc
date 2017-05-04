// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/service_video_capture_provider.h"

#include "content/browser/renderer_host/media/service_video_capture_device_launcher.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/video_capture/public/interfaces/constants.mojom.h"

namespace content {

ServiceVideoCaptureProvider::ServiceVideoCaptureProvider() {
  thread_checker_.DetachFromThread();
  DCHECK(BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  connector_ =
      ServiceManagerConnection::GetForProcess()->GetConnector()->Clone();
}

ServiceVideoCaptureProvider::~ServiceVideoCaptureProvider() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void ServiceVideoCaptureProvider::Uninitialize() {
  DCHECK(thread_checker_.CalledOnValidThread());
  device_factory_.reset();
  device_factory_provider_.reset();
}

void ServiceVideoCaptureProvider::GetDeviceInfosAsync(
    const base::Callback<void(
        const std::vector<media::VideoCaptureDeviceInfo>&)>& result_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  LazyConnectToService();
  device_factory_->GetDeviceInfos(result_callback);
}

std::unique_ptr<VideoCaptureDeviceLauncher>
ServiceVideoCaptureProvider::CreateDeviceLauncher() {
  DCHECK(thread_checker_.CalledOnValidThread());
  LazyConnectToService();
  return base::MakeUnique<ServiceVideoCaptureDeviceLauncher>(&device_factory_);
}

void ServiceVideoCaptureProvider::LazyConnectToService() {
  if (device_factory_provider_.is_bound())
    return;

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
  DCHECK(thread_checker_.CalledOnValidThread());
  // This may indicate that the video capture service has crashed. Uninitialize
  // here, so that a new connection will be established when clients try to
  // reconnect.
  Uninitialize();
}

}  // namespace content
