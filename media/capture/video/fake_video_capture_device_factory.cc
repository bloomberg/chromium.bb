// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/fake_video_capture_device_factory.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"

namespace media {
namespace {

static const size_t kDepthDeviceIndex = 1;

// Cap the frame rate command line input to reasonable values.
static const float kFakeCaptureMinFrameRate = 5.0f;
static const float kFakeCaptureMaxFrameRate = 60.0f;
// Default rate if none is specified as part of the command line.
static const float kFakeCaptureDefaultFrameRate = 20.0f;
// Cap the device count command line input to reasonable values.
static const int kFakeCaptureMinDeviceCount = 1;
static const int kFakeCaptureMaxDeviceCount = 10;

FakeVideoCaptureDeviceMaker::PixelFormat GetPixelFormatFromDeviceId(
    const std::string& device_id) {
  if (device_id == "/dev/video1")
    return FakeVideoCaptureDeviceMaker::PixelFormat::Y16;
  if (device_id == "/dev/video2")
    return FakeVideoCaptureDeviceMaker::PixelFormat::MJPEG;
  return FakeVideoCaptureDeviceMaker::PixelFormat::I420;
}

}  // anonymous namespace

FakeVideoCaptureDeviceFactory::FakeVideoCaptureDeviceFactory()
    : number_of_devices_(1),
      delivery_mode_(FakeVideoCaptureDeviceMaker::DeliveryMode::
                         USE_DEVICE_INTERNAL_BUFFERS),
      frame_rate_(kFakeCaptureDefaultFrameRate) {}

std::unique_ptr<VideoCaptureDevice> FakeVideoCaptureDeviceFactory::CreateDevice(
    const VideoCaptureDeviceDescriptor& device_descriptor) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ParseCommandLine();

  for (int n = 0; n < number_of_devices_; ++n) {
    std::string possible_id = base::StringPrintf("/dev/video%d", n);
    if (device_descriptor.device_id.compare(possible_id) == 0) {
      FakeVideoCaptureDeviceMaker::PixelFormat pixel_format =
          GetPixelFormatFromDeviceId(possible_id);
      FakeVideoCaptureDeviceMaker::DeliveryMode delivery_mode = delivery_mode_;
      if (delivery_mode == FakeVideoCaptureDeviceMaker::DeliveryMode::
                               USE_CLIENT_PROVIDED_BUFFERS &&
          pixel_format == FakeVideoCaptureDeviceMaker::PixelFormat::MJPEG) {
        // Incompatible options. Fall back to using internal buffers.
        delivery_mode = FakeVideoCaptureDeviceMaker::DeliveryMode::
            USE_DEVICE_INTERNAL_BUFFERS;
      }
      return FakeVideoCaptureDeviceMaker::MakeInstance(
          pixel_format, delivery_mode, frame_rate_);
    }
  }
  return std::unique_ptr<VideoCaptureDevice>();
}

void FakeVideoCaptureDeviceFactory::GetDeviceDescriptors(
    VideoCaptureDeviceDescriptors* device_descriptors) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(device_descriptors->empty());

  ParseCommandLine();

  for (int n = 0; n < number_of_devices_; ++n) {
    device_descriptors->emplace_back(base::StringPrintf("fake_device_%d", n),
                                     base::StringPrintf("/dev/video%d", n),
#if defined(OS_LINUX)
                                     VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE
#elif defined(OS_MACOSX)
                                     VideoCaptureApi::MACOSX_AVFOUNDATION
#elif defined(OS_WIN)
                                     VideoCaptureApi::WIN_DIRECT_SHOW
#elif defined(OS_ANDROID)
                                     VideoCaptureApi::ANDROID_API2_LEGACY
#endif
                                     );
  }

  // Video device on index 1 (kDepthDeviceIndex) is depth video capture device.
  // Fill the camera calibration information only for it.
  if (device_descriptors->size() <= kDepthDeviceIndex)
    return;
  VideoCaptureDeviceDescriptor& depth_device(
      (*device_descriptors)[kDepthDeviceIndex]);
  depth_device.camera_calibration.emplace();
  depth_device.camera_calibration->focal_length_x = 135.0;
  depth_device.camera_calibration->focal_length_y = 135.6;
  depth_device.camera_calibration->depth_near = 0.0;
  depth_device.camera_calibration->depth_far = 65.535;
}

void FakeVideoCaptureDeviceFactory::GetSupportedFormats(
    const VideoCaptureDeviceDescriptor& device_descriptor,
    VideoCaptureFormats* supported_formats) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ParseCommandLine();

  const VideoPixelFormat pixel_format = static_cast<VideoPixelFormat>(
      GetPixelFormatFromDeviceId(device_descriptor.device_id));
  const VideoPixelStorage pixel_storage = PIXEL_STORAGE_CPU;
  std::vector<gfx::Size> supported_sizes;
  FakeVideoCaptureDeviceMaker::GetSupportedSizes(&supported_sizes);
  for (const auto& supported_size : supported_sizes) {
    supported_formats->emplace_back(supported_size, frame_rate_, pixel_format,
                                    pixel_storage);
  }
}

// Optional comma delimited parameters to the command line can specify buffer
// ownership, device count, and the fake video devices FPS.
// Examples: "ownership=client, device-count=2, fps=60" "fps=30"
void FakeVideoCaptureDeviceFactory::ParseCommandLine() {
  if (command_line_parsed_)
    return;
  command_line_parsed_ = true;

  const std::string option =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kUseFakeDeviceForMediaStream);
  base::StringTokenizer option_tokenizer(option, ", ");
  option_tokenizer.set_quote_chars("\"");

  while (option_tokenizer.GetNext()) {
    std::vector<std::string> param =
        base::SplitString(option_tokenizer.token(), "=", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);

    if (param.size() != 2u) {
      LOG(WARNING) << "Forget a value '" << option << "'? Use name=value for "
                   << switches::kUseFakeDeviceForMediaStream << ".";
      return;
    }

    if (base::EqualsCaseInsensitiveASCII(param.front(), "ownership") &&
        base::EqualsCaseInsensitiveASCII(param.back(), "client")) {
      delivery_mode_ = FakeVideoCaptureDeviceMaker::DeliveryMode::
          USE_CLIENT_PROVIDED_BUFFERS;
    } else if (base::EqualsCaseInsensitiveASCII(param.front(), "fps")) {
      double fps = 0;
      if (base::StringToDouble(param.back(), &fps)) {
        frame_rate_ =
            std::max(kFakeCaptureMinFrameRate, static_cast<float>(fps));
        frame_rate_ = std::min(kFakeCaptureMaxFrameRate, frame_rate_);
      }
    } else if (base::EqualsCaseInsensitiveASCII(param.front(),
                                                "device-count")) {
      unsigned int count = 0;
      if (base::StringToUint(param.back(), &count)) {
        number_of_devices_ = std::min(
            kFakeCaptureMaxDeviceCount,
            std::max(kFakeCaptureMinDeviceCount, static_cast<int>(count)));
      }
    }
  }
}

}  // namespace media
