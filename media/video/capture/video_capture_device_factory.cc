// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/video_capture_device_factory.h"

namespace media {

VideoCaptureDeviceFactory::VideoCaptureDeviceFactory() {
  thread_checker_.DetachFromThread();
};

scoped_ptr<VideoCaptureDevice> VideoCaptureDeviceFactory::Create(
    const VideoCaptureDevice::Name& device_name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return scoped_ptr<VideoCaptureDevice>(
      VideoCaptureDevice::Create(device_name));
}

void VideoCaptureDeviceFactory::GetDeviceNames(
    VideoCaptureDevice::Names* device_names) {
  DCHECK(thread_checker_.CalledOnValidThread());
  VideoCaptureDevice::GetDeviceNames(device_names);
}

void VideoCaptureDeviceFactory::GetDeviceSupportedFormats(
    const VideoCaptureDevice::Name& device,
    VideoCaptureFormats* supported_formats) {
  DCHECK(thread_checker_.CalledOnValidThread());
  VideoCaptureDevice::GetDeviceSupportedFormats(device, supported_formats);
}

}  // namespace media
