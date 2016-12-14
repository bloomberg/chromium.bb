// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/arc_gpu_video_decode_accelerator.h"

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_math.h"
#include "base/run_loop.h"
#include "media/base/video_frame.h"
#include "media/gpu/gpu_video_decode_accelerator_factory.h"

namespace chromeos {
namespace arc {

namespace {

// An arbitrary chosen limit of the number of buffers. The number of
// buffers used is requested from the untrusted client side.
const size_t kMaxBufferCount = 128;

// Maximum number of concurrent ARC video clients.
// Currently we have no way to know the resources are not enough to create more
// VDA. Arbitrarily chosen a reasonable constant as the limit.
const int kMaxConcurrentClients = 8;

}  // anonymous namespace

int ArcGpuVideoDecodeAccelerator::client_count_ = 0;

ArcGpuVideoDecodeAccelerator::InputRecord::InputRecord(
    int32_t bitstream_buffer_id,
    uint32_t buffer_index,
    int64_t timestamp)
    : bitstream_buffer_id(bitstream_buffer_id),
      buffer_index(buffer_index),
      timestamp(timestamp) {}

ArcGpuVideoDecodeAccelerator::InputBufferInfo::InputBufferInfo() = default;

ArcGpuVideoDecodeAccelerator::InputBufferInfo::InputBufferInfo(
    InputBufferInfo&& other) = default;

ArcGpuVideoDecodeAccelerator::InputBufferInfo::~InputBufferInfo() = default;

ArcGpuVideoDecodeAccelerator::OutputBufferInfo::OutputBufferInfo() = default;

ArcGpuVideoDecodeAccelerator::OutputBufferInfo::OutputBufferInfo(
    OutputBufferInfo&& other) = default;

ArcGpuVideoDecodeAccelerator::OutputBufferInfo::~OutputBufferInfo() = default;

ArcGpuVideoDecodeAccelerator::ArcGpuVideoDecodeAccelerator(
    const gpu::GpuPreferences& gpu_preferences)
    : arc_client_(nullptr),
      next_bitstream_buffer_id_(0),
      output_pixel_format_(media::PIXEL_FORMAT_UNKNOWN),
      output_buffer_size_(0),
      requested_num_of_output_buffers_(0),
      gpu_preferences_(gpu_preferences) {}

ArcGpuVideoDecodeAccelerator::~ArcGpuVideoDecodeAccelerator() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (vda_) {
    client_count_--;
  }
}

ArcVideoAccelerator::Result ArcGpuVideoDecodeAccelerator::Initialize(
    const Config& config,
    ArcVideoAccelerator::Client* client) {
  auto result = InitializeTask(config, client);
  // Report initialization status to UMA.
  UMA_HISTOGRAM_ENUMERATION(
      "Media.ArcGpuVideoDecodeAccelerator.InitializeResult", result,
      RESULT_MAX);
  return result;
}

ArcVideoAccelerator::Result ArcGpuVideoDecodeAccelerator::InitializeTask(
    const Config& config,
    ArcVideoAccelerator::Client* client) {
  DVLOG(5) << "Initialize(device=" << config.device_type
           << ", input_pixel_format=" << config.input_pixel_format
           << ", num_input_buffers=" << config.num_input_buffers << ")";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (config.device_type != Config::DEVICE_DECODER)
    return INVALID_ARGUMENT;
  DCHECK(client);

  if (arc_client_) {
    DLOG(ERROR) << "Re-Initialize() is not allowed";
    return ILLEGAL_STATE;
  }

  if (client_count_ >= kMaxConcurrentClients) {
    LOG(WARNING) << "Reject to Initialize() due to too many clients: "
                 << client_count_;
    return INSUFFICIENT_RESOURCES;
  }

  arc_client_ = client;

  if (config.num_input_buffers > kMaxBufferCount) {
    DLOG(ERROR) << "Request too many buffers: " << config.num_input_buffers;
    return INVALID_ARGUMENT;
  }
  input_buffer_info_.resize(config.num_input_buffers);

  media::VideoDecodeAccelerator::Config vda_config;
  switch (config.input_pixel_format) {
    case HAL_PIXEL_FORMAT_H264:
      vda_config.profile = media::H264PROFILE_MAIN;
      break;
    case HAL_PIXEL_FORMAT_VP8:
      vda_config.profile = media::VP8PROFILE_ANY;
      break;
    case HAL_PIXEL_FORMAT_VP9:
      vda_config.profile = media::VP9PROFILE_PROFILE0;
      break;
    default:
      DLOG(ERROR) << "Unsupported input format: " << config.input_pixel_format;
      return INVALID_ARGUMENT;
  }
  vda_config.output_mode =
      media::VideoDecodeAccelerator::Config::OutputMode::IMPORT;

  auto vda_factory = media::GpuVideoDecodeAcceleratorFactory::CreateWithNoGL();
  vda_ = vda_factory->CreateVDA(
      this, vda_config, gpu::GpuDriverBugWorkarounds(), gpu_preferences_);
  if (!vda_) {
    DLOG(ERROR) << "Failed to create VDA.";
    return PLATFORM_FAILURE;
  }

  client_count_++;
  DVLOG(5) << "Number of concurrent ArcVideoAccelerator clients: "
           << client_count_;

  return SUCCESS;
}

void ArcGpuVideoDecodeAccelerator::SetNumberOfOutputBuffers(size_t number) {
  DVLOG(5) << "SetNumberOfOutputBuffers(" << number << ")";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!vda_) {
    DLOG(ERROR) << "VDA not initialized";
    return;
  }

