// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_FAKE_VIDEO_CAPTURE_DEVICE_FACTORY_H_
#define MEDIA_CAPTURE_VIDEO_FAKE_VIDEO_CAPTURE_DEVICE_FACTORY_H_

#include "media/capture/video/fake_video_capture_device.h"
#include "media/capture/video/video_capture_device_factory.h"

namespace media {

// Implementation of VideoCaptureDeviceFactory that creates fake devices
// that generate test output frames.
// By default, the factory has one device outputting I420.
// When increasing the number of devices using set_number_of_devices(), the
// second device will use Y16, and any devices beyond that will use I420.
// By default, the delivery mode of all devices is USE_OWN_BUFFERS.
// This, as well as other parameters, can be changed using command-line flags.
// See implementation of method ParseCommandLine() for details.
class CAPTURE_EXPORT FakeVideoCaptureDeviceFactory
    : public VideoCaptureDeviceFactory {
 public:
  FakeVideoCaptureDeviceFactory();
  ~FakeVideoCaptureDeviceFactory() override {}

  std::unique_ptr<VideoCaptureDevice> CreateDevice(
      const VideoCaptureDeviceDescriptor& device_descriptor) override;
  void GetDeviceDescriptors(
      VideoCaptureDeviceDescriptors* device_descriptors) override;
  void GetSupportedFormats(
      const VideoCaptureDeviceDescriptor& device_descriptor,
      VideoCaptureFormats* supported_formats) override;

  void set_number_of_devices(int number_of_devices) {
    DCHECK(thread_checker_.CalledOnValidThread());
    number_of_devices_ = number_of_devices;
  }
  int number_of_devices() {
    DCHECK(thread_checker_.CalledOnValidThread());
    return number_of_devices_;
  }

 private:
  void ParseCommandLine();

  int number_of_devices_;
  FakeVideoCaptureDeviceMaker::DeliveryMode delivery_mode_;
  float frame_rate_;
  bool command_line_parsed_ = false;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_FAKE_VIDEO_CAPTURE_DEVICE_FACTORY_H_
