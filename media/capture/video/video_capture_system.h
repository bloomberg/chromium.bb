// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_SYSTEM_H_
#define MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_SYSTEM_H_

#include "media/capture/video/video_capture_device_factory.h"
#include "media/capture/video/video_capture_device_info.h"

namespace media {

// Layer on top of VideoCaptureDeviceFactory that translates device descriptors
// to string identifiers and consolidates and caches device descriptors and
// supported formats into VideoCaptureDeviceInfos.
class CAPTURE_EXPORT VideoCaptureSystem {
 public:
  using DeviceInfoCallback =
      base::Callback<void(const std::vector<VideoCaptureDeviceInfo>&)>;

  explicit VideoCaptureSystem(
      std::unique_ptr<VideoCaptureDeviceFactory> factory);
  ~VideoCaptureSystem();

  // The passed-in |result_callback| must have ownership of the called
  // VideoCaptureSystem instance to guarantee that it stays alive during the
  // asynchronous operation.
  void GetDeviceInfosAsync(const DeviceInfoCallback& result_callback);

  // Creates a VideoCaptureDevice object. Returns nullptr if something goes
  // wrong.
  std::unique_ptr<VideoCaptureDevice> CreateDevice(
      const std::string& device_id);

  media::VideoCaptureDeviceFactory* video_capture_device_factory() const {
    return factory_.get();
  }

 private:
  void OnDescriptorsReceived(
      const DeviceInfoCallback& result_callback,
      std::unique_ptr<VideoCaptureDeviceDescriptors> descriptors);

  // Returns nullptr if no descriptor found.
  const VideoCaptureDeviceInfo* LookupDeviceInfoFromId(
      const std::string& device_id);

  const std::unique_ptr<VideoCaptureDeviceFactory> factory_;
  std::vector<VideoCaptureDeviceInfo> devices_info_cache_;

  base::ThreadChecker thread_checker_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_FACTORY_H_
