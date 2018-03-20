// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of a VideoCaptureDeviceFactory class for Windows platforms.

#ifndef MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_FACTORY_WIN_H_
#define MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_FACTORY_WIN_H_

#include <mfidl.h>
#include <windows.devices.enumeration.h>

#include "base/macros.h"
#include "base/threading/thread.h"
#include "media/capture/video/video_capture_device_factory.h"

namespace media {

using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Devices::Enumeration::DeviceInformationCollection;

// Extension of VideoCaptureDeviceFactory to create and manipulate Windows
// devices, via either DirectShow or MediaFoundation APIs.
class CAPTURE_EXPORT VideoCaptureDeviceFactoryWin
    : public VideoCaptureDeviceFactory {
 public:
  static bool PlatformSupportsMediaFoundation();

  VideoCaptureDeviceFactoryWin();
  ~VideoCaptureDeviceFactoryWin() override;

  using MFEnumDeviceSourcesFunc = decltype(&MFEnumDeviceSources);

  std::unique_ptr<VideoCaptureDevice> CreateDevice(
      const VideoCaptureDeviceDescriptor& device_descriptor) override;
  void GetDeviceDescriptors(
      VideoCaptureDeviceDescriptors* device_descriptors) override;
  void GetSupportedFormats(
      const VideoCaptureDeviceDescriptor& device_descriptor,
      VideoCaptureFormats* supported_formats) override;
  void GetCameraLocationsAsync(
      std::unique_ptr<VideoCaptureDeviceDescriptors> device_descriptors,
      DeviceDescriptorsCallback result_callback) override;

  void set_use_media_foundation_for_testing(bool use) {
    use_media_foundation_ = use;
  }
  void set_mf_enum_device_sources_func_for_testing(
      MFEnumDeviceSourcesFunc func) {
    mf_enum_device_sources_func_ = func;
  }

 private:
  void EnumerateDevicesUWP(
      std::unique_ptr<VideoCaptureDeviceDescriptors> device_descriptors,
      DeviceDescriptorsCallback result_callback);
  void FoundAllDevicesUWP(
      std::unique_ptr<VideoCaptureDeviceDescriptors> device_descriptors,
      DeviceDescriptorsCallback result_callback,
      IAsyncOperation<DeviceInformationCollection*>* operation);
  void DeviceInfoReady(
      std::unique_ptr<VideoCaptureDeviceDescriptors> device_descriptors,
      DeviceDescriptorsCallback result_callback);

  bool use_media_foundation_;
  // In production code, when Media Foundation libraries are available,
  // |mf_enum_device_sources_func_| points to MFEnumDeviceSources. It enables
  // mock of Media Foundation API in unit tests.
  MFEnumDeviceSourcesFunc mf_enum_device_sources_func_ = nullptr;

  // For calling WinRT methods on a COM initiated thread.
  base::Thread com_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner_;
  std::unordered_set<IAsyncOperation<DeviceInformationCollection*>*> async_ops_;
  base::WeakPtrFactory<VideoCaptureDeviceFactoryWin> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureDeviceFactoryWin);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_FACTORY_WIN_H_
