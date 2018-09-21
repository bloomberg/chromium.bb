// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/fake_video_capture_device.h"

#include <stddef.h>
#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/base/video_frame.h"
#include "media/capture/mojom/image_capture_types.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"

namespace media {

namespace {

// Sweep at 600 deg/sec.
static const float kPacmanAngularVelocity = 600;
// Beep every 500 ms.
static const int kBeepInterval = 500;
// Gradient travels from bottom to top in 5 seconds.
static const float kGradientFrequency = 1.f / 5;

static const double kMinZoom = 100.0;
static const double kMaxZoom = 400.0;
static const double kZoomStep = 1.0;

// Larger int means better.
enum class PixelFormatMatchType : int {
  INCOMPATIBLE = 0,
  SUPPORTED_THROUGH_CONVERSION = 1,
  EXACT = 2
};

PixelFormatMatchType DetermineFormatMatchType(
    VideoPixelFormat supported_format,
    VideoPixelFormat requested_format) {
  if (requested_format == PIXEL_FORMAT_I420 &&
      supported_format == PIXEL_FORMAT_MJPEG) {
    return PixelFormatMatchType::SUPPORTED_THROUGH_CONVERSION;
  }
  return (requested_format == supported_format)
             ? PixelFormatMatchType::EXACT
             : PixelFormatMatchType::INCOMPATIBLE;
}

const VideoCaptureFormat& FindClosestSupportedFormat(
    const VideoCaptureFormat& requested_format,
    const VideoCaptureFormats& supported_formats) {
  DCHECK(!supported_formats.empty());
  int best_index = 0;
  PixelFormatMatchType best_format_match = PixelFormatMatchType::INCOMPATIBLE;
  int best_width_mismatch = std::numeric_limits<int>::max();
  float best_frame_rate_mismatch = std::numeric_limits<float>::max();
  for (int i = 0; i < static_cast<int>(supported_formats.size()); i++) {
    const auto& supported_format = supported_formats[i];
    PixelFormatMatchType current_format_match = DetermineFormatMatchType(
        supported_format.pixel_format, requested_format.pixel_format);
    if (current_format_match < best_format_match) {
      continue;
    }
    if (supported_format.frame_size.width() <
        requested_format.frame_size.width())
      continue;
    const int current_width_mismatch = supported_format.frame_size.width() -
                                       requested_format.frame_size.width();
    if (current_width_mismatch > best_width_mismatch)
      continue;
    const float current_frame_rate_mismatch =
        std::abs(supported_format.frame_rate - requested_format.frame_rate);
    if (current_width_mismatch < best_width_mismatch) {
      best_width_mismatch = current_width_mismatch;
      best_frame_rate_mismatch = current_frame_rate_mismatch;
      best_index = i;
      continue;
    }
    DCHECK_EQ(best_frame_rate_mismatch, current_frame_rate_mismatch);
    if (current_frame_rate_mismatch < best_frame_rate_mismatch) {
      best_frame_rate_mismatch = current_frame_rate_mismatch;
      best_index = i;
    }
  }
  return supported_formats[best_index];
}

}  // anonymous namespace

// Paints and delivers frames to a client, which is set via Initialize().
class FrameDeliverer {
 public:
  FrameDeliverer(std::unique_ptr<PacmanFramePainter> frame_painter)
      : frame_painter_(std::move(frame_painter)) {}
  virtual ~FrameDeliverer() = default;
  virtual void Initialize(VideoPixelFormat pixel_format,
                          std::unique_ptr<VideoCaptureDevice::Client> client,
                          const FakeDeviceState* device_state) {
    client_ = std::move(client);
    device_state_ = device_state;
    client_->OnStarted();
  }
  virtual void PaintAndDeliverNextFrame(base::TimeDelta timestamp_to_paint) = 0;

 protected:
  base::TimeDelta CalculateTimeSinceFirstInvocation(base::TimeTicks now) {
    if (first_ref_time_.is_null())
      first_ref_time_ = now;
    return now - first_ref_time_;
  }

