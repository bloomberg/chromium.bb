// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_device_enumeration_event_handler.h"

#include "base/logging.h"
#include "ppapi/shared_impl/ppb_device_ref_shared.h"

namespace content {

namespace {

ppapi::DeviceRefData FromStreamDeviceInfo(const StreamDeviceInfo& info) {
  ppapi::DeviceRefData data;
  data.id = info.device.id;
  data.name = info.device.name;
  data.type = PepperDeviceEnumerationEventHandler::FromMediaStreamType(
      info.device.type);
  return data;
}

}  // namespace

PepperDeviceEnumerationEventHandler::PepperDeviceEnumerationEventHandler()
    : next_id_(1) {
}

PepperDeviceEnumerationEventHandler::~PepperDeviceEnumerationEventHandler() {
  DCHECK(enumerate_callbacks_.empty());
  DCHECK(open_callbacks_.empty());
}

int PepperDeviceEnumerationEventHandler::RegisterEnumerateDevicesCallback(
    const webkit::ppapi::PluginDelegate::EnumerateDevicesCallback& callback) {
  enumerate_callbacks_[next_id_] = callback;
  return next_id_++;
}

void PepperDeviceEnumerationEventHandler::UnregisterEnumerateDevicesCallback(
    int request_id) {
  enumerate_callbacks_.erase(request_id);
}

int PepperDeviceEnumerationEventHandler::RegisterOpenDeviceCallback(
    const PepperPluginDelegateImpl::OpenDeviceCallback& callback) {
  open_callbacks_[next_id_] = callback;
  return next_id_++;
}

void PepperDeviceEnumerationEventHandler::OnStreamGenerated(
    int request_id,
    const std::string& label,
    const StreamDeviceInfoArray& audio_device_array,
    const StreamDeviceInfoArray& video_device_array) {
}

void PepperDeviceEnumerationEventHandler::OnStreamGenerationFailed(
    int request_id) {
}

void PepperDeviceEnumerationEventHandler::OnDevicesEnumerated(
    int request_id,
    const StreamDeviceInfoArray& device_array) {
  NotifyDevicesEnumerated(request_id, true, device_array);
}

void PepperDeviceEnumerationEventHandler::OnDevicesEnumerationFailed(
    int request_id) {
  NotifyDevicesEnumerated(request_id, false, StreamDeviceInfoArray());
}

void PepperDeviceEnumerationEventHandler::OnDeviceOpened(
    int request_id,
    const std::string& label,
    const StreamDeviceInfo& device_info) {
  NotifyDeviceOpened(request_id, true, label);
}

void PepperDeviceEnumerationEventHandler::OnDeviceOpenFailed(int request_id) {
  NotifyDeviceOpened(request_id, false, std::string());
}

// static
MediaStreamType PepperDeviceEnumerationEventHandler::FromPepperDeviceType(
    PP_DeviceType_Dev type) {
  switch (type) {
    case PP_DEVICETYPE_DEV_INVALID:
      return MEDIA_NO_SERVICE;
    case PP_DEVICETYPE_DEV_AUDIOCAPTURE:
      return MEDIA_DEVICE_AUDIO_CAPTURE;
    case PP_DEVICETYPE_DEV_VIDEOCAPTURE:
      return MEDIA_DEVICE_VIDEO_CAPTURE;
    default:
      NOTREACHED();
      return MEDIA_NO_SERVICE;
  }
}

// static
PP_DeviceType_Dev PepperDeviceEnumerationEventHandler::FromMediaStreamType(
    MediaStreamType type) {
  switch (type) {
    case MEDIA_NO_SERVICE:
      return PP_DEVICETYPE_DEV_INVALID;
    case MEDIA_DEVICE_AUDIO_CAPTURE:
      return PP_DEVICETYPE_DEV_AUDIOCAPTURE;
    case MEDIA_DEVICE_VIDEO_CAPTURE:
      return PP_DEVICETYPE_DEV_VIDEOCAPTURE;
    default:
      NOTREACHED();
      return PP_DEVICETYPE_DEV_INVALID;
  }
}

void PepperDeviceEnumerationEventHandler::NotifyDevicesEnumerated(
    int request_id,
    bool succeeded,
    const StreamDeviceInfoArray& device_array) {
  EnumerateCallbackMap::iterator iter = enumerate_callbacks_.find(request_id);
  if (iter == enumerate_callbacks_.end()) {
    // This might be enumerated result sent before StopEnumerateDevices is
    // called since EnumerateDevices is persistent request.
    return;
  }

  webkit::ppapi::PluginDelegate::EnumerateDevicesCallback callback =
      iter->second;

  std::vector<ppapi::DeviceRefData> devices;
  if (succeeded) {
    devices.reserve(device_array.size());
    for (StreamDeviceInfoArray::const_iterator info =
             device_array.begin(); info != device_array.end(); ++info) {
      devices.push_back(FromStreamDeviceInfo(*info));
    }
  }
  callback.Run(request_id, succeeded, devices);
}

void PepperDeviceEnumerationEventHandler::NotifyDeviceOpened(
    int request_id,
    bool succeeded,
    const std::string& label) {
  OpenCallbackMap::iterator iter = open_callbacks_.find(request_id);
  if (iter == open_callbacks_.end()) {
    NOTREACHED();
    return;
  }

  PepperPluginDelegateImpl::OpenDeviceCallback callback = iter->second;
  open_callbacks_.erase(iter);

  callback.Run(request_id, succeeded, label);
}

}  // namespace content
