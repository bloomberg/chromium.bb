// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/fake_video_capture_device_factory.h"

#include <array>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "media/base/media_switches.h"

namespace {

static const size_t kDepthDeviceIndex = 1;

// Cap the frame rate command line input to reasonable values.
static const float kFakeCaptureMinFrameRate = 5.0f;
static const float kFakeCaptureMaxFrameRate = 60.0f;

// Cap the device count command line input to reasonable values.
static const int kFakeCaptureMinDeviceCount = 0;
static const int kFakeCaptureMaxDeviceCount = 10;
static const int kDefaultDeviceCount = 1;

static const char* kDefaultDeviceIdMask = "/dev/video%d";
static const media::FakeVideoCaptureDevice::DeliveryMode kDefaultDeliveryMode =
    media::FakeVideoCaptureDevice::DeliveryMode::USE_DEVICE_INTERNAL_BUFFERS;
static constexpr std::array<gfx::Size, 5> kDefaultResolutions{
    {gfx::Size(96, 96), gfx::Size(320, 240), gfx::Size(640, 480),
     gfx::Size(1280, 720), gfx::Size(1920, 1080)}};
static constexpr std::array<float, 1> kDefaultFrameRates{{20.0f}};

static const double kInitialZoom = 100.0;

static const media::VideoPixelFormat kSupportedPixelFormats[] = {
    media::PIXEL_FORMAT_I420, media::PIXEL_FORMAT_Y16,
    media::PIXEL_FORMAT_MJPEG};

template <typename TElement, size_t TSize>
std::vector<TElement> ArrayToVector(const std::array<TElement, TSize>& arr) {
  return std::vector<TElement>(arr.begin(), arr.end());
}

media::VideoPixelFormat GetPixelFormatFromDeviceIndex(int device_index) {
  if (device_index == 1)
    return media::PIXEL_FORMAT_Y16;
  if (device_index == 2)
    return media::PIXEL_FORMAT_MJPEG;
  return media::PIXEL_FORMAT_I420;
}

void AppendAllCombinationsToFormatsContainer(
    const std::vector<media::VideoPixelFormat>& pixel_formats,
    const std::vector<gfx::Size>& resolutions,
    const std::vector<float>& frame_rates,
    media::VideoCaptureFormats* output) {
  for (const auto& pixel_format : pixel_formats) {
    for (const auto& resolution : resolutions) {
      for (const auto& frame_rate : frame_rates)
        output->emplace_back(resolution, frame_rate, pixel_format);
    }
  }
}

class ErrorFakeDevice : public media::VideoCaptureDevice {
 public:
  // VideoCaptureDevice implementation.
  void AllocateAndStart(const media::VideoCaptureParams& params,
                        std::unique_ptr<Client> client) override {
    client->OnError(FROM_HERE, "Device has no supported formats.");
  }

  void StopAndDeAllocate() override {}
  void GetPhotoCapabilities(GetPhotoCapabilitiesCallback callback) override {}
  void SetPhotoOptions(media::mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override {}
  void TakePhoto(TakePhotoCallback callback) override {}
};

}  // anonymous namespace

namespace media {

FakeVideoCaptureDeviceSettings::FakeVideoCaptureDeviceSettings() = default;

FakeVideoCaptureDeviceSettings::~FakeVideoCaptureDeviceSettings() = default;

FakeVideoCaptureDeviceSettings::FakeVideoCaptureDeviceSettings(
    const FakeVideoCaptureDeviceSettings& other) = default;

FakeVideoCaptureDeviceFactory::FakeVideoCaptureDeviceFactory() {
  // The default |devices_config_| is the one obtained from an empty options
  // string.
  ParseFakeDevicesConfigFromOptionsString("", &devices_config_);
}

FakeVideoCaptureDeviceFactory::~FakeVideoCaptureDeviceFactory() = default;

// static
std::unique_ptr<VideoCaptureDevice>
FakeVideoCaptureDeviceFactory::CreateDeviceWithSupportedFormats(
    FakeVideoCaptureDevice::DeliveryMode delivery_mode,
    const VideoCaptureFormats& formats) {
  if (formats.empty())
    return CreateErrorDevice();

  for (const auto& entry : formats) {
    bool pixel_format_supported = false;
    for (const auto& supported_pixel_format : kSupportedPixelFormats) {
      if (entry.pixel_format == supported_pixel_format) {
        pixel_format_supported = true;
        break;
      }
    }
    if (!pixel_format_supported) {
      DLOG(ERROR) << "Requested an unsupported pixel format "
                  << VideoPixelFormatToString(entry.pixel_format);
      return nullptr;
    }
  }

  const VideoCaptureFormat& initial_format = formats.front();
  auto device_state = base::MakeUnique<FakeDeviceState>(
      kInitialZoom, initial_format.frame_rate, initial_format.pixel_format);

  auto photo_frame_painter = base::MakeUnique<PacmanFramePainter>(
      PacmanFramePainter::Format::SK_N32, device_state.get());
  auto photo_device = base::MakeUnique<FakePhotoDevice>(
      std::move(photo_frame_painter), device_state.get());

  return base::MakeUnique<FakeVideoCaptureDevice>(
      formats,
      base::MakeUnique<FrameDelivererFactory>(delivery_mode,
                                              device_state.get()),
      std::move(photo_device), std::move(device_state));
}

// static
std::unique_ptr<VideoCaptureDevice>
FakeVideoCaptureDeviceFactory::CreateDeviceWithDefaultResolutions(
    VideoPixelFormat pixel_format,
    FakeVideoCaptureDevice::DeliveryMode delivery_mode,
    float frame_rate) {
  VideoCaptureFormats formats;
  for (const gfx::Size& resolution : kDefaultResolutions)
    formats.emplace_back(resolution, frame_rate, pixel_format);
  return CreateDeviceWithSupportedFormats(delivery_mode, formats);
}

// static
std::unique_ptr<VideoCaptureDevice>
FakeVideoCaptureDeviceFactory::CreateErrorDevice() {
  return base::MakeUnique<ErrorFakeDevice>();
}

void FakeVideoCaptureDeviceFactory::SetToDefaultDevicesConfig(
    int device_count) {
  devices_config_.clear();
  ParseFakeDevicesConfigFromOptionsString(
      base::StringPrintf("device-count=%d", device_count), &devices_config_);
}

void FakeVideoCaptureDeviceFactory::SetToCustomDevicesConfig(
    const std::vector<FakeVideoCaptureDeviceSettings>& config) {
  devices_config_ = config;
}

std::unique_ptr<VideoCaptureDevice> FakeVideoCaptureDeviceFactory::CreateDevice(
    const VideoCaptureDeviceDescriptor& device_descriptor) {
  DCHECK(thread_checker_.CalledOnValidThread());

  for (const auto& entry : devices_config_) {
    if (device_descriptor.device_id != entry.device_id)
      continue;
    return CreateDeviceWithSupportedFormats(entry.delivery_mode,
                                            entry.supported_formats);
  }
  return nullptr;
}

void FakeVideoCaptureDeviceFactory::GetDeviceDescriptors(
    VideoCaptureDeviceDescriptors* device_descriptors) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(device_descriptors->empty());