  PacmanFramePainter* frame_painter() { return frame_painter_.get(); }
  const FakeDeviceState* device_state() { return device_state_; }
  VideoCaptureDevice::Client* client() { return client_.get(); }

 private:
  const std::unique_ptr<PacmanFramePainter> frame_painter_;
  const FakeDeviceState* device_state_ = nullptr;
  std::unique_ptr<VideoCaptureDevice::Client> client_;
  base::TimeTicks first_ref_time_;
};

// Delivers frames using its own buffers via OnIncomingCapturedData().
class OwnBufferFrameDeliverer : public FrameDeliverer {
 public:
  OwnBufferFrameDeliverer(std::unique_ptr<PacmanFramePainter> frame_painter);
  ~OwnBufferFrameDeliverer() override;

  // Implementation of FrameDeliverer
  void Initialize(VideoPixelFormat pixel_format,
                  std::unique_ptr<VideoCaptureDevice::Client> client,
                  const FakeDeviceState* device_state) override;
  void PaintAndDeliverNextFrame(base::TimeDelta timestamp_to_paint) override;

 private:
  std::unique_ptr<uint8_t[]> buffer_;
};

// Delivers frames using buffers provided by the client via
// OnIncomingCapturedBuffer().
class ClientBufferFrameDeliverer : public FrameDeliverer {
 public:
  ClientBufferFrameDeliverer(std::unique_ptr<PacmanFramePainter> frame_painter);
  ~ClientBufferFrameDeliverer() override;

  // Implementation of FrameDeliverer
  void PaintAndDeliverNextFrame(base::TimeDelta timestamp_to_paint) override;
};

class JpegEncodingFrameDeliverer : public FrameDeliverer {
 public:
  JpegEncodingFrameDeliverer(std::unique_ptr<PacmanFramePainter> frame_painter);
  ~JpegEncodingFrameDeliverer() override;

  // Implementation of FrameDeliveryStrategy
  void PaintAndDeliverNextFrame(base::TimeDelta timestamp_to_paint) override;

