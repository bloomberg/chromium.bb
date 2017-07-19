// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_DEVICES_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_DEVICES_DISPATCHER_HOST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "content/browser/media/media_devices_permission_checker.h"
#include "content/browser/renderer_host/media/media_devices_manager.h"
#include "content/common/content_export.h"
#include "content/common/media/media_devices.mojom.h"
#include "media/capture/video/video_capture_device_descriptor.h"
#include "url/origin.h"

using ::mojom::MediaDeviceType;

namespace content {

class MediaStreamManager;

class CONTENT_EXPORT MediaDevicesDispatcherHost
    : public ::mojom::MediaDevicesDispatcherHost,
      public MediaDeviceChangeSubscriber {
 public:
  MediaDevicesDispatcherHost(int render_process_id,
                             int render_frame_id,
                             const std::string& device_id_salt,
                             MediaStreamManager* media_stream_manager);
  ~MediaDevicesDispatcherHost() override;

  static void Create(int render_process_id,
                     int render_frame_id,
                     const std::string& device_id_salt,
                     MediaStreamManager* media_stream_manager,
                     ::mojom::MediaDevicesDispatcherHostRequest request);

  // ::mojom::MediaDevicesDispatcherHost implementation.
  void EnumerateDevices(bool request_audio_input,
                        bool request_video_input,
                        bool request_audio_output,
                        EnumerateDevicesCallback client_callback) override;
  void GetVideoInputCapabilities(
      GetVideoInputCapabilitiesCallback client_callback) override;
  void GetAudioInputCapabilities(
      GetAudioInputCapabilitiesCallback client_callback) override;
  void SubscribeDeviceChangeNotifications(MediaDeviceType type,
                                          uint32_t subscription_id) override;
  void UnsubscribeDeviceChangeNotifications(MediaDeviceType type,
                                            uint32_t subscription_id) override;

  // MediaDeviceChangeSubscriber implementation.
  void OnDevicesChanged(MediaDeviceType type,
                        const MediaDeviceInfoArray& device_infos) override;

  void SetPermissionChecker(
      std::unique_ptr<MediaDevicesPermissionChecker> permission_checker);

  void SetDeviceChangeListenerForTesting(
      ::mojom::MediaDevicesListenerPtr listener);

  void SetSecurityOriginForTesting(const url::Origin& origin);

 private:
  void CheckPermissionsForEnumerateDevices(
      const MediaDevicesManager::BoolDeviceTypes& requested_types,
      EnumerateDevicesCallback client_callback,
      const url::Origin& security_origin);

  void DoEnumerateDevices(
      const MediaDevicesManager::BoolDeviceTypes& requested_types,
      EnumerateDevicesCallback client_callback,
      const url::Origin& security_origin,
      const MediaDevicesManager::BoolDeviceTypes& has_permissions);

  void DevicesEnumerated(
      const MediaDevicesManager::BoolDeviceTypes& requested_types,
      EnumerateDevicesCallback client_callback,
      const url::Origin& security_origin,
      const MediaDevicesManager::BoolDeviceTypes& has_permissions,
      const MediaDeviceEnumeration& enumeration);

  void GetDefaultVideoInputDeviceID(
      GetVideoInputCapabilitiesCallback client_callback,
      const url::Origin& security_origin);

  void GotDefaultVideoInputDeviceID(
      GetVideoInputCapabilitiesCallback client_callback,
      const url::Origin& security_origin,
      const std::string& default_device_id);

  void FinalizeGetVideoInputCapabilities(
      GetVideoInputCapabilitiesCallback client_callback,
      const url::Origin& security_origin,
      const std::string& default_device_id,
      const media::VideoCaptureDeviceDescriptors& device_descriptors);

  void GetDefaultAudioInputDeviceID(
      GetAudioInputCapabilitiesCallback client_callback,
      const url::Origin& security_origin);

  void GotDefaultAudioInputDeviceID(const std::string& default_device_id);

  void GotAudioInputEnumeration(const std::string& default_device_id,
                                const MediaDeviceEnumeration& enumeration);

  void GotAudioInputParameters(size_t index,
                               const media::AudioParameters& parameters);

  void FinalizeGetAudioInputCapabilities();

  // Returns the currently supported video formats for the given |device_id|.
  media::VideoCaptureFormats GetVideoInputFormats(const std::string& device_id);

  void NotifyDeviceChangeOnUIThread(const std::vector<uint32_t>& subscriptions,
                                    MediaDeviceType type,
                                    const MediaDeviceInfoArray& device_infos);

  // The following const fields can be accessed on any thread.
  const int render_process_id_;
  const int render_frame_id_;
  const std::string device_id_salt_;
  const std::string group_id_salt_;

  // The following fields can only be accessed on the IO thread.
  MediaStreamManager* media_stream_manager_;
  std::unique_ptr<MediaDevicesPermissionChecker> permission_checker_;
  std::vector<uint32_t> device_change_subscriptions_[NUM_MEDIA_DEVICE_TYPES];

  // This field can only be accessed on the UI thread.
  ::mojom::MediaDevicesListenerPtr device_change_listener_;
  url::Origin security_origin_for_testing_;

  struct AudioInputCapabilitiesRequest;
  // Queued requests for audio-input capabilities.
  std::vector<AudioInputCapabilitiesRequest>
      pending_audio_input_capabilities_requests_;
  size_t num_pending_audio_input_parameters_;
  std::vector<::mojom::AudioInputDeviceCapabilities>
      current_audio_input_capabilities_;

  base::WeakPtrFactory<MediaDevicesDispatcherHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaDevicesDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_DEVICES_DISPATCHER_HOST_H_
