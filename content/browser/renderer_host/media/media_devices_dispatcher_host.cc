// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_devices_dispatcher_host.h"

#include <stddef.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "content/browser/bad_message.h"
#include "content/browser/media/media_devices_util.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/common/media/media_devices.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/media_stream_request.h"
#include "media/audio/audio_system.h"
#include "media/base/media_switches.h"
#include "media/base/video_facing.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "url/origin.h"

namespace content {

namespace {

// Resolutions used if the source doesn't support capability enumeration.
struct {
  int width;
  int height;
} const kFallbackVideoResolutions[] = {{1920, 1080}, {1280, 720}, {960, 720},
                                       {640, 480},   {640, 360},  {320, 240},
                                       {320, 180}};

// Frame rates for sources with no support for capability enumeration.
const int kFallbackVideoFrameRates[] = {30, 60};

url::Origin GetOrigin(int process_id,
                      int frame_id,
                      const url::Origin& origin_for_testing) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!origin_for_testing.unique())
    return origin_for_testing;

  RenderFrameHost* frame_host = RenderFrameHost::FromID(process_id, frame_id);
  return frame_host ? frame_host->GetLastCommittedOrigin() : url::Origin();
}

MediaDeviceInfo TranslateDeviceInfo(bool has_permission,
                                    const std::string& device_id_salt,
                                    const std::string& group_id_salt,
                                    const url::Origin& security_origin,
                                    const MediaDeviceInfo& device_info) {
  return MediaDeviceInfo(
      GetHMACForMediaDeviceID(device_id_salt, security_origin,
                              device_info.device_id),
      has_permission ? device_info.label : std::string(),
      device_info.group_id.empty()
          ? std::string()
          : GetHMACForMediaDeviceID(group_id_salt, security_origin,
                                    device_info.group_id));
}

MediaDeviceInfoArray TranslateMediaDeviceInfoArray(
    bool has_permission,
    const std::string& device_id_salt,
    const std::string& group_id_salt,
    const url::Origin& security_origin,
    const MediaDeviceInfoArray& input) {
  MediaDeviceInfoArray result;
  for (const auto& device_info : input) {
    result.push_back(TranslateDeviceInfo(has_permission, device_id_salt,
                                         group_id_salt, security_origin,
                                         device_info));
  }
  return result;
}

::mojom::FacingMode ToFacingMode(media::VideoFacingMode facing_mode) {
  switch (facing_mode) {
    case media::MEDIA_VIDEO_FACING_NONE:
      return ::mojom::FacingMode::NONE;
    case media::MEDIA_VIDEO_FACING_USER:
      return ::mojom::FacingMode::USER;
    case media::MEDIA_VIDEO_FACING_ENVIRONMENT:
      return ::mojom::FacingMode::ENVIRONMENT;
    default:
      NOTREACHED();
      return ::mojom::FacingMode::NONE;
  }
}

std::vector<::mojom::AudioInputDeviceCapabilitiesPtr>
ToVectorAudioInputDeviceCapabilitiesPtr(
    const std::vector<::mojom::AudioInputDeviceCapabilities>&
        capabilities_vector,
    const url::Origin& security_origin,
    const std::string& salt) {
  std::vector<::mojom::AudioInputDeviceCapabilitiesPtr> result;
  result.reserve(capabilities_vector.size());
  for (auto& capabilities : capabilities_vector) {
    ::mojom::AudioInputDeviceCapabilitiesPtr capabilities_ptr =
        ::mojom::AudioInputDeviceCapabilities::New();
    capabilities_ptr->device_id =
        GetHMACForMediaDeviceID(salt, security_origin, capabilities.device_id);
    capabilities_ptr->parameters = capabilities.parameters;
    result.push_back(std::move(capabilities_ptr));
  }
  return result;
}

}  // namespace

// static
void MediaDevicesDispatcherHost::Create(
    int render_process_id,
    int render_frame_id,
    const std::string& device_id_salt,
    MediaStreamManager* media_stream_manager,
    ::mojom::MediaDevicesDispatcherHostRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  mojo::MakeStrongBinding(base::MakeUnique<MediaDevicesDispatcherHost>(
                              render_process_id, render_frame_id,
                              device_id_salt, media_stream_manager),
                          std::move(request));
}

