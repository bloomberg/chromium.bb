// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_output_authorization_handler.h"

#include "base/memory/ptr_util.h"
#include "base/task_runner_util.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_device_id.h"
#include "media/audio/audio_system.h"
#include "media/base/limits.h"

namespace {

media::AudioParameters TryToFixAudioParameters(
    const media::AudioParameters& params) {
  DCHECK(!params.IsValid());
  media::AudioParameters params_copy(params);

  // If the number of output channels is greater than the maximum, use the
  // maximum allowed value. Hardware channels are ignored upstream, so it is
  // better to report a valid value if this is the only problem.
  if (params.channels() > media::limits::kMaxChannels) {
    DCHECK(params.channel_layout() == media::CHANNEL_LAYOUT_DISCRETE);
    params_copy.set_channels_for_discrete(media::limits::kMaxChannels);
  }

  // If hardware parameters are still invalid, use dummy parameters with
  // fake audio path and let the client handle the error.
  return params_copy.IsValid()
             ? params_copy
             : media::AudioParameters::UnavailableDeviceParams();
}

}  // namespace

namespace content {

AudioOutputAuthorizationHandler::AudioOutputAuthorizationHandler(
    media::AudioSystem* audio_system,
    MediaStreamManager* media_stream_manager,
    int render_process_id,
    const std::string& salt)
    : audio_system_(audio_system),
      media_stream_manager_(media_stream_manager),
      permission_checker_(base::MakeUnique<MediaDevicesPermissionChecker>()),
      render_process_id_(render_process_id),
      salt_(salt),
      weak_factory_(this) {
  DCHECK(media_stream_manager_);
}

void AudioOutputAuthorizationHandler::OverridePermissionsForTesting(
    bool override_value) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  permission_checker_.reset(new MediaDevicesPermissionChecker(override_value));
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
    const url::Origin& security_origin,
    AuthorizationCompletedCallback cb) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!IsValidDeviceId(device_id)) {
    std::move(cb).Run(media::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND, false,
                      media::AudioParameters::UnavailableDeviceParams(),
                      std::string());
    return;
  }

  // If |session_id| should be used for output device selection and such an
  // output device is found, reuse the input device permissions.
  if (media::AudioDeviceDescription::UseSessionIdToSelectDevice(session_id,
                                                                device_id)) {
    const StreamDeviceInfo* info =
        media_stream_manager_->audio_input_device_manager()
            ->GetOpenedDeviceInfoById(session_id);
    if (info && !info->device.matched_output_device_id.empty()) {
      media::AudioParameters output_params(
          media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
          static_cast<media::ChannelLayout>(
              info->device.matched_output.channel_layout),
          info->device.matched_output.sample_rate, 16,
          info->device.matched_output.frames_per_buffer);
      output_params.set_effects(info->device.matched_output.effects);

      // We already have the unhashed id and the parameters, so jump to
      // DeviceParametersReceived.
      DeviceParametersReceived(std::move(cb), true,
                               info->device.matched_output_device_id,
                               output_params);
      return;
    }
    // Otherwise, the default device is used.
  }

  if (media::AudioDeviceDescription::IsDefaultDevice(device_id)) {
    // Default device doesn't need authorization.
    AccessChecked(std::move(cb), device_id, security_origin, true);
    return;
  }

  // Check security origin if nondefault device is requested.
  if (!MediaStreamManager::IsOriginAllowed(render_process_id_,
                                           security_origin)) {
    std::move(cb).Run(media::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED, false,
                      media::AudioParameters::UnavailableDeviceParams(),
                      std::string());
    bad_message::ReceivedBadMessage(render_process_id_,
                                    bad_message::AOAH_UNAUTHORIZED_URL);
    return;
  }

  // Check that MediaStream device permissions have been granted for
  // nondefault devices.
  permission_checker_->CheckPermission(
      MEDIA_DEVICE_TYPE_AUDIO_OUTPUT, render_process_id_, render_frame_id,
      base::BindOnce(&AudioOutputAuthorizationHandler::AccessChecked,
                     weak_factory_.GetWeakPtr(), std::move(cb), device_id,
                     security_origin));
}

void AudioOutputAuthorizationHandler::AccessChecked(
    AuthorizationCompletedCallback cb,
    const std::string& device_id,
    const url::Origin& security_origin,
    bool has_access) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!has_access) {
    std::move(cb).Run(media::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED, false,
                      media::AudioParameters::UnavailableDeviceParams(),
                      std::string());
    return;
  }

  // For default device, read output parameters directly. Nondefault
  // devices require translation first.
  if (media::AudioDeviceDescription::IsDefaultDevice(device_id)) {
    GetDeviceParameters(std::move(cb),
                        media::AudioDeviceDescription::kDefaultDeviceId);
    return;
  }
  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_AUDIO_OUTPUT] = true;
  media_stream_manager_->media_devices_manager()->EnumerateDevices(
      devices_to_enumerate,
      base::Bind(&AudioOutputAuthorizationHandler::TranslateDeviceID,
                 weak_factory_.GetWeakPtr(), base::Passed(&cb), device_id,
                 security_origin));
}

void AudioOutputAuthorizationHandler::TranslateDeviceID(
    AuthorizationCompletedCallback cb,
    const std::string& device_id,
    const url::Origin& security_origin,
    const MediaDeviceEnumeration& enumeration) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!media::AudioDeviceDescription::IsDefaultDevice(device_id));
  for (const MediaDeviceInfo& device_info :
       enumeration[MEDIA_DEVICE_TYPE_AUDIO_OUTPUT]) {
    if (content::DoesMediaDeviceIDMatchHMAC(salt_, security_origin, device_id,
                                            device_info.device_id)) {
      GetDeviceParameters(std::move(cb), device_info.device_id);
      return;
    }
  }
  std::move(cb).Run(media::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND, false,
                    media::AudioParameters::UnavailableDeviceParams(),
                    std::string());
}

void AudioOutputAuthorizationHandler::GetDeviceParameters(
    AuthorizationCompletedCallback cb,
    const std::string& raw_device_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!raw_device_id.empty());
  audio_system_->GetOutputStreamParameters(
      raw_device_id,
      base::BindOnce(&AudioOutputAuthorizationHandler::DeviceParametersReceived,
                     weak_factory_.GetWeakPtr(), std::move(cb), false,
                     raw_device_id));
}

void AudioOutputAuthorizationHandler::DeviceParametersReceived(
    AuthorizationCompletedCallback cb,
    bool should_send_id,
    const std::string& raw_device_id,
    const media::AudioParameters& output_params) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!raw_device_id.empty());

  std::move(cb).Run(media::OUTPUT_DEVICE_STATUS_OK, should_send_id,
                    output_params.IsValid()
                        ? output_params
                        : TryToFixAudioParameters(output_params),
                    raw_device_id);
}

}  // namespace content
