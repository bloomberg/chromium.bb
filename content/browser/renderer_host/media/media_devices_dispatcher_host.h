// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_DEVICES_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_DEVICES_DISPATCHER_HOST_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "content/browser/media/media_devices_permission_checker.h"
#include "content/browser/media/media_devices_util.h"
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
                             MediaStreamManager* media_stream_manager);
  ~MediaDevicesDispatcherHost() override;

  static void Create(int render_process_id,
                     int render_frame_id,
                     MediaStreamManager* media_stream_manager,
                     ::mojom::MediaDevicesDispatcherHostRequest request);

  // ::mojom::MediaDevicesDispatcherHost implementation.
  void EnumerateDevices(bool request_audio_input,
                        bool request_video_input,
                        bool request_audio_output,
                        EnumerateDevicesCallback client_callback) override;
  void GetVideoInputCapabilities(
      GetVideoInputCapabilitiesCallback client_callback) override;
  void GetAllVideoInputDeviceFormats(
      const std::string& device_id,
      GetAllVideoInputDeviceFormatsCallback client_callback) override;
  void GetAvailableVideoInputDeviceFormats(
      const std::string& device_id,
      GetAvailableVideoInputDeviceFormatsCallback client_callback) override;
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

  void set_salt_and_origin_callback_for_testing(
      MediaDeviceSaltAndOriginCallback callback) {
    salt_and_origin_callback_ = std::move(callback);
  }

 private:
  using GetVideoInputDeviceFormatsCallback =
      GetAllVideoInputDeviceFormatsCallback;

  void CheckPermissionsForEnumerateDevices(
      const MediaDevicesManager::BoolDeviceTypes& requested_types,
      EnumerateDevicesCallback client_callback,
      const std::pair<std::string, url::Origin>& salt_and_origin);

  void DoEnumerateDevices(
      const MediaDevicesManager::BoolDeviceTypes& requested_types,
      EnumerateDevicesCallback client_callback,
      std::string device_id_salt,
      const url::Origin& security_origin,
      const MediaDevicesManager::BoolDeviceTypes& has_permissions);

  void DevicesEnumerated(
      const MediaDevicesManager::BoolDeviceTypes& requested_types,
      EnumerateDevicesCallback client_callback,
      const std::string& device_id_salt,
      const url::Origin& security_origin,
      const MediaDevicesManager::BoolDeviceTypes& has_permissions,
      const MediaDeviceEnumeration& enumeration);

  void GetDefaultVideoInputDeviceID(
      GetVideoInputCapabilitiesCallback client_callback,
      const std::pair<std::string, url::Origin>& salt_and_origin);

  void GotDefaultVideoInputDeviceID(
      GetVideoInputCapabilitiesCallback client_callback,
      std::string device_id_salt,
      const url::Origin& security_origin,
      const std::string& default_device_id);

  void FinalizeGetVideoInputCapabilities(
      GetVideoInputCapabilitiesCallback client_callback,
      const std::string& device_id_salt,
      const url::Origin& security_origin,
      const std::string& default_device_id,
      const media::VideoCaptureDeviceDescriptors& device_descriptors);

  void GetDefaultAudioInputDeviceID(
      GetAudioInputCapabilitiesCallback client_callback,
      const std::pair<std::string, url::Origin>& salt_and_origin);

  void GotDefaultAudioInputDeviceID(const std::string& default_device_id);

  void GotAudioInputEnumeration(const std::string& default_device_id,
                                const MediaDeviceEnumeration& enumeration);

  void GotAudioInputParameters(
      size_t index,
      const base::Optional<media::AudioParameters>& parameters);

  void FinalizeGetAudioInputCapabilities();

  void GetVideoInputDeviceFormats(
      const std::string& device_id,
      bool try_in_use_first,
      GetVideoInputDeviceFormatsCallback client_callback);
  void EnumerateVideoDevicesForFormats(
      GetVideoInputDeviceFormatsCallback client_callback,
      const std::string& device_id,
      bool try_in_use_first,
      const std::pair<std::string, url::Origin>& salt_and_origin);
  void FinalizeGetVideoInputDeviceFormats(
      GetVideoInputDeviceFormatsCallback client_callback,
      const std::string& device_id,
      bool try_in_use_first,
      const std::string& device_id_salt,
      const url::Origin& security_origin,
      const media::VideoCaptureDeviceDescriptors& device_descriptors);

  // Returns the supported video formats for the given |device_id|.
  // If |try_in_use_first| is true and the device is being used, only the format
  // in use is returned. Otherwise, all formats supported by the device are
  // returned.
  media::VideoCaptureFormats GetVideoInputFormats(const std::string& device_id,
                                                  bool try_in_use_first);

  void NotifyDeviceChangeOnUIThread(const std::vector<uint32_t>& subscriptions,
                                    MediaDeviceType type,
                                    const MediaDeviceInfoArray& device_infos);

  std::string ComputeGroupIDSalt(const std::string& device_id_salt);

  // The following const fields can be accessed on any thread.
  const int render_process_id_;
  const int render_frame_id_;
  // This value is combined with the device ID salt to produce a salt for group
  // IDs that, unlike the device ID salt, is not persistent across browsing
  // sessions, but like the device ID salt, is reset when the user clears
  // browsing data.
  const std::string group_id_salt_base_;

  // The following fields can only be accessed on the IO thread.
  MediaStreamManager* media_stream_manager_;
  std::unique_ptr<MediaDevicesPermissionChecker> permission_checker_;
  std::vector<uint32_t> device_change_subscriptions_[NUM_MEDIA_DEVICE_TYPES];

  // This field can only be accessed on the UI thread.
  ::mojom::MediaDevicesListenerPtr device_change_listener_;

  struct AudioInputCapabilitiesRequest;
  // Queued requests for audio-input capabilities.
  std::vector<AudioInputCapabilitiesRequest>
      pending_audio_input_capabilities_requests_;
  size_t num_pending_audio_input_parameters_;
  std::vector<::mojom::AudioInputDeviceCapabilities>
      current_audio_input_capabilities_;

  // Callback used to obtain the current device ID salt and security origin.
  MediaDeviceSaltAndOriginCallback salt_and_origin_callback_;

  base::WeakPtrFactory<MediaDevicesDispatcherHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaDevicesDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_DEVICES_DISPATCHER_HOST_H_