MediaDevicesDispatcherHost::MediaDevicesDispatcherHost(
    int render_process_id,
    int render_frame_id,
    const std::string& device_id_salt,
    MediaStreamManager* media_stream_manager)
    : render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      device_id_salt_(device_id_salt),
      group_id_salt_(BrowserContext::CreateRandomMediaDeviceIDSalt()),
      media_stream_manager_(media_stream_manager),
      permission_checker_(base::MakeUnique<MediaDevicesPermissionChecker>()),
      num_pending_audio_input_parameters_(0),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

MediaDevicesDispatcherHost::~MediaDevicesDispatcherHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // It may happen that media_devices_manager() is destroyed before MDDH on some
  // shutdown scenarios.
  if (!media_stream_manager_->media_devices_manager())
    return;

  for (size_t i = 0; i < NUM_MEDIA_DEVICE_TYPES; ++i) {
    if (!device_change_subscriptions_[i].empty()) {
      media_stream_manager_->media_devices_manager()
          ->UnsubscribeDeviceChangeNotifications(
              static_cast<MediaDeviceType>(i), this);
    }
  }
}

void MediaDevicesDispatcherHost::EnumerateDevices(
    bool request_audio_input,
    bool request_video_input,
    bool request_audio_output,
    EnumerateDevicesCallback client_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!request_audio_input && !request_video_input && !request_audio_output) {
    bad_message::ReceivedBadMessage(
        render_process_id_, bad_message::MDDH_INVALID_DEVICE_TYPE_REQUEST);
    return;
  }

  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_AUDIO_INPUT] = request_audio_input;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_VIDEO_INPUT] = request_video_input;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_AUDIO_OUTPUT] = request_audio_output;

  base::PostTaskAndReplyWithResult(
      BrowserThread::GetTaskRunnerForThread(BrowserThread::UI).get(), FROM_HERE,
      base::Bind(GetOrigin, render_process_id_, render_frame_id_,
                 security_origin_for_testing_),
      base::Bind(
          &MediaDevicesDispatcherHost::CheckPermissionsForEnumerateDevices,
          weak_factory_.GetWeakPtr(), devices_to_enumerate,
          base::Passed(&client_callback)));
}

void MediaDevicesDispatcherHost::GetVideoInputCapabilities(
    GetVideoInputCapabilitiesCallback client_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTaskAndReplyWithResult(
      BrowserThread::GetTaskRunnerForThread(BrowserThread::UI).get(), FROM_HERE,
      base::Bind(GetOrigin, render_process_id_, render_frame_id_,
                 security_origin_for_testing_),
      base::Bind(&MediaDevicesDispatcherHost::GetDefaultVideoInputDeviceID,
                 weak_factory_.GetWeakPtr(), base::Passed(&client_callback)));
}

void MediaDevicesDispatcherHost::GetAudioInputCapabilities(
    GetAudioInputCapabilitiesCallback client_callback) {
  base::PostTaskAndReplyWithResult(
      BrowserThread::GetTaskRunnerForThread(BrowserThread::UI).get(), FROM_HERE,
      base::Bind(GetOrigin, render_process_id_, render_frame_id_,
                 security_origin_for_testing_),
      base::Bind(&MediaDevicesDispatcherHost::GetDefaultAudioInputDeviceID,
                 weak_factory_.GetWeakPtr(), base::Passed(&client_callback)));
}

void MediaDevicesDispatcherHost::SubscribeDeviceChangeNotifications(
    MediaDeviceType type,
    uint32_t subscription_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(IsValidMediaDeviceType(type));
  auto it =
      std::find(device_change_subscriptions_[type].begin(),
                device_change_subscriptions_[type].end(), subscription_id);
  if (it != device_change_subscriptions_[type].end()) {
    bad_message::ReceivedBadMessage(
        render_process_id_, bad_message::MDDH_INVALID_SUBSCRIPTION_REQUEST);
    return;
  }

  if (device_change_subscriptions_[type].empty()) {
    media_stream_manager_->media_devices_manager()
        ->SubscribeDeviceChangeNotifications(type, this);
  }

  device_change_subscriptions_[type].push_back(subscription_id);
}

void MediaDevicesDispatcherHost::UnsubscribeDeviceChangeNotifications(
    MediaDeviceType type,
    uint32_t subscription_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(IsValidMediaDeviceType(type));
  auto it =
      std::find(device_change_subscriptions_[type].begin(),
                device_change_subscriptions_[type].end(), subscription_id);
  // Ignore invalid unsubscription requests.
  if (it == device_change_subscriptions_[type].end())
    return;

  device_change_subscriptions_[type].erase(it);
  if (device_change_subscriptions_[type].empty()) {
    media_stream_manager_->media_devices_manager()
        ->UnsubscribeDeviceChangeNotifications(type, this);
  }
}

