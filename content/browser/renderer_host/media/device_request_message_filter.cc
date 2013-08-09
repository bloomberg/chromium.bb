// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "content/browser/renderer_host/media/device_request_message_filter.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/common/media/media_stream_messages.h"
#include "content/public/browser/resource_context.h"
#include "crypto/hmac.h"

// Clears the MediaStreamDevice.name from all devices in |device_list|.
static void ClearDeviceLabels(content::StreamDeviceInfoArray* devices) {
  for (content::StreamDeviceInfoArray::iterator device_itr = devices->begin();
       device_itr != devices->end();
       ++device_itr) {
    device_itr->device.name.clear();
  }
}

namespace content {

DeviceRequestMessageFilter::DeviceRequestMessageFilter(
    ResourceContext* resource_context,
    MediaStreamManager* media_stream_manager)
    : resource_context_(resource_context),
      media_stream_manager_(media_stream_manager) {
  DCHECK(resource_context);
  DCHECK(media_stream_manager);
}

DeviceRequestMessageFilter::~DeviceRequestMessageFilter() {
  DCHECK(requests_.empty());
}

struct DeviceRequestMessageFilter::DeviceRequest {
  DeviceRequest(int request_id,
                const GURL& origin,
                const std::string& audio_devices_label,
                const std::string& video_devices_label)
      : request_id(request_id),
        origin(origin),
        has_audio_returned(false),
        has_video_returned(false),
        audio_devices_label(audio_devices_label),
        video_devices_label(video_devices_label) {}

  int request_id;
  GURL origin;
  bool has_audio_returned;
  bool has_video_returned;
  std::string audio_devices_label;
  std::string video_devices_label;
  StreamDeviceInfoArray audio_devices;
  StreamDeviceInfoArray video_devices;
};

void DeviceRequestMessageFilter::StreamGenerated(
    const std::string& label,
    const StreamDeviceInfoArray& audio_devices,
    const StreamDeviceInfoArray& video_devices) {
  NOTIMPLEMENTED();
}

void DeviceRequestMessageFilter::StreamGenerationFailed(
    const std::string& label) {
  NOTIMPLEMENTED();
}

void DeviceRequestMessageFilter::StopGeneratedStream(
    const std::string& label) {
  NOTIMPLEMENTED();
}

void DeviceRequestMessageFilter::DevicesEnumerated(
    const std::string& label,
    const StreamDeviceInfoArray& new_devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Look up the DeviceRequest by id.
  DeviceRequestList::iterator request_it = requests_.begin();
  for (; request_it != requests_.end(); ++request_it) {
    if (label == request_it->audio_devices_label ||
        label == request_it->video_devices_label) {
      break;
    }
  }
  DCHECK(request_it != requests_.end());

  StreamDeviceInfoArray* audio_devices = &request_it->audio_devices;
  StreamDeviceInfoArray* video_devices = &request_it->video_devices;

  // Store hmac'd device ids instead of raw device ids.
  if (label == request_it->audio_devices_label) {
    request_it->has_audio_returned = true;
    DCHECK(audio_devices->empty());
    HmacDeviceIds(request_it->origin, new_devices, audio_devices);
  } else {
    DCHECK(label == request_it->video_devices_label);
    request_it->has_video_returned = true;
    DCHECK(video_devices->empty());
    HmacDeviceIds(request_it->origin, new_devices, video_devices);
  }

  if (!request_it->has_audio_returned || !request_it->has_video_returned) {
    // Wait for the rest of the devices to complete.
    return;
  }

  // Query for mic and camera permissions.
  if (!resource_context_->AllowMicAccess(request_it->origin))
    ClearDeviceLabels(audio_devices);
  if (!resource_context_->AllowCameraAccess(request_it->origin))
    ClearDeviceLabels(video_devices);

  // Both audio and video devices are ready for copying.
  StreamDeviceInfoArray all_devices = *audio_devices;
  all_devices.insert(
      all_devices.end(), video_devices->begin(), video_devices->end());

  Send(new MediaStreamMsg_GetSourcesACK(request_it->request_id, all_devices));

  // TODO(vrk): Rename StopGeneratedStream() to CancelDeviceRequest().
  media_stream_manager_->StopGeneratedStream(request_it->audio_devices_label);
  media_stream_manager_->StopGeneratedStream(request_it->video_devices_label);
  requests_.erase(request_it);
}

void DeviceRequestMessageFilter::DeviceOpened(
    const std::string& label,
    const StreamDeviceInfo& video_device) {
  NOTIMPLEMENTED();
}

bool DeviceRequestMessageFilter::OnMessageReceived(const IPC::Message& message,
                                                   bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(DeviceRequestMessageFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_GetSources, OnGetSources)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void DeviceRequestMessageFilter::OnChannelClosing() {
  BrowserMessageFilter::OnChannelClosing();

  // Since the IPC channel is gone, cancel outstanding device requests.
  for (DeviceRequestList::iterator it = requests_.begin();
       it != requests_.end();
       ++it) {
    // TODO(vrk): Rename StopGeneratedStream() to CancelDeviceRequest().
    media_stream_manager_->StopGeneratedStream(it->audio_devices_label);
    media_stream_manager_->StopGeneratedStream(it->video_devices_label);
  }
  requests_.clear();
}

void DeviceRequestMessageFilter::HmacDeviceIds(
    const GURL& origin,
    const StreamDeviceInfoArray& raw_devices,
    StreamDeviceInfoArray* devices_with_guids) {
  DCHECK(devices_with_guids);

  // Replace raw ids with hmac'd ids before returning to renderer process.
  for (StreamDeviceInfoArray::const_iterator device_itr = raw_devices.begin();
       device_itr != raw_devices.end();
       ++device_itr) {
    crypto::HMAC hmac(crypto::HMAC::SHA256);
    const size_t digest_length = hmac.DigestLength();
    std::vector<uint8> digest(digest_length);
    bool result = hmac.Init(origin.spec()) &&
                  hmac.Sign(device_itr->device.id, &digest[0], digest.size());
    DCHECK(result);
    if (result) {
      StreamDeviceInfo current_device_info = *device_itr;
      current_device_info.device.id =
          StringToLowerASCII(base::HexEncode(&digest[0], digest.size()));
      devices_with_guids->push_back(current_device_info);
    }
  }
}

bool DeviceRequestMessageFilter::DoesRawIdMatchGuid(
    const GURL& security_origin,
    const std::string& device_guid,
    const std::string& raw_device_id) {
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  bool result = hmac.Init(security_origin.spec());
  DCHECK(result);
  std::vector<uint8> converted_guid;
  base::HexStringToBytes(device_guid, &converted_guid);
  return hmac.Verify(
      raw_device_id,
      base::StringPiece(reinterpret_cast<const char*>(&converted_guid[0]),
                        converted_guid.size()));
}

void DeviceRequestMessageFilter::OnGetSources(int request_id,
                                              const GURL& security_origin) {
  // Make request to get audio devices.
  const std::string& audio_label = media_stream_manager_->EnumerateDevices(
      this, -1, -1, -1, MEDIA_DEVICE_AUDIO_CAPTURE, security_origin);
  DCHECK(!audio_label.empty());

  // Make request for video devices.
  const std::string& video_label = media_stream_manager_->EnumerateDevices(
      this, -1, -1, -1, MEDIA_DEVICE_VIDEO_CAPTURE, security_origin);
  DCHECK(!video_label.empty());

  requests_.push_back(DeviceRequest(
      request_id, security_origin, audio_label, video_label));
}

}  // namespace content