  if (number > kMaxBufferCount) {
    DLOG(ERROR) << "Too many buffers: " << number;
    arc_client_->OnError(INVALID_ARGUMENT);
    return;
  }

  std::vector<media::PictureBuffer> buffers;
  for (size_t id = 0; id < number; ++id) {
    // TODO(owenlin): Make sure the |coded_size| is what we want.
    buffers.push_back(
        media::PictureBuffer(base::checked_cast<int32_t>(id), coded_size_));
  }
  vda_->AssignPictureBuffers(buffers);

  buffers_pending_import_.clear();
  buffers_pending_import_.resize(number);
}

void ArcGpuVideoDecodeAccelerator::BindSharedMemory(PortType port,
                                                    uint32_t index,
                                                    base::ScopedFD ashmem_fd,
                                                    off_t offset,
                                                    size_t length) {
  DVLOG(5) << "ArcGVDA::BindSharedMemory, offset: " << offset
           << ", length: " << length;
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!vda_) {
    DLOG(ERROR) << "VDA not initialized";
    return;
  }

  if (port != PORT_INPUT) {
    DLOG(ERROR) << "SharedBuffer is only supported for input";
    arc_client_->OnError(INVALID_ARGUMENT);
    return;
  }
  if (!ValidatePortAndIndex(port, index)) {
    arc_client_->OnError(INVALID_ARGUMENT);
    return;
  }
  InputBufferInfo* input_info = &input_buffer_info_[index];
  input_info->handle = std::move(ashmem_fd);
  input_info->offset = offset;
  input_info->length = length;
}

bool ArcGpuVideoDecodeAccelerator::VerifyDmabuf(
    const base::ScopedFD& dmabuf_fd,
    const std::vector<::arc::ArcVideoAcceleratorDmabufPlane>& dmabuf_planes)
    const {
  size_t num_planes = media::VideoFrame::NumPlanes(output_pixel_format_);
  if (dmabuf_planes.size() != num_planes) {
    DLOG(ERROR) << "Invalid number of dmabuf planes passed: "
                << dmabuf_planes.size() << ", expected: " << num_planes;
    return false;
  }

  off_t size = lseek(dmabuf_fd.get(), 0, SEEK_END);
  lseek(dmabuf_fd.get(), 0, SEEK_SET);
  if (size < 0) {
    DPLOG(ERROR) << "fail to find the size of dmabuf";
    return false;
  }

  size_t i = 0;
  for (const auto& plane : dmabuf_planes) {
    DVLOG(4) << "Plane " << i << ", offset: " << plane.offset
             << ", stride: " << plane.stride;

    size_t rows =
        media::VideoFrame::Rows(i, output_pixel_format_, coded_size_.height());
    base::CheckedNumeric<off_t> current_size(plane.offset);
    current_size += base::CheckMul(plane.stride, rows);

    if (!current_size.IsValid() || current_size.ValueOrDie() > size) {
      DLOG(ERROR) << "Invalid strides/offsets";
      return false;
    }

    ++i;
  }

  return true;
}