void MediaDevicesDispatcherHost::OnDevicesChanged(
    MediaDeviceType type,
    const MediaDeviceInfoArray& device_infos) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(IsValidMediaDeviceType(type));
  std::vector<uint32_t> subscriptions = device_change_subscriptions_[type];
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&MediaDevicesDispatcherHost::NotifyDeviceChangeOnUIThread,
                     weak_factory_.GetWeakPtr(), std::move(subscriptions), type,
                     device_infos));
}

void MediaDevicesDispatcherHost::NotifyDeviceChangeOnUIThread(
    const std::vector<uint32_t>& subscriptions,
    MediaDeviceType type,
    const MediaDeviceInfoArray& device_infos) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsValidMediaDeviceType(type));

  ::mojom::MediaDevicesListenerPtr media_devices_listener;
  url::Origin security_origin;
  if (device_change_listener_) {
    media_devices_listener = std::move(device_change_listener_);
  } else {
    RenderFrameHost* render_frame_host =
        RenderFrameHost::FromID(render_process_id_, render_frame_id_);
    if (!render_frame_host)
      return;

    render_frame_host->GetRemoteInterfaces()->GetInterface(
        mojo::MakeRequest(&media_devices_listener));
    if (!media_devices_listener)
      return;

    security_origin = render_frame_host->GetLastCommittedOrigin();
  }

  for (uint32_t subscription_id : subscriptions) {
    bool has_permission = permission_checker_->CheckPermissionOnUIThread(
        type, render_process_id_, render_frame_id_);
    media_devices_listener->OnDevicesChanged(
        type, subscription_id,
        TranslateMediaDeviceInfoArray(has_permission, device_id_salt_,
                                      group_id_salt_, security_origin,
                                      device_infos));
  }
}

void MediaDevicesDispatcherHost::SetPermissionChecker(
    std::unique_ptr<MediaDevicesPermissionChecker> permission_checker) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(permission_checker);
  permission_checker_ = std::move(permission_checker);
}

void MediaDevicesDispatcherHost::SetDeviceChangeListenerForTesting(
    ::mojom::MediaDevicesListenerPtr listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  device_change_listener_ = std::move(listener);
}

void MediaDevicesDispatcherHost::SetSecurityOriginForTesting(
    const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  security_origin_for_testing_ = origin;
}

void MediaDevicesDispatcherHost::CheckPermissionsForEnumerateDevices(
    const MediaDevicesManager::BoolDeviceTypes& requested_types,
    EnumerateDevicesCallback client_callback,
    const url::Origin& security_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  permission_checker_->CheckPermissions(
      requested_types, render_process_id_, render_frame_id_,
      base::BindOnce(&MediaDevicesDispatcherHost::DoEnumerateDevices,
                     weak_factory_.GetWeakPtr(), requested_types,
                     base::Passed(&client_callback), security_origin));
}

void MediaDevicesDispatcherHost::DoEnumerateDevices(
    const MediaDevicesManager::BoolDeviceTypes& requested_types,
    EnumerateDevicesCallback client_callback,
    const url::Origin& security_origin,
    const MediaDevicesManager::BoolDeviceTypes& has_permissions) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  media_stream_manager_->media_devices_manager()->EnumerateDevices(
      requested_types,
      base::Bind(&MediaDevicesDispatcherHost::DevicesEnumerated,
                 weak_factory_.GetWeakPtr(), requested_types,
                 base::Passed(&client_callback), security_origin,
                 has_permissions));
}

void MediaDevicesDispatcherHost::DevicesEnumerated(
    const MediaDevicesManager::BoolDeviceTypes& requested_types,
    EnumerateDevicesCallback client_callback,
    const url::Origin& security_origin,
    const MediaDevicesManager::BoolDeviceTypes& has_permissions,
    const MediaDeviceEnumeration& enumeration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::vector<std::vector<MediaDeviceInfo>> result(NUM_MEDIA_DEVICE_TYPES);
  for (size_t i = 0; i < NUM_MEDIA_DEVICE_TYPES; ++i) {
    if (!requested_types[i])
      continue;

    for (const auto& device_info : enumeration[i]) {
      result[i].push_back(TranslateDeviceInfo(has_permissions[i],
                                              device_id_salt_, group_id_salt_,
                                              security_origin, device_info));
    }
  }
  std::move(client_callback).Run(result);
}