 private:
  std::vector<uint8_t> sk_n32_buffer_;
  std::vector<unsigned char> jpeg_buffer_;
};

FrameDelivererFactory::FrameDelivererFactory(
    FakeVideoCaptureDevice::DeliveryMode delivery_mode,
    const FakeDeviceState* device_state)
    : delivery_mode_(delivery_mode), device_state_(device_state) {}

std::unique_ptr<FrameDeliverer> FrameDelivererFactory::CreateFrameDeliverer(
    const VideoCaptureFormat& format) {
  PacmanFramePainter::Format painter_format;
  switch (format.pixel_format) {
    case PIXEL_FORMAT_I420:
      painter_format = PacmanFramePainter::Format::I420;
      break;
    case PIXEL_FORMAT_Y16:
      painter_format = PacmanFramePainter::Format::Y16;
      break;
    case PIXEL_FORMAT_MJPEG:
      painter_format = PacmanFramePainter::Format::SK_N32;
      break;
    default:
      NOTREACHED();
      painter_format = PacmanFramePainter::Format::I420;
  }
  auto frame_painter =
      std::make_unique<PacmanFramePainter>(painter_format, device_state_);

  FakeVideoCaptureDevice::DeliveryMode delivery_mode = delivery_mode_;
  if (format.pixel_format == PIXEL_FORMAT_MJPEG &&
      delivery_mode_ ==
          FakeVideoCaptureDevice::DeliveryMode::USE_CLIENT_PROVIDED_BUFFERS) {
    DLOG(WARNING) << "PIXEL_FORMAT_MJPEG cannot be used in combination with "
                  << "USE_CLIENT_PROVIDED_BUFFERS. Switching to "
                     "USE_DEVICE_INTERNAL_BUFFERS.";
    delivery_mode =
        FakeVideoCaptureDevice::DeliveryMode::USE_DEVICE_INTERNAL_BUFFERS;
  }

  switch (delivery_mode) {
    case FakeVideoCaptureDevice::DeliveryMode::USE_DEVICE_INTERNAL_BUFFERS:
      if (format.pixel_format == PIXEL_FORMAT_MJPEG) {
        return std::make_unique<JpegEncodingFrameDeliverer>(
            std::move(frame_painter));
      } else {
        return std::make_unique<OwnBufferFrameDeliverer>(
            std::move(frame_painter));
      }
    case FakeVideoCaptureDevice::DeliveryMode::USE_CLIENT_PROVIDED_BUFFERS:
      return std::make_unique<ClientBufferFrameDeliverer>(
          std::move(frame_painter));
  }
  NOTREACHED();
  return nullptr;
}

PacmanFramePainter::PacmanFramePainter(Format pixel_format,
                                       const FakeDeviceState* fake_device_state)
    : pixel_format_(pixel_format), fake_device_state_(fake_device_state) {}

void PacmanFramePainter::PaintFrame(base::TimeDelta elapsed_time,
                                    uint8_t* target_buffer) {
  DrawPacman(elapsed_time, target_buffer);
  DrawGradientSquares(elapsed_time, target_buffer);
}

// Starting from top left, -45 deg gradient.  Value at point (row, column) is
// calculated as (top_left_value + (row + column) * step) % MAX_VALUE, where
// step is MAX_VALUE / (width + height).  MAX_VALUE is 255 (for 8 bit per
// component) or 65535 for Y16.
// This is handy for pixel tests where we use the squares to verify rendering.
void PacmanFramePainter::DrawGradientSquares(base::TimeDelta elapsed_time,
                                             uint8_t* target_buffer) {
  const int width = fake_device_state_->format.frame_size.width();
  const int height = fake_device_state_->format.frame_size.height();

  const int side = width / 16;  // square side length.
  DCHECK(side);
  const gfx::Point squares[] = {{0, 0},
                                {width - side, 0},
                                {0, height - side},
                                {width - side, height - side}};
  const float start =
      fmod(65536 * elapsed_time.InSecondsF() * kGradientFrequency, 65536);
  const float color_step = 65535 / static_cast<float>(width + height);
  for (const auto& corner : squares) {
    for (int y = corner.y(); y < corner.y() + side; ++y) {
      for (int x = corner.x(); x < corner.x() + side; ++x) {
        const unsigned int value =
            static_cast<unsigned int>(start + (x + y) * color_step) & 0xFFFF;
        size_t offset = (y * width) + x;
        switch (pixel_format_) {
          case Format::Y16:
            target_buffer[offset * sizeof(uint16_t)] = value & 0xFF;
            target_buffer[offset * sizeof(uint16_t) + 1] = value >> 8;
            break;
          case Format::SK_N32:
            target_buffer[offset * sizeof(uint32_t) + 1] = value >> 8;
            target_buffer[offset * sizeof(uint32_t) + 2] = value >> 8;
            target_buffer[offset * sizeof(uint32_t) + 3] = value >> 8;
            break;
          case Format::I420:
            target_buffer[offset] = value >> 8;
            break;
        }
      }
    }
  }
}

void PacmanFramePainter::DrawPacman(base::TimeDelta elapsed_time,
                                    uint8_t* target_buffer) {
  const int width = fake_device_state_->format.frame_size.width();
  const int height = fake_device_state_->format.frame_size.height();

  SkColorType colorspace = kAlpha_8_SkColorType;
  switch (pixel_format_) {
    case Format::I420:
      // Skia doesn't support painting in I420. Instead, paint an 8bpp
      // monochrome image to the beginning of |target_buffer|. This section of
      // |target_buffer| corresponds to the Y-plane of the YUV image. Do not
      // touch the U or V planes of |target_buffer|. Assuming they have been
      // initialized to 0, which corresponds to a green color tone, the result
      // will be an green-ish monochrome frame.
      colorspace = kAlpha_8_SkColorType;
      break;
    case Format::SK_N32:
      // SkColorType is RGBA on some platforms and BGRA on others.
      colorspace = kN32_SkColorType;
      break;
    case Format::Y16:
      // Skia doesn't support painting in Y16. Instead, paint an 8bpp monochrome
      // image to the beginning of |target_buffer|. Later, move the 8bit pixel
      // values to a position corresponding to the high byte values of 16bit
      // pixel values (assuming the byte order is little-endian).
      colorspace = kAlpha_8_SkColorType;
      break;
  }

  const SkImageInfo info =
      SkImageInfo::Make(width, height, colorspace, kOpaque_SkAlphaType);
  SkBitmap bitmap;
  bitmap.setInfo(info);
  bitmap.setPixels(target_buffer);
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  SkCanvas canvas(bitmap);

  const SkScalar unscaled_zoom = fake_device_state_->zoom / 100.f;
  SkMatrix matrix;
  matrix.setScale(unscaled_zoom, unscaled_zoom, width / 2, height / 2);
  canvas.setMatrix(matrix);

  // For the SK_N32 case, match the green color tone produced by the
  // I420 case.
  if (pixel_format_ == Format::SK_N32) {
    const SkRect full_frame = SkRect::MakeWH(width, height);
    paint.setARGB(255, 0, 127, 0);
    canvas.drawRect(full_frame, paint);
    paint.setColor(SK_ColorGREEN);
  }

  // Draw a sweeping circle to show an animation.
  const float end_angle =
      fmod(kPacmanAngularVelocity * elapsed_time.InSecondsF(), 361);
  const int radius = std::min(width, height) / 4;
  const SkRect rect = SkRect::MakeXYWH(width / 2 - radius, height / 2 - radius,
                                       2 * radius, 2 * radius);
  canvas.drawArc(rect, 0, end_angle, true, paint);

  // Draw current time.
  const int milliseconds = elapsed_time.InMilliseconds() % 1000;
  const int seconds = elapsed_time.InSeconds() % 60;
  const int minutes = elapsed_time.InMinutes() % 60;
  const int hours = elapsed_time.InHours();
  const int frame_count = elapsed_time.InMilliseconds() *
                          fake_device_state_->format.frame_rate / 1000;

  const std::string time_string =
      base::StringPrintf("%d:%02d:%02d:%03d %d", hours, minutes, seconds,
                         milliseconds, frame_count);
  canvas.scale(3, 3);
  canvas.drawText(time_string.data(), time_string.length(), 30, 20, paint);

  if (pixel_format_ == Format::Y16) {
    // Use 8 bit bitmap rendered to first half of the buffer as high byte values
    // for the whole buffer. Low byte values are not important.
    for (int i = (width * height) - 1; i >= 0; --i)
      target_buffer[i * 2 + 1] = target_buffer[i];
  }
}

FakePhotoDevice::FakePhotoDevice(
    std::unique_ptr<PacmanFramePainter> sk_n32_painter,
    const FakeDeviceState* fake_device_state,
    const FakePhotoDeviceConfig& config)
    : sk_n32_painter_(std::move(sk_n32_painter)),
      fake_device_state_(fake_device_state),
      config_(config) {}

FakePhotoDevice::~FakePhotoDevice() = default;

void FakePhotoDevice::TakePhoto(VideoCaptureDevice::TakePhotoCallback callback,
                                base::TimeDelta elapsed_time) {
  if (config_.should_fail_take_photo)
    return;

  // Create a PNG-encoded frame and send it back to |callback|.
  auto required_sk_n32_buffer_size = VideoFrame::AllocationSize(
      PIXEL_FORMAT_ARGB, fake_device_state_->format.frame_size);
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[required_sk_n32_buffer_size]);
  memset(buffer.get(), 0, required_sk_n32_buffer_size);
  sk_n32_painter_->PaintFrame(elapsed_time, buffer.get());
  mojom::BlobPtr blob = mojom::Blob::New();
  const gfx::PNGCodec::ColorFormat encoding_source_format =
      (kN32_SkColorType == kRGBA_8888_SkColorType) ? gfx::PNGCodec::FORMAT_RGBA
                                                   : gfx::PNGCodec::FORMAT_BGRA;
  const bool result = gfx::PNGCodec::Encode(
      buffer.get(), encoding_source_format,
      fake_device_state_->format.frame_size,
      VideoFrame::RowBytes(0 /* plane */, PIXEL_FORMAT_ARGB,
                           fake_device_state_->format.frame_size.width()),
      true /* discard_transparency */, std::vector<gfx::PNGCodec::Comment>(),
      &blob->data);
  DCHECK(result);

