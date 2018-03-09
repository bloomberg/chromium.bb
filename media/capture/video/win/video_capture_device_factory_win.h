// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of a VideoCaptureDeviceFactory class for Windows platforms.

#ifndef MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_FACTORY_WIN_H_
#define MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_FACTORY_WIN_H_

#include <mfidl.h>
#include "base/macros.h"
#include "media/capture/video/video_capture_device_factory.h"

namespace media {

// Extension of VideoCaptureDeviceFactory to create and manipulate Windows
// devices, via either DirectShow or MediaFoundation APIs.
class CAPTURE_EXPORT VideoCaptureDeviceFactoryWin
    : public VideoCaptureDeviceFactory {
 public:
  static bool PlatformSupportsMediaFoundation();

  VideoCaptureDeviceFactoryWin();
  ~VideoCaptureDeviceFactoryWin() override {}

  using MFEnumDeviceSourcesFunc = decltype(&MFEnumDeviceSources);

  std::unique_ptr<VideoCaptureDevice> CreateDevice(
      const VideoCaptureDeviceDescriptor& device_descriptor) override;
  void GetDeviceDescriptors(
      VideoCaptureDeviceDescriptors* device_descriptors) override;
  void GetSupportedFormats(
      const VideoCaptureDeviceDescriptor& device_descriptor,
      VideoCaptureFormats* supported_formats) override;

  void set_use_media_foundation_for_testing(bool use) {
    use_media_foundation_ = use;
  }
  void set_mf_enum_device_sources_func_for_testing(
      MFEnumDeviceSourcesFunc func) {
    mf_enum_device_sources_func_ = func;
  }

 private:
  bool use_media_foundation_;
  // In production code, when Media Foundation libraries are available,
  // |mf_enum_device_sources_func_| points to MFEnumDeviceSources. It enables
  // mock of Media Foundation API in unit tests.
  MFEnumDeviceSourcesFunc mf_enum_device_sources_func_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureDeviceFactoryWin);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_FACTORY_WIN_H_