  int entry_index = 0;
  for (const auto& entry : devices_config_) {
    device_descriptors->emplace_back(
        base::StringPrintf("fake_device_%d", entry_index), entry.device_id,
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
    entry_index++;
  }

  // Video device on index |kDepthDeviceIndex| is depth video capture device.
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

  for (const auto& entry : devices_config_) {
    if (device_descriptor.device_id != entry.device_id)
      continue;
    supported_formats->insert(supported_formats->end(),
                              entry.supported_formats.begin(),
                              entry.supported_formats.end());
  }
}

// static
void FakeVideoCaptureDeviceFactory::ParseFakeDevicesConfigFromOptionsString(
    const std::string options_string,
    std::vector<FakeVideoCaptureDeviceSettings>* config) {
  base::StringTokenizer option_tokenizer(options_string, ", ");
  option_tokenizer.set_quote_chars("\"");

  FakeVideoCaptureDevice::DeliveryMode delivery_mode = kDefaultDeliveryMode;
  std::vector<gfx::Size> resolutions = ArrayToVector(kDefaultResolutions);
  std::vector<float> frame_rates = ArrayToVector(kDefaultFrameRates);
  int device_count = kDefaultDeviceCount;

  while (option_tokenizer.GetNext()) {
    std::vector<std::string> param =
        base::SplitString(option_tokenizer.token(), "=", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);

    if (param.size() != 2u) {
      LOG(WARNING) << "Forget a value '" << options_string
                   << "'? Use name=value for "
                   << switches::kUseFakeDeviceForMediaStream << ".";
      return;
    }

    if (base::EqualsCaseInsensitiveASCII(param.front(), "ownership") &&
        base::EqualsCaseInsensitiveASCII(param.back(), "client")) {
      delivery_mode =
          FakeVideoCaptureDevice::DeliveryMode::USE_CLIENT_PROVIDED_BUFFERS;
    } else if (base::EqualsCaseInsensitiveASCII(param.front(), "fps")) {
      double parsed_fps = 0;
      if (base::StringToDouble(param.back(), &parsed_fps)) {
        float capped_frame_rate =
            std::max(kFakeCaptureMinFrameRate, static_cast<float>(parsed_fps));
        capped_frame_rate =
            std::min(kFakeCaptureMaxFrameRate, capped_frame_rate);
        frame_rates.clear();
        frame_rates.push_back(capped_frame_rate);
      }
    } else if (base::EqualsCaseInsensitiveASCII(param.front(),
                                                "device-count")) {
      unsigned int count = 0;
      if (base::StringToUint(param.back(), &count)) {
        device_count = std::min(
            kFakeCaptureMaxDeviceCount,
            std::max(kFakeCaptureMinDeviceCount, static_cast<int>(count)));
      }
    }
  }

  for (int device_index = 0; device_index < device_count; device_index++) {
    std::vector<VideoPixelFormat> pixel_formats;
    pixel_formats.push_back(GetPixelFormatFromDeviceIndex(device_index));
    FakeVideoCaptureDeviceSettings settings;
    settings.delivery_mode = delivery_mode;
    settings.device_id = base::StringPrintf(kDefaultDeviceIdMask, device_index);
    AppendAllCombinationsToFormatsContainer(
        pixel_formats, resolutions, frame_rates, &settings.supported_formats);
    config->push_back(settings);
  }
}

}  // namespace media