void ArcGpuVideoDecodeAccelerator::BindDmabuf(
    PortType port,
    uint32_t index,
    base::ScopedFD dmabuf_fd,
    const std::vector<::arc::ArcVideoAcceleratorDmabufPlane>& dmabuf_planes) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!vda_) {
    DLOG(ERROR) << "VDA not initialized";
    return;
  }

  if (port != PORT_OUTPUT) {
    DLOG(ERROR) << "Dmabuf is only supported for input";
    arc_client_->OnError(INVALID_ARGUMENT);
    return;
  }
  if (!ValidatePortAndIndex(port, index)) {
    arc_client_->OnError(INVALID_ARGUMENT);
    return;
  }
  if (!VerifyDmabuf(dmabuf_fd, dmabuf_planes)) {
    arc_client_->OnError(INVALID_ARGUMENT);
    return;
  }

  OutputBufferInfo& info = buffers_pending_import_[index];
  info.handle = std::move(dmabuf_fd);
  info.planes = dmabuf_planes;
}

void ArcGpuVideoDecodeAccelerator::UseBuffer(PortType port,
                                             uint32_t index,
                                             const BufferMetadata& metadata) {
  DVLOG(5) << "UseBuffer(port=" << port << ", index=" << index
           << ", metadata=(bytes_used=" << metadata.bytes_used
           << ", timestamp=" << metadata.timestamp << ")";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!vda_) {
    DLOG(ERROR) << "VDA not initialized";
    return;
  }
  if (!ValidatePortAndIndex(port, index)) {
    arc_client_->OnError(INVALID_ARGUMENT);
    return;
  }
  switch (port) {
    case PORT_INPUT: {
      InputBufferInfo* input_info = &input_buffer_info_[index];
      int32_t bitstream_buffer_id = next_bitstream_buffer_id_;
      // Mask against 30 bits, to avoid (undefined) wraparound on signed
      // integer.
      next_bitstream_buffer_id_ = (next_bitstream_buffer_id_ + 1) & 0x3FFFFFFF;
      int dup_fd = HANDLE_EINTR(dup(input_info->handle.get()));
      if (dup_fd < 0) {
        DLOG(ERROR) << "dup() failed.";
        arc_client_->OnError(PLATFORM_FAILURE);
        return;
      }
      CreateInputRecord(bitstream_buffer_id, index, metadata.timestamp);
      vda_->Decode(media::BitstreamBuffer(
          bitstream_buffer_id, base::SharedMemoryHandle(dup_fd, true),
          metadata.bytes_used, input_info->offset));
      break;
    }
    case PORT_OUTPUT: {
      // is_valid() is true for the first time the buffer is passed to the VDA.
      // In that case, VDA needs to import the buffer first.
      OutputBufferInfo& info = buffers_pending_import_[index];
      if (info.handle.is_valid()) {
        gfx::GpuMemoryBufferHandle handle;
#if defined(USE_OZONE)
        handle.native_pixmap_handle.fds.emplace_back(
            base::FileDescriptor(info.handle.release(), true));
        for (const auto& plane : info.planes) {
          handle.native_pixmap_handle.planes.emplace_back(plane.stride,
                                                          plane.offset, 0, 0);
        }
#endif
        vda_->ImportBufferForPicture(index, handle);
      } else {
        vda_->ReusePictureBuffer(index);
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

void ArcGpuVideoDecodeAccelerator::Reset() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!vda_) {
    DLOG(ERROR) << "VDA not initialized";
    return;
  }
  vda_->Reset();
}

void ArcGpuVideoDecodeAccelerator::Flush() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!vda_) {
    DLOG(ERROR) << "VDA not initialized";
    return;
  }
  vda_->Flush();
}