void MediaDevicesDispatcherHost::GetDefaultVideoInputDeviceID(
    GetVideoInputCapabilitiesCallback client_callback,
    const url::Origin& security_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  GetDefaultMediaDeviceID(
      MEDIA_DEVICE_TYPE_VIDEO_INPUT, render_process_id_, render_frame_id_,
      base::Bind(&MediaDevicesDispatcherHost::GotDefaultVideoInputDeviceID,
                 weak_factory_.GetWeakPtr(), base::Passed(&client_callback),
                 security_origin));
}

void MediaDevicesDispatcherHost::GotDefaultVideoInputDeviceID(
    GetVideoInputCapabilitiesCallback client_callback,
    const url::Origin& security_origin,
    const std::string& default_device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  media_stream_manager_->video_capture_manager()->EnumerateDevices(
      base::Bind(&MediaDevicesDispatcherHost::FinalizeGetVideoInputCapabilities,
                 weak_factory_.GetWeakPtr(), base::Passed(&client_callback),
                 security_origin, default_device_id));
}

void MediaDevicesDispatcherHost::FinalizeGetVideoInputCapabilities(
    GetVideoInputCapabilitiesCallback client_callback,
    const url::Origin& security_origin,
    const std::string& default_device_id,
    const media::VideoCaptureDeviceDescriptors& device_descriptors) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::vector<::mojom::VideoInputDeviceCapabilitiesPtr>
      video_input_capabilities;
  for (const auto& descriptor : device_descriptors) {
    std::string hmac_device_id = GetHMACForMediaDeviceID(
        device_id_salt_, security_origin, descriptor.device_id);
    ::mojom::VideoInputDeviceCapabilitiesPtr capabilities =
        ::mojom::VideoInputDeviceCapabilities::New();
    capabilities->device_id = std::move(hmac_device_id);
    capabilities->formats = GetVideoInputFormats(descriptor.device_id);
    if (capabilities->formats.empty()) {
      // The device does not seem to support capability enumeration. Compose
      // a fallback list of capabilities.
      for (const auto& resolution : kFallbackVideoResolutions) {
        for (const auto frame_rate : kFallbackVideoFrameRates) {
          capabilities->formats.push_back(media::VideoCaptureFormat(
              gfx::Size(resolution.width, resolution.height), frame_rate,
              media::PIXEL_FORMAT_I420));
        }
      }
    }
    capabilities->facing_mode = ToFacingMode(descriptor.facing);
#if defined(OS_ANDROID)
    // On Android, the facing mode is not available in the |facing| field,
    // but is available as part of the label.
    // TODO(guidou): Remove this code once the |facing| field is supported
    // on Android. See http://crbug.com/672856.
    if (descriptor.GetNameAndModel().find("front") != std::string::npos)
      capabilities->facing_mode = ::mojom::FacingMode::USER;
    else if (descriptor.GetNameAndModel().find("back") != std::string::npos)
      capabilities->facing_mode = ::mojom::FacingMode::ENVIRONMENT;
#endif
    if (descriptor.device_id == default_device_id) {
      video_input_capabilities.insert(video_input_capabilities.begin(),
                                      std::move(capabilities));
    } else {
      video_input_capabilities.push_back(std::move(capabilities));
    }
  }

  std::move(client_callback).Run(std::move(video_input_capabilities));
}

media::VideoCaptureFormats MediaDevicesDispatcherHost::GetVideoInputFormats(
    const std::string& device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  media::VideoCaptureFormats formats;
  base::Optional<media::VideoCaptureFormat> format =
      media_stream_manager_->video_capture_manager()->GetDeviceFormatInUse(
          MEDIA_DEVICE_VIDEO_CAPTURE, device_id);
  if (format.has_value()) {
    formats.push_back(format.value());
    return formats;
  }

  media_stream_manager_->video_capture_manager()->GetDeviceSupportedFormats(
      device_id, &formats);
  // Remove formats that have zero resolution.
  formats.erase(std::remove_if(formats.begin(), formats.end(),
                               [](const media::VideoCaptureFormat& format) {
                                 return format.frame_size.GetArea() <= 0;
                               }),
                formats.end());

  return formats;
}