  blob->mime_type = "image/png";
  std::move(callback).Run(std::move(blob));
}

FakeVideoCaptureDevice::FakeVideoCaptureDevice(
    const VideoCaptureFormats& supported_formats,
    std::unique_ptr<FrameDelivererFactory> frame_deliverer_factory,
    std::unique_ptr<FakePhotoDevice> photo_device,
    std::unique_ptr<FakeDeviceState> device_state)
    : supported_formats_(supported_formats),
      frame_deliverer_factory_(std::move(frame_deliverer_factory)),
      photo_device_(std::move(photo_device)),
      device_state_(std::move(device_state)),
      weak_factory_(this) {}

FakeVideoCaptureDevice::~FakeVideoCaptureDevice() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void FakeVideoCaptureDevice::AllocateAndStart(
    const VideoCaptureParams& params,
    std::unique_ptr<VideoCaptureDevice::Client> client) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const VideoCaptureFormat& selected_format =
      FindClosestSupportedFormat(params.requested_format, supported_formats_);

  beep_time_ = base::TimeDelta();
  elapsed_time_ = base::TimeDelta();
  frame_deliverer_ =
      frame_deliverer_factory_->CreateFrameDeliverer(selected_format);
  device_state_->format.frame_size = selected_format.frame_size;
  frame_deliverer_->Initialize(device_state_->format.pixel_format,
                               std::move(client), device_state_.get());
  current_session_id_++;
  BeepAndScheduleNextCapture(base::TimeTicks::Now());
}

