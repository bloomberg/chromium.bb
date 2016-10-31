// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_devices_dispatcher_host.h"

#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/common/media/media_devices.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/common/media_stream_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "url/origin.h"

namespace content {

namespace {

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

}  // namespace

// static
void MediaDevicesDispatcherHost::Create(
    int render_process_id,
    int routing_id,
    const std::string& device_id_salt,
    MediaStreamManager* media_stream_manager,
    ::mojom::MediaDevicesDispatcherHostRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  mojo::MakeStrongBinding(
      base::MakeUnique<MediaDevicesDispatcherHost>(
          render_process_id, routing_id, device_id_salt, media_stream_manager),
      std::move(request));
}

MediaDevicesDispatcherHost::MediaDevicesDispatcherHost(
    int render_process_id,
    int routing_id,
    const std::string& device_id_salt,
    MediaStreamManager* media_stream_manager)
    : render_process_id_(render_process_id),
      routing_id_(routing_id),
      device_id_salt_(device_id_salt),
      group_id_salt_(ResourceContext::CreateRandomMediaDeviceIDSalt()),
      media_stream_manager_(media_stream_manager),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

MediaDevicesDispatcherHost::~MediaDevicesDispatcherHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void MediaDevicesDispatcherHost::EnumerateDevices(
    bool request_audio_input,
    bool request_video_input,
    bool request_audio_output,
    const url::Origin& security_origin,
    const EnumerateDevicesCallback& client_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!request_audio_input && !request_video_input && !request_audio_output) {
    bad_message::ReceivedBadMessage(
        render_process_id_, bad_message::MDDH_INVALID_DEVICE_TYPE_REQUEST);
    return;
  }

  if (!MediaStreamManager::IsOriginAllowed(render_process_id_,
                                           security_origin)) {
    bad_message::ReceivedBadMessage(render_process_id_,
                                    bad_message::MDDH_UNAUTHORIZED_ORIGIN);
    return;
  }

  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_AUDIO_INPUT] = request_audio_input;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_VIDEO_INPUT] = request_video_input;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_AUDIO_OUTPUT] = request_audio_output;

  permission_checker_.CheckPermissions(
      devices_to_enumerate, render_process_id_, routing_id_, security_origin,
      base::Bind(&MediaDevicesDispatcherHost::DoEnumerateDevices,
                 weak_factory_.GetWeakPtr(), devices_to_enumerate,
                 security_origin, client_callback));
}

void MediaDevicesDispatcherHost::SetPermissionChecker(
    const MediaDevicesPermissionChecker& permission_checker) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  permission_checker_ = permission_checker;
}

void MediaDevicesDispatcherHost::DoEnumerateDevices(
    const MediaDevicesManager::BoolDeviceTypes& requested_types,
    const url::Origin& security_origin,
    const EnumerateDevicesCallback& client_callback,
    const MediaDevicesManager::BoolDeviceTypes& has_permissions) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  media_stream_manager_->media_devices_manager()->EnumerateDevices(
      requested_types,
      base::Bind(&MediaDevicesDispatcherHost::DevicesEnumerated,
                 weak_factory_.GetWeakPtr(), requested_types, security_origin,
                 client_callback, has_permissions));
}

void MediaDevicesDispatcherHost::DevicesEnumerated(
    const MediaDevicesManager::BoolDeviceTypes& requested_types,
    const url::Origin& security_origin,
    const EnumerateDevicesCallback& client_callback,
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
  client_callback.Run(result);
}

}  // namespace content