struct MediaDevicesDispatcherHost::AudioInputCapabilitiesRequest {
  url::Origin security_origin;
  GetAudioInputCapabilitiesCallback client_callback;
};

void MediaDevicesDispatcherHost::GetDefaultAudioInputDeviceID(
    GetAudioInputCapabilitiesCallback client_callback,
    const url::Origin& security_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  pending_audio_input_capabilities_requests_.push_back(
      AudioInputCapabilitiesRequest{security_origin,
                                    std::move(client_callback)});
  if (pending_audio_input_capabilities_requests_.size() > 1U)
    return;

  DCHECK(current_audio_input_capabilities_.empty());
  GetDefaultMediaDeviceID(
      MEDIA_DEVICE_TYPE_AUDIO_INPUT, render_process_id_, render_frame_id_,
      base::Bind(&MediaDevicesDispatcherHost::GotDefaultAudioInputDeviceID,
                 weak_factory_.GetWeakPtr()));
}

void MediaDevicesDispatcherHost::GotDefaultAudioInputDeviceID(
    const std::string& default_device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_GT(pending_audio_input_capabilities_requests_.size(), 0U);
  DCHECK(current_audio_input_capabilities_.empty());
  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_AUDIO_INPUT] = true;
  media_stream_manager_->media_devices_manager()->EnumerateDevices(
      devices_to_enumerate,
      base::Bind(&MediaDevicesDispatcherHost::GotAudioInputEnumeration,
                 weak_factory_.GetWeakPtr(), default_device_id));
}

void MediaDevicesDispatcherHost::GotAudioInputEnumeration(
    const std::string& default_device_id,
    const MediaDeviceEnumeration& enumeration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_GT(pending_audio_input_capabilities_requests_.size(), 0U);
  DCHECK(current_audio_input_capabilities_.empty());
  DCHECK_EQ(num_pending_audio_input_parameters_, 0U);
  for (const auto& device_info : enumeration[MEDIA_DEVICE_TYPE_AUDIO_INPUT]) {
    ::mojom::AudioInputDeviceCapabilities capabilities(
        device_info.device_id,
        media::AudioParameters::UnavailableDeviceParams());
    if (device_info.device_id == default_device_id)
      current_audio_input_capabilities_.insert(
          current_audio_input_capabilities_.begin(), std::move(capabilities));
    else
      current_audio_input_capabilities_.push_back(std::move(capabilities));
  }
  // No devices or fake devices, no need to read audio parameters.
  if (current_audio_input_capabilities_.empty() ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeDeviceForMediaStream)) {
    FinalizeGetAudioInputCapabilities();
    return;
  }

  num_pending_audio_input_parameters_ =
      current_audio_input_capabilities_.size();
  for (size_t i = 0; i < num_pending_audio_input_parameters_; ++i) {
    media_stream_manager_->audio_system()->GetInputStreamParameters(
        current_audio_input_capabilities_[i].device_id,
        base::BindOnce(&MediaDevicesDispatcherHost::GotAudioInputParameters,
                       weak_factory_.GetWeakPtr(), i));
  }
}

void MediaDevicesDispatcherHost::GotAudioInputParameters(
    size_t index,
    const media::AudioParameters& parameters) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_GT(pending_audio_input_capabilities_requests_.size(), 0U);
  DCHECK_GT(current_audio_input_capabilities_.size(), index);
  DCHECK_GT(num_pending_audio_input_parameters_, 0U);

  if (parameters.IsValid())
    current_audio_input_capabilities_[index].parameters = parameters;

  DCHECK(current_audio_input_capabilities_[index].parameters.IsValid());
  if (--num_pending_audio_input_parameters_ == 0U)
    FinalizeGetAudioInputCapabilities();
}

void MediaDevicesDispatcherHost::FinalizeGetAudioInputCapabilities() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_GT(pending_audio_input_capabilities_requests_.size(), 0U);
  DCHECK_EQ(num_pending_audio_input_parameters_, 0U);

  for (auto& request : pending_audio_input_capabilities_requests_) {
    std::move(request.client_callback)
        .Run(ToVectorAudioInputDeviceCapabilitiesPtr(
            current_audio_input_capabilities_, request.security_origin,
            device_id_salt_));
  }

  current_audio_input_capabilities_.clear();
  pending_audio_input_capabilities_requests_.clear();
}

}  // namespace content
