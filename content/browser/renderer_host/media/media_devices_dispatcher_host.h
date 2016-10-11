// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_DEVICES_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_DEVICES_DISPATCHER_HOST_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "content/browser/renderer_host/media/media_devices_manager.h"
#include "content/common/content_export.h"
#include "content/common/media/media_devices.mojom.h"

using ::mojom::MediaDeviceType;

namespace url {
class Origin;
}

namespace content {

class MediaStreamManager;
class MediaStreamUIProxy;

class CONTENT_EXPORT MediaDevicesDispatcherHost
    : public ::mojom::MediaDevicesDispatcherHost {
 public:
  MediaDevicesDispatcherHost(int render_process_id,
                             int routing_id,
                             const std::string& device_id_salt,
                             MediaStreamManager* media_stream_manager,
                             bool use_fake_ui);
  ~MediaDevicesDispatcherHost() override;

  static void Create(int render_process_id,
                     int routing_id,
                     const std::string& device_id_salt,
                     MediaStreamManager* media_stream_manager,
                     bool use_fake_ui,
                     ::mojom::MediaDevicesDispatcherHostRequest request);

  // ::mojom::MediaDevicesDispatcherHost implementation.
  void EnumerateDevices(
      bool request_audio_input,
      bool request_video_input,
      bool request_audio_output,
      const url::Origin& security_origin,
      const EnumerateDevicesCallback& client_callback) override;

  // Sets a MediaStreamUIProxy to be used in a single run of EnumerateDevices(),
  // provided that the MediaDevicesDispatcherHost was created with |use_fake_ui|
  // set to true. Subsequent runs of EnumerateDevices() use a new internally
  // generated fake MediaStreamUIProxy.
  // |fake_ui_proxy| is ignored if the MediaDevicesDispatcherHost was created
  // with |use_fake_ui|, in which case a real MediaStreamUIProxy will be used
  // instead.
  // This function is intended to be used for testing.
  void SetFakeUIProxyForTesting(
      std::unique_ptr<MediaStreamUIProxy> fake_ui_proxy);

 private:
  // Internal type that represents a callback that receives a
  // MediaDevicesManager::BoolDeviceTypes containing the permissions for each
  // device type.
  using AccessCheckedCallback =
      base::Callback<void(const MediaDevicesManager::BoolDeviceTypes&)>;

  // Currently, the same permission (MEDIA_DEVICE_AUDIO_CAPTURE) is used for
  // both audio input and output.
  // TODO(guidou): use specific permission for audio output when it becomes
  // available. See http://crbug.com/556542.
  void CheckAccess(bool check_audio,
                   bool check_video_input,
                   const url::Origin& security_origin,
                   const AccessCheckedCallback& callback);

  void AudioAccessChecked(std::unique_ptr<MediaStreamUIProxy> ui_proxy,
                          bool check_video_permission,
                          const url::Origin& security_origin,
                          const AccessCheckedCallback& callback,
                          bool has_audio_permission);

  void VideoAccessChecked(std::unique_ptr<MediaStreamUIProxy> ui_proxy,
                          bool has_audio_permission,
                          const AccessCheckedCallback& callback,
                          bool has_video_permission);

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

  std::unique_ptr<MediaStreamUIProxy> GetUIProxy();

  int render_process_id_;
  int routing_id_;
  std::string device_id_salt_;
  std::string group_id_salt_;
  MediaStreamManager* media_stream_manager_;
  bool use_fake_ui_;
  std::unique_ptr<MediaStreamUIProxy> fake_ui_proxy_;

  base::WeakPtrFactory<MediaDevicesDispatcherHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaDevicesDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_DEVICES_DISPATCHER_HOST_H_
