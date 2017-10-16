// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_output_authorization_handler.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task_runner_util.h"
#include "content/browser/media/media_devices_util.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/render_frame_host.h"
#include "media/audio/audio_system.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"

namespace content {

namespace {

// Returns (by callback) the Media Device salt and the Origin for the frame and
// whether it may request nondefault audio devices.
void CheckAccessOnUIThread(
    int render_process_id,
    int render_frame_id,
    bool override_permissions,
    bool permissions_override_value,
    base::OnceCallback<void(std::string, const url::Origin&, bool)> cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const auto& salt_and_origin =
      GetMediaDeviceSaltAndOrigin(render_process_id, render_frame_id);
  std::string salt = salt_and_origin.first;
  const url::Origin& origin = salt_and_origin.second;

  if (!MediaStreamManager::IsOriginAllowed(render_process_id, origin)) {
    // In this case, it's likely a navigation has occurred while processing this
    // request.
    std::move(cb).Run(std::string(), url::Origin(), false);
    return;
  }

  // Check that MediaStream device permissions have been granted for
  // nondefault devices.
  if (override_permissions) {
    std::move(cb).Run(std::move(salt), origin, permissions_override_value);
    return;
  }

  std::move(cb).Run(
      std::move(salt), origin,
      MediaDevicesPermissionChecker().CheckPermissionOnUIThread(
          MEDIA_DEVICE_TYPE_AUDIO_OUTPUT, render_process_id, render_frame_id));
}

}  // namespace

AudioOutputAuthorizationHandler::AudioOutputAuthorizationHandler(
    media::AudioSystem* audio_system,
    MediaStreamManager* media_stream_manager,
    int render_process_id)
    : audio_system_(audio_system),
      media_stream_manager_(media_stream_manager),
      render_process_id_(render_process_id),
      weak_factory_(this) {
  DCHECK(media_stream_manager_);
}

AudioOutputAuthorizationHandler::~AudioOutputAuthorizationHandler() {
  // |weak_factory| is not thread safe. Make sure it's destructed on the
  // right thread.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void AudioOutputAuthorizationHandler::RequestDeviceAuthorization(
    int render_frame_id,
    int session_id,
    const std::string& device_id,
    AuthorizationCompletedCallback cb) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!IsValidDeviceId(device_id)) {
    std::move(cb).Run(media::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND,
                      media::AudioParameters::UnavailableDeviceParams(),
                      std::string(), std::string());
    return;
  }

  // If |session_id| should be used for output device selection and such an
  // output device is found, reuse the input device permissions.
  if (media::AudioDeviceDescription::UseSessionIdToSelectDevice(session_id,
                                                                device_id)) {
    const MediaStreamDevice* device =
        media_stream_manager_->audio_input_device_manager()
            ->GetOpenedDeviceById(session_id);
    if (device && !device->matched_output_device_id.empty()) {
      media::AudioParameters params(
          media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
          device->matched_output.channel_layout(),
          device->matched_output.sample_rate(), 16,
          device->matched_output.frames_per_buffer());
      params.set_effects(device->matched_output.effects());

      // We don't need the origin for authorization in this case, but it's used
      // for hashing the device id before sending it back to the renderer.
      BrowserThread::PostTaskAndReplyWithResult(
          BrowserThread::UI, FROM_HERE,
          base::BindOnce(&GetMediaDeviceSaltAndOrigin, render_process_id_,
                         render_frame_id),
          base::BindOnce(&AudioOutputAuthorizationHandler::HashDeviceId,
                         weak_factory_.GetWeakPtr(), std::move(cb),
                         device->matched_output_device_id, params));
      return;
    }
    // Otherwise, the default device is used.
  }

  if (media::AudioDeviceDescription::IsDefaultDevice(device_id)) {
    // The default device doesn't need authorization.
    GetDeviceParameters(std::move(cb),
                        media::AudioDeviceDescription::kDefaultDeviceId);
    return;
  }

  // Check device permissions if nondefault device is requested.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(
          &CheckAccessOnUIThread, render_process_id_, render_frame_id,
          override_permissions_, permissions_override_value_,
          media::BindToCurrentLoop(base::BindOnce(
              &AudioOutputAuthorizationHandler::AccessChecked,
              weak_factory_.GetWeakPtr(), std::move(cb), device_id))));
}

void AudioOutputAuthorizationHandler::OverridePermissionsForTesting(
    bool override_value) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  override_permissions_ = true;
  permissions_override_value_ = override_value;
}

void AudioOutputAuthorizationHandler::HashDeviceId(
    AuthorizationCompletedCallback cb,
    const std::string& raw_device_id,
    const media::AudioParameters& params,
    const std::pair<std::string, url::Origin>& salt_and_origin) const {
  std::string hashed_device_id = GetHMACForMediaDeviceID(
      salt_and_origin.first, salt_and_origin.second, raw_device_id);
  DeviceParametersReceived(std::move(cb), hashed_device_id, raw_device_id,
                           params);
}

void AudioOutputAuthorizationHandler::AccessChecked(
    AuthorizationCompletedCallback cb,
    const std::string& device_id,
    std::string salt,
    const url::Origin& security_origin,
    bool has_access) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!has_access) {
    std::move(cb).Run(media::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED,
                      media::AudioParameters::UnavailableDeviceParams(),
                      std::string(), std::string());
    return;
  }

  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_AUDIO_OUTPUT] = true;
  media_stream_manager_->media_devices_manager()->EnumerateDevices(
      devices_to_enumerate,
      base::Bind(&AudioOutputAuthorizationHandler::TranslateDeviceID,
                 weak_factory_.GetWeakPtr(), base::Passed(&cb), device_id,
                 std::move(salt), security_origin));
}

void AudioOutputAuthorizationHandler::TranslateDeviceID(
    AuthorizationCompletedCallback cb,
    const std::string& device_id,
    const std::string& salt,
    const url::Origin& security_origin,
    const MediaDeviceEnumeration& enumeration) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!media::AudioDeviceDescription::IsDefaultDevice(device_id));
  for (const MediaDeviceInfo& device_info :
       enumeration[MEDIA_DEVICE_TYPE_AUDIO_OUTPUT]) {
    if (DoesMediaDeviceIDMatchHMAC(salt, security_origin, device_id,
                                   device_info.device_id)) {
      GetDeviceParameters(std::move(cb), device_info.device_id);
      return;
    }
  }
  std::move(cb).Run(media::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND,
                    media::AudioParameters::UnavailableDeviceParams(),
                    std::string(), std::string());
}

void AudioOutputAuthorizationHandler::GetDeviceParameters(
    AuthorizationCompletedCallback cb,
    const std::string& raw_device_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!raw_device_id.empty());
  audio_system_->GetOutputStreamParameters(
      raw_device_id,
      base::BindOnce(&AudioOutputAuthorizationHandler::DeviceParametersReceived,
                     weak_factory_.GetWeakPtr(), std::move(cb), std::string(),
                     raw_device_id));
}

void AudioOutputAuthorizationHandler::DeviceParametersReceived(
    AuthorizationCompletedCallback cb,
    const std::string& id_for_renderer,
    const std::string& raw_device_id,
    const base::Optional<media::AudioParameters>& params) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!raw_device_id.empty());
  DCHECK(!params || params->IsValid());
  std::move(cb).Run(
      media::OUTPUT_DEVICE_STATUS_OK,
      params.value_or(media::AudioParameters::UnavailableDeviceParams()),
      raw_device_id, id_for_renderer);
}

}  // namespace content
