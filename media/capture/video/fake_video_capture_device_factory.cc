// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/fake_video_capture_device_factory.h"

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "media/base/media_switches.h"
#include "media/capture/video/fake_video_capture_device.h"

namespace media {

FakeVideoCaptureDeviceFactory::FakeVideoCaptureDeviceFactory()
    : number_of_devices_(1) {
}

scoped_ptr<VideoCaptureDevice> FakeVideoCaptureDeviceFactory::Create(
    const VideoCaptureDevice::Name& device_name) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const std::string option =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kUseFakeDeviceForMediaStream);

  const FakeVideoCaptureDevice::BufferOwnership fake_vcd_ownership =
      base::StartsWith(option, "client", base::CompareCase::INSENSITIVE_ASCII)
          ? FakeVideoCaptureDevice::BufferOwnership::CLIENT_BUFFERS
          : FakeVideoCaptureDevice::BufferOwnership::OWN_BUFFERS;

  const FakeVideoCaptureDevice::BufferPlanarity fake_vcd_planarity =
      base::EndsWith(option, "triplanar", base::CompareCase::INSENSITIVE_ASCII)
          ? FakeVideoCaptureDevice::BufferPlanarity::TRIPLANAR
          : FakeVideoCaptureDevice::BufferPlanarity::PACKED;

  for (int n = 0; n < number_of_devices_; ++n) {
    std::string possible_id = base::StringPrintf("/dev/video%d", n);
    if (device_name.id().compare(possible_id) == 0) {
      return scoped_ptr<VideoCaptureDevice>(
          new FakeVideoCaptureDevice(fake_vcd_ownership, fake_vcd_planarity));
    }
  }
  return scoped_ptr<VideoCaptureDevice>();
}

void FakeVideoCaptureDeviceFactory::GetDeviceNames(
    VideoCaptureDevice::Names* const device_names) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(device_names->empty());
  for (int n = 0; n < number_of_devices_; ++n) {
    VideoCaptureDevice::Name name(base::StringPrintf("fake_device_%d", n),
                                  base::StringPrintf("/dev/video%d", n)
#if defined(OS_LINUX)
                                      ,
                                  VideoCaptureDevice::Name::V4L2_SINGLE_PLANE
#elif defined(OS_MACOSX)
                                      ,
                                  VideoCaptureDevice::Name::AVFOUNDATION
#elif defined(OS_WIN)
                                      ,
                                  VideoCaptureDevice::Name::DIRECT_SHOW
#elif defined(OS_ANDROID)
                                      ,
                                  VideoCaptureDevice::Name::API2_LEGACY
#endif
                                  );
    device_names->push_back(name);
  }
}

void FakeVideoCaptureDeviceFactory::GetDeviceSupportedFormats(
    const VideoCaptureDevice::Name& device,
    VideoCaptureFormats* supported_formats) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const int frame_rate = 1000 / FakeVideoCaptureDevice::FakeCapturePeriodMs();
  const gfx::Size supported_sizes[] = {gfx::Size(320, 240),
                                       gfx::Size(640, 480),
                                       gfx::Size(1280, 720),
                                       gfx::Size(1920, 1080)};
  supported_formats->clear();
  for (const auto& size : supported_sizes) {
    supported_formats->push_back(
        VideoCaptureFormat(size, frame_rate, media::PIXEL_FORMAT_I420));
  }
}

}  // namespace media
