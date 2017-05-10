// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_SERVICE_VIDEO_CAPTURE_PROVIDER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_SERVICE_VIDEO_CAPTURE_PROVIDER_H_

#include "base/threading/thread_checker.h"
#include "content/browser/renderer_host/media/video_capture_provider.h"
#include "services/video_capture/public/interfaces/device_factory.mojom.h"
#include "services/video_capture/public/interfaces/device_factory_provider.mojom.h"

namespace service_manager {
class Connector;
}

namespace content {

// Implementation of VideoCaptureProvider that uses the "video_capture" service.
class CONTENT_EXPORT ServiceVideoCaptureProvider : public VideoCaptureProvider {
 public:
  ServiceVideoCaptureProvider();
  ~ServiceVideoCaptureProvider() override;

  void Uninitialize() override;

  void GetDeviceInfosAsync(
      const base::Callback<void(
          const std::vector<media::VideoCaptureDeviceInfo>&)>& result_callback)
      override;

  std::unique_ptr<VideoCaptureDeviceLauncher> CreateDeviceLauncher() override;

 private:
  void LazyConnectToService();
  void OnLostConnectionToDeviceFactory();

  std::unique_ptr<service_manager::Connector> connector_;
  // We must hold on to |device_factory_provider_| because it holds the
  // service-side binding for |device_factory_|.
  video_capture::mojom::DeviceFactoryProviderPtr device_factory_provider_;
  video_capture::mojom::DeviceFactoryPtr device_factory_;
  base::SequenceChecker sequence_checker_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_SERVICE_VIDEO_CAPTURE_PROVIDER_H_