void ArcGpuVideoDecodeAccelerator::ProvidePictureBuffers(
    uint32_t requested_num_of_buffers,
    media::VideoPixelFormat output_pixel_format,
    uint32_t textures_per_buffer,
    const gfx::Size& dimensions,
    uint32_t texture_target) {
  DVLOG(5) << "ProvidePictureBuffers("
           << "requested_num_of_buffers=" << requested_num_of_buffers
           << ", dimensions=" << dimensions.ToString() << ")";
  DCHECK(thread_checker_.CalledOnValidThread());
  coded_size_ = dimensions;

  // By default, use an empty rect to indicate the visible rectangle is not
  // available.
  visible_rect_ = gfx::Rect();
  if ((output_pixel_format_ != media::PIXEL_FORMAT_UNKNOWN) &&
      (output_pixel_format_ != output_pixel_format)) {
    arc_client_->OnError(PLATFORM_FAILURE);
    return;
  }
  output_pixel_format_ = output_pixel_format;
  requested_num_of_output_buffers_ = requested_num_of_buffers;
  output_buffer_size_ =
      media::VideoFrame::AllocationSize(output_pixel_format_, coded_size_);

  NotifyOutputFormatChanged();
}

void ArcGpuVideoDecodeAccelerator::NotifyOutputFormatChanged() {
  VideoFormat video_format;
  switch (output_pixel_format_) {
    case media::PIXEL_FORMAT_I420:
    case media::PIXEL_FORMAT_YV12:
    case media::PIXEL_FORMAT_NV12:
    case media::PIXEL_FORMAT_NV21:
      // HAL_PIXEL_FORMAT_YCbCr_420_888 is the flexible pixel format in Android
      // which handles all 420 formats, with both orderings of chroma (CbCr and
      // CrCb) as well as planar and semi-planar layouts.
      video_format.pixel_format = HAL_PIXEL_FORMAT_YCbCr_420_888;
      break;
    case media::PIXEL_FORMAT_ARGB:
      video_format.pixel_format = HAL_PIXEL_FORMAT_BGRA_8888;
      break;
    default:
      DLOG(ERROR) << "Format not supported: " << output_pixel_format_;
      arc_client_->OnError(PLATFORM_FAILURE);
      return;
  }
  video_format.buffer_size = output_buffer_size_;
  video_format.min_num_buffers = requested_num_of_output_buffers_;
  video_format.coded_width = coded_size_.width();
  video_format.coded_height = coded_size_.height();
  video_format.crop_top = visible_rect_.y();
  video_format.crop_left = visible_rect_.x();
  video_format.crop_width = visible_rect_.width();
  video_format.crop_height = visible_rect_.height();
  arc_client_->OnOutputFormatChanged(video_format);
}

void ArcGpuVideoDecodeAccelerator::DismissPictureBuffer(
    int32_t picture_buffer) {
  // no-op
}

void ArcGpuVideoDecodeAccelerator::PictureReady(const media::Picture& picture) {
  DVLOG(5) << "PictureReady(picture_buffer_id=" << picture.picture_buffer_id()
           << ", bitstream_buffer_id=" << picture.bitstream_buffer_id();
  DCHECK(thread_checker_.CalledOnValidThread());

  // Handle visible size change.
  if (visible_rect_ != picture.visible_rect()) {
    DVLOG(5) << "visible size changed: " << picture.visible_rect().ToString();
    visible_rect_ = picture.visible_rect();
    NotifyOutputFormatChanged();
  }

  InputRecord* input_record = FindInputRecord(picture.bitstream_buffer_id());
  if (input_record == nullptr) {
    DLOG(ERROR) << "Cannot find for bitstream buffer id: "
                << picture.bitstream_buffer_id();
    arc_client_->OnError(PLATFORM_FAILURE);
    return;
  }

  BufferMetadata metadata;
  metadata.timestamp = input_record->timestamp;
  metadata.bytes_used = output_buffer_size_;
  arc_client_->OnBufferDone(PORT_OUTPUT, picture.picture_buffer_id(), metadata);
}

void ArcGpuVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer(
    int32_t bitstream_buffer_id) {
  DVLOG(5) << "NotifyEndOfBitstreamBuffer(" << bitstream_buffer_id << ")";
  DCHECK(thread_checker_.CalledOnValidThread());
  InputRecord* input_record = FindInputRecord(bitstream_buffer_id);
  if (input_record == nullptr) {
    arc_client_->OnError(PLATFORM_FAILURE);
    return;
  }
  arc_client_->OnBufferDone(PORT_INPUT, input_record->buffer_index,
                            BufferMetadata());
}

void ArcGpuVideoDecodeAccelerator::NotifyFlushDone() {
  DCHECK(thread_checker_.CalledOnValidThread());
  arc_client_->OnFlushDone();
}

void ArcGpuVideoDecodeAccelerator::NotifyResetDone() {
  DCHECK(thread_checker_.CalledOnValidThread());
  arc_client_->OnResetDone();
}

static ArcVideoAccelerator::Result ConvertErrorCode(
    media::VideoDecodeAccelerator::Error error) {
  switch (error) {
    case media::VideoDecodeAccelerator::ILLEGAL_STATE:
      return ArcVideoAccelerator::ILLEGAL_STATE;
    case media::VideoDecodeAccelerator::INVALID_ARGUMENT:
      return ArcVideoAccelerator::INVALID_ARGUMENT;
    case media::VideoDecodeAccelerator::UNREADABLE_INPUT:
      return ArcVideoAccelerator::UNREADABLE_INPUT;
    case media::VideoDecodeAccelerator::PLATFORM_FAILURE:
      return ArcVideoAccelerator::PLATFORM_FAILURE;
    default:
      DLOG(ERROR) << "Unknown error: " << error;
      return ArcVideoAccelerator::PLATFORM_FAILURE;
  }
}

void ArcGpuVideoDecodeAccelerator::NotifyError(
    media::VideoDecodeAccelerator::Error error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DLOG(ERROR) << "Error notified: " << error;
  arc_client_->OnError(ConvertErrorCode(error));
}

void ArcGpuVideoDecodeAccelerator::CreateInputRecord(
    int32_t bitstream_buffer_id,
    uint32_t buffer_index,
    int64_t timestamp) {
  input_records_.push_front(
      InputRecord(bitstream_buffer_id, buffer_index, timestamp));

  // The same value copied from media::GpuVideoDecoder. The input record is
  // needed when the input buffer or the corresponding output buffer are
  // returned from VDA. However there is no guarantee how much buffers will be
  // kept in the VDA. We kept the last |kMaxNumberOfInputRecords| in
  // |input_records_| and drop the others.
  const size_t kMaxNumberOfInputRecords = 128;
  if (input_records_.size() > kMaxNumberOfInputRecords)
    input_records_.pop_back();
}

ArcGpuVideoDecodeAccelerator::InputRecord*
ArcGpuVideoDecodeAccelerator::FindInputRecord(int32_t bitstream_buffer_id) {
  for (auto& record : input_records_) {
    if (record.bitstream_buffer_id == bitstream_buffer_id)
      return &record;
  }
  return nullptr;
}

bool ArcGpuVideoDecodeAccelerator::ValidatePortAndIndex(PortType port,
                                                        uint32_t index) const {
  switch (port) {
    case PORT_INPUT:
      if (index >= input_buffer_info_.size()) {
        DLOG(ERROR) << "Invalid index: " << index;
        return false;
      }
      return true;
    case PORT_OUTPUT:
      if (index >= buffers_pending_import_.size()) {
        DLOG(ERROR) << "Invalid index: " << index;
        return false;
      }
      return true;
    default:
      DLOG(ERROR) << "Invalid port: " << port;
      return false;
  }
}

}  // namespace arc
}  // namespace chromeos
