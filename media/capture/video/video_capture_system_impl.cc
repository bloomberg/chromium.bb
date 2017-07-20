// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_system_impl.h"

#include "media/base/bind_to_current_loop.h"

namespace {

// Compares two VideoCaptureFormat by checking smallest frame_size area, then
// by _largest_ frame_rate. Used to order a VideoCaptureFormats vector so that
// the first entry for a given resolution has the largest frame rate, as needed
// by the ConsolidateCaptureFormats() method.
bool IsCaptureFormatSmaller(const media::VideoCaptureFormat& format1,
                            const media::VideoCaptureFormat& format2) {
  DCHECK(format1.frame_size.GetCheckedArea().IsValid());
  DCHECK(format2.frame_size.GetCheckedArea().IsValid());
  if (format1.frame_size.GetCheckedArea().ValueOrDefault(0) ==
      format2.frame_size.GetCheckedArea().ValueOrDefault(0)) {
    return format1.frame_rate > format2.frame_rate;
  }
  return format1.frame_size.GetCheckedArea().ValueOrDefault(0) <
         format2.frame_size.GetCheckedArea().ValueOrDefault(0);
}

bool IsCaptureFormatSizeEqual(const media::VideoCaptureFormat& format1,
                              const media::VideoCaptureFormat& format2) {
  DCHECK(format1.frame_size.GetCheckedArea().IsValid());
  DCHECK(format2.frame_size.GetCheckedArea().IsValid());
  return format1.frame_size.GetCheckedArea().ValueOrDefault(0) ==
         format2.frame_size.GetCheckedArea().ValueOrDefault(0);
}

// This function receives a list of capture formats, removes duplicated
// resolutions while keeping the highest frame rate for each, and forcing I420
// pixel format.
void ConsolidateCaptureFormats(media::VideoCaptureFormats* formats) {
  if (formats->empty())
    return;
  std::sort(formats->begin(), formats->end(), IsCaptureFormatSmaller);
  // Due to the ordering imposed, the largest frame_rate is kept while removing
  // duplicated resolutions.
  media::VideoCaptureFormats::iterator last =
      std::unique(formats->begin(), formats->end(), IsCaptureFormatSizeEqual);
  formats->erase(last, formats->end());
  // Mark all formats as I420, since this is what the renderer side will get
  // anyhow: the actual pixel format is decided at the device level.
  // Don't do this for Y16 format as it is handled separatelly.
  for (auto& format : *formats) {
    if (format.pixel_format != media::PIXEL_FORMAT_Y16)
      format.pixel_format = media::PIXEL_FORMAT_I420;
  }
}

}  // anonymous namespace

namespace media {

VideoCaptureSystemImpl::VideoCaptureSystemImpl(
    std::unique_ptr<VideoCaptureDeviceFactory> factory)
    : factory_(std::move(factory)) {
  thread_checker_.DetachFromThread();
}

VideoCaptureSystemImpl::~VideoCaptureSystemImpl() = default;

void VideoCaptureSystemImpl::GetDeviceInfosAsync(
    DeviceInfoCallback result_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  VideoCaptureDeviceDescriptors descriptors;
  factory_->GetDeviceDescriptors(&descriptors);
  // For devices for which we already have an entry in |devices_info_cache_|,
  // we do not want to query the |factory_| for supported formats again. We
  // simply copy them from |devices_info_cache_|.
  std::vector<VideoCaptureDeviceInfo> new_devices_info_cache;
  new_devices_info_cache.reserve(descriptors.size());
  for (const auto& descriptor : descriptors) {
    if (auto* cached_info = LookupDeviceInfoFromId(descriptor.device_id)) {
      new_devices_info_cache.push_back(*cached_info);
    } else {
      // Query for supported formats in order to create the entry.
      VideoCaptureDeviceInfo device_info(descriptor);
      factory_->GetSupportedFormats(descriptor, &device_info.supported_formats);
      ConsolidateCaptureFormats(&device_info.supported_formats);
      new_devices_info_cache.push_back(device_info);
    }
  }

  devices_info_cache_.swap(new_devices_info_cache);
  base::ResetAndReturn(&result_callback).Run(devices_info_cache_);
}

// Creates a VideoCaptureDevice object. Returns NULL if something goes wrong.
std::unique_ptr<VideoCaptureDevice> VideoCaptureSystemImpl::CreateDevice(
    const std::string& device_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const VideoCaptureDeviceInfo* device_info = LookupDeviceInfoFromId(device_id);
  if (!device_info)
    return nullptr;
  return factory_->CreateDevice(device_info->descriptor);
}

const VideoCaptureDeviceInfo* VideoCaptureSystemImpl::LookupDeviceInfoFromId(
    const std::string& device_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto iter = std::find_if(
      devices_info_cache_.begin(), devices_info_cache_.end(),
      [&device_id](const media::VideoCaptureDeviceInfo& device_info) {
        return device_info.descriptor.device_id == device_id;
      });
  if (iter == devices_info_cache_.end())
    return nullptr;
  return &(*iter);
}

}  // namespace media
