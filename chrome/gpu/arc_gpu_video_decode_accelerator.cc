// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/arc_gpu_video_decode_accelerator.h"

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "content/public/gpu/gpu_video_decode_accelerator_factory.h"
#include "media/base/video_frame.h"

namespace chromeos {
namespace arc {

ArcGpuVideoDecodeAccelerator::InputRecord::InputRecord(
    int32_t bitstream_buffer_id,
    uint32_t buffer_index,
    int64_t timestamp)
    : bitstream_buffer_id(bitstream_buffer_id),
      buffer_index(buffer_index),
      timestamp(timestamp) {}

ArcGpuVideoDecodeAccelerator::InputBufferInfo::InputBufferInfo()
    : offset(0), length(0) {}

ArcGpuVideoDecodeAccelerator::InputBufferInfo::InputBufferInfo(
    InputBufferInfo&& other)
    : handle(std::move(other.handle)),
      offset(other.offset),
      length(other.length) {}

ArcGpuVideoDecodeAccelerator::InputBufferInfo::~InputBufferInfo() {}

ArcGpuVideoDecodeAccelerator::ArcGpuVideoDecodeAccelerator()
    : pending_eos_output_buffer_(false),
      arc_client_(nullptr),
      next_bitstream_buffer_id_(0),
      output_buffer_size_(0) {}

ArcGpuVideoDecodeAccelerator::~ArcGpuVideoDecodeAccelerator() {}

namespace {

// An arbitrary chosen limit of the number of buffers. The number of
// buffers used is requested from the untrusted client side.
const size_t kMaxBufferCount = 128;

}  // anonymous namespace

bool ArcGpuVideoDecodeAccelerator::Initialize(
    const Config& config,
    ArcVideoAccelerator::Client* client) {
  DVLOG(5) << "Initialize(device=" << config.device_type
           << ", input_pixel_format=" << config.input_pixel_format
           << ", num_input_buffers=" << config.num_input_buffers << ")";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (config.device_type != Config::DEVICE_DECODER)
    return false;
  DCHECK(client);

  if (arc_client_) {
    DLOG(ERROR) << "Re-Initialize() is not allowed";
    return false;
  }

  arc_client_ = client;

  if (config.num_input_buffers > kMaxBufferCount) {
    DLOG(ERROR) << "Request too many buffers: " << config.num_input_buffers;
    return false;
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
    default:
      DLOG(ERROR) << "Unsupported input format: " << config.input_pixel_format;
      return false;
  }
  vda_config.output_mode =
      media::VideoDecodeAccelerator::Config::OutputMode::IMPORT;

  std::unique_ptr<content::GpuVideoDecodeAcceleratorFactory> vda_factory =
      content::GpuVideoDecodeAcceleratorFactory::CreateWithNoGL();
  vda_ = vda_factory->CreateVDA(this, vda_config);
  if (!vda_) {
    DLOG(ERROR) << "Failed to create VDA.";
    return false;
  }
  return true;
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
    media::PictureBuffer::TextureIds texture_ids;
    texture_ids.push_back(0);

    // TODO(owenlin): Make sure the |coded_size| is what we want.
    buffers.push_back(media::PictureBuffer(base::checked_cast<int32_t>(id),
                                           coded_size_, texture_ids));
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

void ArcGpuVideoDecodeAccelerator::BindDmabuf(PortType port,
                                              uint32_t index,
                                              base::ScopedFD dmabuf_fd) {
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
  buffers_pending_import_[index] = std::move(dmabuf_fd);
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
      if (metadata.flags & BUFFER_FLAG_EOS) {
        vda_->Flush();
      }
      break;
    }
    case PORT_OUTPUT: {
      SendEosIfNeededOrReusePicture(index);
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

void ArcGpuVideoDecodeAccelerator::ProvidePictureBuffers(
    uint32_t requested_num_of_buffers,
    uint32_t textures_per_buffer,
    const gfx::Size& dimensions,
    uint32_t texture_target) {
  DVLOG(5) << "ProvidePictureBuffers("
           << "requested_num_of_buffers=" << requested_num_of_buffers
           << ", dimensions=" << dimensions.ToString() << ")";
  DCHECK(thread_checker_.CalledOnValidThread());
  coded_size_ = dimensions;

  VideoFormat video_format;
  media::VideoPixelFormat output_format = vda_->GetOutputFormat();
  switch (output_format) {
    case media::PIXEL_FORMAT_I420:
    case media::PIXEL_FORMAT_YV12:
    case media::PIXEL_FORMAT_NV12:
    case media::PIXEL_FORMAT_NV21:
      // HAL_PIXEL_FORMAT_YCbCr_420_888 is the flexible pixel format in Android
      // which handles all 420 formats, with both orderings of chroma (CbCr and
      // CrCb) as well as planar and semi-planar layouts.
      video_format.pixel_format = HAL_PIXEL_FORMAT_YCbCr_420_888;
      break;
    default:
      DLOG(ERROR) << "Format not supported: " << output_format;
      arc_client_->OnError(PLATFORM_FAILURE);
      return;
  }
  video_format.buffer_size =
      media::VideoFrame::AllocationSize(output_format, coded_size_);
  output_buffer_size_ = video_format.buffer_size;
  video_format.min_num_buffers = requested_num_of_buffers;
  video_format.coded_width = dimensions.width();
  video_format.coded_height = dimensions.height();
  // TODO(owenlin): How to get visible size?
  video_format.crop_top = 0;
  video_format.crop_left = 0;
  video_format.crop_width = dimensions.width();
  video_format.crop_height = dimensions.height();
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

  // Empty buffer, returned in Flushing.
  if (picture.bitstream_buffer_id() == -1) {
    buffers_pending_eos_.push(picture.picture_buffer_id());
    return;
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
  pending_eos_output_buffer_ = true;
  while (!buffers_pending_eos_.empty()) {
    SendEosIfNeededOrReusePicture(buffers_pending_eos_.front());
    buffers_pending_eos_.pop();
  }
}

void ArcGpuVideoDecodeAccelerator::NotifyResetDone() {
  DCHECK(thread_checker_.CalledOnValidThread());
  arc_client_->OnResetDone();
}

static ArcVideoAccelerator::Error ConvertErrorCode(
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

void ArcGpuVideoDecodeAccelerator::SendEosIfNeededOrReusePicture(
    uint32_t index) {
  if (pending_eos_output_buffer_) {
    BufferMetadata metadata;
    metadata.flags = BUFFER_FLAG_EOS;
    arc_client_->OnBufferDone(PORT_OUTPUT, index, metadata);
    pending_eos_output_buffer_ = false;
  } else {
    if (buffers_pending_import_[index].is_valid()) {
      std::vector<gfx::GpuMemoryBufferHandle> buffers;
      buffers.push_back(gfx::GpuMemoryBufferHandle());
#if defined(USE_OZONE)
      buffers.back().native_pixmap_handle.fd =
          base::FileDescriptor(buffers_pending_import_[index].release(), true);
#endif
      vda_->ImportBufferForPicture(index, buffers);
    } else {
      vda_->ReusePictureBuffer(index);
    }
  }
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
                                                        uint32_t index) {
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