void FakeVideoCaptureDevice::StopAndDeAllocate() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Invalidate WeakPtr to stop the perpetual scheduling of tasks.
  weak_factory_.InvalidateWeakPtrs();
  frame_deliverer_.reset();
}

void FakeVideoCaptureDevice::GetPhotoState(GetPhotoStateCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  photo_device_->GetPhotoState(std::move(callback));
}

void FakePhotoDevice::GetPhotoState(
    VideoCaptureDevice::GetPhotoStateCallback callback) {
  if (config_.should_fail_get_photo_capabilities)
    return;

  mojom::PhotoStatePtr photo_state = mojo::CreateEmptyPhotoState();

  photo_state->current_white_balance_mode = mojom::MeteringMode::NONE;
  photo_state->current_exposure_mode = mojom::MeteringMode::NONE;
  photo_state->current_focus_mode = mojom::MeteringMode::NONE;

  photo_state->exposure_compensation = mojom::Range::New();
  photo_state->exposure_time = mojom::Range::New();
  photo_state->color_temperature = mojom::Range::New();
  photo_state->iso = mojom::Range::New();
  photo_state->iso->current = 100.0;
  photo_state->iso->max = 100.0;
  photo_state->iso->min = 100.0;
  photo_state->iso->step = 0.0;

  photo_state->brightness = media::mojom::Range::New();
  photo_state->contrast = media::mojom::Range::New();
  photo_state->saturation = media::mojom::Range::New();
  photo_state->sharpness = media::mojom::Range::New();

  photo_state->focus_distance = mojom::Range::New();
  photo_state->focus_distance->current = 3.0;
  photo_state->focus_distance->max = 5.0;
  photo_state->focus_distance->min = 1.0;
  photo_state->focus_distance->step = 1.0;

  photo_state->zoom = mojom::Range::New();
  photo_state->zoom->current = fake_device_state_->zoom;
  photo_state->zoom->max = kMaxZoom;
  photo_state->zoom->min = kMinZoom;
  photo_state->zoom->step = kZoomStep;

  photo_state->supports_torch = false;
  photo_state->torch = false;

  photo_state->red_eye_reduction = mojom::RedEyeReduction::NEVER;
  photo_state->height = mojom::Range::New();
  photo_state->height->current = fake_device_state_->format.frame_size.height();
  photo_state->height->max = 1080.0;
  photo_state->height->min = 96.0;
  photo_state->height->step = 1.0;
  photo_state->width = mojom::Range::New();
  photo_state->width->current = fake_device_state_->format.frame_size.width();
  photo_state->width->max = 1920.0;
  photo_state->width->min = 96.0;
  photo_state->width->step = 1.0;

  std::move(callback).Run(std::move(photo_state));
}

