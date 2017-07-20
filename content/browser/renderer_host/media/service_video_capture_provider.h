// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_SERVICE_VIDEO_CAPTURE_PROVIDER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_SERVICE_VIDEO_CAPTURE_PROVIDER_H_

#include "base/threading/thread_checker.h"
#include "content/browser/renderer_host/media/video_capture_provider.h"
#include "services/video_capture/public/interfaces/device_factory.mojom.h"
#include "services/video_capture/public/interfaces/device_factory_provider.mojom.h"

namespace content {

// Implementation of VideoCaptureProvider that uses the "video_capture" service.
class CONTENT_EXPORT ServiceVideoCaptureProvider : public VideoCaptureProvider {
 public:
  class ServiceConnector {
   public:
    virtual ~ServiceConnector() {}
    virtual void BindFactoryProvider(
        video_capture::mojom::DeviceFactoryProviderPtr* provider) = 0;
  };

  // The parameterless constructor creates a default ServiceConnector which
  // uses the ServiceManager associated with the current process to connect
  // to the video capture service.
  ServiceVideoCaptureProvider();
  // Lets clients provide a custom ServiceConnector.
  explicit ServiceVideoCaptureProvider(
      std::unique_ptr<ServiceConnector> service_connector);
  ~ServiceVideoCaptureProvider() override;

  void Uninitialize() override;
  void GetDeviceInfosAsync(GetDeviceInfosCallback result_callback) override;
  std::unique_ptr<VideoCaptureDeviceLauncher> CreateDeviceLauncher() override;

 private:
  enum class ReasonForUninitialize {
    kShutdown,
    kClientRequest,
    kConnectionLost
  };

  void LazyConnectToService();
  void OnLostConnectionToDeviceFactory();
  void UninitializeInternal(ReasonForUninitialize reason);

  std::unique_ptr<ServiceConnector> service_connector_;
  // We must hold on to |device_factory_provider_| because it holds the
  // service-side binding for |device_factory_|.
  video_capture::mojom::DeviceFactoryProviderPtr device_factory_provider_;
  video_capture::mojom::DeviceFactoryPtr device_factory_;
  SEQUENCE_CHECKER(sequence_checker_);

  bool has_created_device_launcher_;
  base::TimeTicks time_of_last_connect_;
  base::TimeTicks time_of_last_uninitialize_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_SERVICE_VIDEO_CAPTURE_PROVIDER_H_
