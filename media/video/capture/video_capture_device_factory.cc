// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/video_capture_device_factory.h"

#include "base/command_line.h"
#include "media/base/media_switches.h"
#include "media/video/capture/fake_video_capture_device_factory.h"
#include "media/video/capture/file_video_capture_device_factory.h"

#if defined(OS_MACOSX)
#include "media/video/capture/mac/video_capture_device_factory_mac.h"
#elif defined(OS_LINUX)
#include "media/video/capture/linux/video_capture_device_factory_linux.h"
#elif defined(OS_ANDROID)
#include "media/video/capture/android/video_capture_device_factory_android.h"
#endif

namespace media {

// static
scoped_ptr<VideoCaptureDeviceFactory>
    VideoCaptureDeviceFactory::CreateFactory() {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  // Use a Fake or File Video Device Factory if the command line flags are
  // present, otherwise use the normal, platform-dependent, device factory.
  if (command_line->HasSwitch(switches::kUseFakeDeviceForMediaStream)) {
    if (command_line->HasSwitch(switches::kUseFileForFakeVideoCapture)) {
      return scoped_ptr<VideoCaptureDeviceFactory>(new
          media::FileVideoCaptureDeviceFactory());
    } else {
      return scoped_ptr<VideoCaptureDeviceFactory>(new
          media::FakeVideoCaptureDeviceFactory());
    }
  } else {
#if defined(OS_MACOSX)
    return scoped_ptr<VideoCaptureDeviceFactory>(new
        VideoCaptureDeviceFactoryMac());
#elif defined(OS_LINUX)
    return scoped_ptr<VideoCaptureDeviceFactory>(new
        VideoCaptureDeviceFactoryLinux());
#elif defined(OS_LINUX)
    return scoped_ptr<VideoCaptureDeviceFactory>(new
        VideoCaptureDeviceFactoryLinux());
#elif defined(OS_ANDROID)
    return scoped_ptr<VideoCaptureDeviceFactory>(new
        VideoCaptureDeviceFactoryAndroid());
#else
    return scoped_ptr<VideoCaptureDeviceFactory>(new
        VideoCaptureDeviceFactory());
#endif
  }
}

VideoCaptureDeviceFactory::VideoCaptureDeviceFactory() {
  thread_checker_.DetachFromThread();
}

VideoCaptureDeviceFactory::~VideoCaptureDeviceFactory() {}

scoped_ptr<VideoCaptureDevice> VideoCaptureDeviceFactory::Create(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    const VideoCaptureDevice::Name& device_name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return scoped_ptr<VideoCaptureDevice>(
      VideoCaptureDevice::Create(ui_task_runner, device_name));
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