void FakeVideoCaptureDevice::SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                                             SetPhotoOptionsCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  photo_device_->SetPhotoOptions(std::move(settings), std::move(callback),
                                 device_state_.get());
}

void FakePhotoDevice::SetPhotoOptions(
    mojom::PhotoSettingsPtr settings,
    VideoCaptureDevice::SetPhotoOptionsCallback callback,
    FakeDeviceState* device_state_write_access) {
  if (config_.should_fail_set_photo_options)
    return;

  if (settings->has_zoom) {
    device_state_write_access->zoom =
        std::max(kMinZoom, std::min(settings->zoom, kMaxZoom));
  }

  std::move(callback).Run(true);
}

void FakeVideoCaptureDevice::TakePhoto(TakePhotoCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&FakePhotoDevice::TakePhoto,
                            base::Unretained(photo_device_.get()),
                            base::Passed(&callback), elapsed_time_));
}

OwnBufferFrameDeliverer::OwnBufferFrameDeliverer(
    std::unique_ptr<PacmanFramePainter> frame_painter)
    : FrameDeliverer(std::move(frame_painter)) {}

OwnBufferFrameDeliverer::~OwnBufferFrameDeliverer() = default;

void OwnBufferFrameDeliverer::Initialize(
    VideoPixelFormat pixel_format,
    std::unique_ptr<VideoCaptureDevice::Client> client,
    const FakeDeviceState* device_state) {
  FrameDeliverer::Initialize(pixel_format, std::move(client), device_state);
  buffer_.reset(new uint8_t[VideoFrame::AllocationSize(
      pixel_format, device_state->format.frame_size)]);
}

void OwnBufferFrameDeliverer::PaintAndDeliverNextFrame(
    base::TimeDelta timestamp_to_paint) {
  if (!client())
    return;
  const size_t frame_size = device_state()->format.ImageAllocationSize();
  memset(buffer_.get(), 0, frame_size);
  frame_painter()->PaintFrame(timestamp_to_paint, buffer_.get());
  base::TimeTicks now = base::TimeTicks::Now();
  client()->OnIncomingCapturedData(buffer_.get(), frame_size,
                                   device_state()->format, 0 /* rotation */,
                                   now, CalculateTimeSinceFirstInvocation(now));
}

ClientBufferFrameDeliverer::ClientBufferFrameDeliverer(
    std::unique_ptr<PacmanFramePainter> frame_painter)
    : FrameDeliverer(std::move(frame_painter)) {}

ClientBufferFrameDeliverer::~ClientBufferFrameDeliverer() = default;

void ClientBufferFrameDeliverer::PaintAndDeliverNextFrame(
    base::TimeDelta timestamp_to_paint) {
  if (!client())
    return;

  const int arbitrary_frame_feedback_id = 0;
  VideoCaptureDevice::Client::Buffer capture_buffer;
  const auto reserve_result = client()->ReserveOutputBuffer(
      device_state()->format.frame_size, device_state()->format.pixel_format,
      arbitrary_frame_feedback_id, &capture_buffer);
  if (reserve_result != VideoCaptureDevice::Client::ReserveResult::kSucceeded) {
    client()->OnFrameDropped(
        ConvertReservationFailureToFrameDropReason(reserve_result));
    return;
  }
  auto buffer_access =
      capture_buffer.handle_provider->GetHandleForInProcessAccess();
  DCHECK(buffer_access->data()) << "Buffer has NO backing memory";

  uint8_t* data_ptr = buffer_access->data();
  memset(data_ptr, 0, buffer_access->mapped_size());
  frame_painter()->PaintFrame(timestamp_to_paint, data_ptr);

  base::TimeTicks now = base::TimeTicks::Now();
  client()->OnIncomingCapturedBuffer(std::move(capture_buffer),
                                     device_state()->format, now,
                                     CalculateTimeSinceFirstInvocation(now));
}

