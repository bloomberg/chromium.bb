// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_OUTPUT_AUTHORIZATION_HANDLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_OUTPUT_AUTHORIZATION_HANDLER_H_

#include <memory>
#include <string>
#include <utility>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/media/media_devices_permission_checker.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_manager.h"
#include "media/base/audio_parameters.h"
#include "media/base/output_device_info.h"

namespace content {

// This class, which lives on the IO thread, handles the logic of an IPC device
// request from the renderer. It checks which device to use (in case of using
// |session_id| to select device), verifies that the renderer is authorized to
// use the device, and gets the default device parameters for the selected audio
// device.
class CONTENT_EXPORT AudioOutputAuthorizationHandler {
 public:
  // The result of an authorization check. In addition to the status, it
  // indicates whether a device was found using the |session_id| in the variable
  // |should_send_id|, in which case the renderer expects to get the id hash. It
  // also has the default audio parameters for the device, and the id for the
  // device, which is needed to open a stream for the device. This id is not
  // hashed, so it must be hashed before sending it to the renderer.
  // TODO(maxmorin): Change to OnceCallback once base:: code is ready for it.
  using AuthorizationCompletedCallback =
      base::Callback<void(media::OutputDeviceStatus status,
                          bool should_send_id,
                          const media::AudioParameters& params,
                          const std::string& raw_device_id)>;

  AudioOutputAuthorizationHandler(media::AudioManager* audio_manager,
                                  MediaStreamManager* media_stream_manager,
                                  int render_process_id_,
                                  const std::string& salt);

  ~AudioOutputAuthorizationHandler();

  // Checks authorization of the device with the hashed id |device_id| for the
  // given render frame id and security origin, or uses |session_id| for
  // authorization. Looks up device id (if |session_id| is used for device
  // selection) and default device parameters.
  void RequestDeviceAuthorization(int render_frame_id,
                                  int session_id,
                                  const std::string& device_id,
                                  const url::Origin& security_origin,
                                  AuthorizationCompletedCallback cb) const;

  // Calling this method will make the checks for permission from the user
  // always return |override_value|.
  void OverridePermissionsForTesting(bool override_value);

 private:
  // Convention: Something named |device_id| is hashed and something named
  // |raw_device_id| is not hashed.

  void AccessChecked(AuthorizationCompletedCallback cb,
                     const std::string& device_id,
                     const url::Origin& security_origin,
                     bool has_access) const;

  void TranslateDeviceID(AuthorizationCompletedCallback cb,
                         const std::string& device_id,
                         const url::Origin& security_origin,
                         const MediaDeviceEnumeration& enumeration) const;

  void GetDeviceParameters(AuthorizationCompletedCallback cb,
                           const std::string& raw_device_id) const;

  void DeviceParametersReceived(
      AuthorizationCompletedCallback cb,
      bool should_send_id,
      const std::string& raw_device_id,
      const media::AudioParameters& output_params) const;

  media::AudioManager* audio_manager_;
  MediaStreamManager* const media_stream_manager_;
  std::unique_ptr<MediaDevicesPermissionChecker> permission_checker_;
  const int render_process_id_;
  const std::string salt_;
  // All access is on the IO thread, and taking a weak pointer to const looks
  // const, so this can be mutable.
  mutable base::WeakPtrFactory<const AudioOutputAuthorizationHandler>
      weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputAuthorizationHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_OUTPUT_AUTHORIZATION_HANDLER_H_
