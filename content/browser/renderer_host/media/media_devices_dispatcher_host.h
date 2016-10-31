// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_DEVICES_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_DEVICES_DISPATCHER_HOST_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "content/browser/media/media_devices_permission_checker.h"
#include "content/browser/renderer_host/media/media_devices_manager.h"
#include "content/common/content_export.h"
#include "content/common/media/media_devices.mojom.h"

using ::mojom::MediaDeviceType;

namespace url {
class Origin;
}

namespace content {

class MediaStreamManager;

class CONTENT_EXPORT MediaDevicesDispatcherHost
    : public ::mojom::MediaDevicesDispatcherHost {
 public:
  MediaDevicesDispatcherHost(int render_process_id,
                             int routing_id,
                             const std::string& device_id_salt,
                             MediaStreamManager* media_stream_manager);
  ~MediaDevicesDispatcherHost() override;

  static void Create(int render_process_id,
                     int routing_id,
                     const std::string& device_id_salt,
                     MediaStreamManager* media_stream_manager,
                     ::mojom::MediaDevicesDispatcherHostRequest request);

  // ::mojom::MediaDevicesDispatcherHost implementation.
  void EnumerateDevices(
      bool request_audio_input,
      bool request_video_input,
      bool request_audio_output,
      const url::Origin& security_origin,
      const EnumerateDevicesCallback& client_callback) override;

  void SetPermissionChecker(
      const MediaDevicesPermissionChecker& permission_checker);

 private:
  void DoEnumerateDevices(
      const MediaDevicesManager::BoolDeviceTypes& requested_types,
      const url::Origin& security_origin,
      const EnumerateDevicesCallback& client_callback,
      const MediaDevicesManager::BoolDeviceTypes& permissions);

  void DevicesEnumerated(
      const MediaDevicesManager::BoolDeviceTypes& requested_types,
      const url::Origin& security_origin,
      const EnumerateDevicesCallback& client_callback,
      const MediaDevicesManager::BoolDeviceTypes& has_permissions,
      const MediaDeviceEnumeration& enumeration);

  int render_process_id_;
  int routing_id_;
  std::string device_id_salt_;
  std::string group_id_salt_;
  MediaStreamManager* media_stream_manager_;
  MediaDevicesPermissionChecker permission_checker_;

  base::WeakPtrFactory<MediaDevicesDispatcherHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaDevicesDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_DEVICES_DISPATCHER_HOST_H_