JpegEncodingFrameDeliverer::JpegEncodingFrameDeliverer(
    std::unique_ptr<PacmanFramePainter> frame_painter)
    : FrameDeliverer(std::move(frame_painter)) {}

JpegEncodingFrameDeliverer::~JpegEncodingFrameDeliverer() = default;

void JpegEncodingFrameDeliverer::PaintAndDeliverNextFrame(
    base::TimeDelta timestamp_to_paint) {
  if (!client())
    return;

  auto required_sk_n32_buffer_size = VideoFrame::AllocationSize(
      PIXEL_FORMAT_ARGB, device_state()->format.frame_size);
  sk_n32_buffer_.resize(required_sk_n32_buffer_size);
  memset(&sk_n32_buffer_[0], 0, required_sk_n32_buffer_size);

  frame_painter()->PaintFrame(timestamp_to_paint, &sk_n32_buffer_[0]);

  static const int kQuality = 75;
  SkImageInfo info = SkImageInfo::MakeN32(
      device_state()->format.frame_size.width(),
      device_state()->format.frame_size.height(), kOpaque_SkAlphaType);
  SkPixmap src(info, &sk_n32_buffer_[0],
               VideoFrame::RowBytes(0 /* plane */, PIXEL_FORMAT_ARGB,
                                    device_state()->format.frame_size.width()));
  bool success = gfx::JPEGCodec::Encode(src, kQuality, &jpeg_buffer_);
  if (!success) {
    DLOG(ERROR) << "Jpeg encoding failed";
    return;
  }

  const size_t frame_size = jpeg_buffer_.size();
  base::TimeTicks now = base::TimeTicks::Now();
  client()->OnIncomingCapturedData(&jpeg_buffer_[0], frame_size,
                                   device_state()->format, 0 /* rotation */,
                                   now, CalculateTimeSinceFirstInvocation(now));
}

void FakeVideoCaptureDevice::BeepAndScheduleNextCapture(
    base::TimeTicks expected_execution_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const base::TimeDelta beep_interval =
      base::TimeDelta::FromMilliseconds(kBeepInterval);
  const base::TimeDelta frame_interval =
      base::TimeDelta::FromMicroseconds(1e6 / device_state_->format.frame_rate);
  beep_time_ += frame_interval;
  elapsed_time_ += frame_interval;

  // Generate a synchronized beep twice per second.
  if (beep_time_ >= beep_interval) {
    FakeAudioInputStream::BeepOnce();
    beep_time_ -= beep_interval;
  }

  // Reschedule next CaptureTask.
  const base::TimeTicks current_time = base::TimeTicks::Now();
  // Don't accumulate any debt if we are lagging behind - just post the next
  // frame immediately and continue as normal.
  const base::TimeTicks next_execution_time =
      std::max(current_time, expected_execution_time + frame_interval);
  const base::TimeDelta delay = next_execution_time - current_time;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeVideoCaptureDevice::OnNextFrameDue,
                 weak_factory_.GetWeakPtr(), next_execution_time,
                 current_session_id_),
      delay);
}

void FakeVideoCaptureDevice::OnNextFrameDue(
    base::TimeTicks expected_execution_time,
    int session_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (session_id != current_session_id_)
    return;

  frame_deliverer_->PaintAndDeliverNextFrame(elapsed_time_);
  BeepAndScheduleNextCapture(expected_execution_time);
}

}  // namespace media
