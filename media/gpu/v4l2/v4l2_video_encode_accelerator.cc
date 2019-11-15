// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_video_encode_accelerator.h"

#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <numeric>
#include <utility>

#include "base/bind.h"
#include "base/bits.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/color_plane_layout.h"
#include "media/base/scopedfd_helper.h"
#include "media/base/unaligned_shared_memory.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_types.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/chromeos/image_processor_factory.h"
#include "media/gpu/gpu_video_encode_accelerator_helpers.h"
#include "media/gpu/linux/platform_video_frame_utils.h"
#include "media/gpu/macros.h"
#include "media/video/h264_level_limits.h"
#include "media/video/h264_parser.h"

#define NOTIFY_ERROR(x)                      \
  do {                                       \
    VLOGF(1) << "Setting error state:" << x; \
    SetErrorState(x);                        \
  } while (0)

#define IOCTL_OR_ERROR_RETURN_VALUE(type, arg, value, type_str) \
  do {                                                          \
    if (device_->Ioctl(type, arg) != 0) {                       \
      VPLOGF(1) << "ioctl() failed: " << type_str;              \
      NOTIFY_ERROR(kPlatformFailureError);                      \
      return value;                                             \
    }                                                           \
  } while (0)

#define IOCTL_OR_ERROR_RETURN(type, arg) \
  IOCTL_OR_ERROR_RETURN_VALUE(type, arg, ((void)0), #type)

#define IOCTL_OR_ERROR_RETURN_FALSE(type, arg) \
  IOCTL_OR_ERROR_RETURN_VALUE(type, arg, false, #type)

#define IOCTL_OR_LOG_ERROR(type, arg)              \
  do {                                             \
    if (device_->Ioctl(type, arg) != 0)            \
      VPLOGF(1) << "ioctl() failed: " << #type;    \
  } while (0)

namespace {
const uint8_t kH264StartCode[] = {0, 0, 0, 1};
const size_t kH264StartCodeSize = sizeof(kH264StartCode);

// Copy a H.264 NALU of size |src_size| (without start code), located at |src|,
// into a buffer starting at |dst| of size |dst_size|, prepending it with
// a H.264 start code (as long as both fit). After copying, update |dst| to
// point to the address immediately after the copied data, and update |dst_size|
// to contain remaining destination buffer size.
static void CopyNALUPrependingStartCode(const uint8_t* src,
                                        size_t src_size,
                                        uint8_t** dst,
                                        size_t* dst_size) {
  size_t size_to_copy = kH264StartCodeSize + src_size;
  if (size_to_copy > *dst_size) {
    VLOGF(1) << "Could not copy a NALU, not enough space in destination buffer";
    return;
  }

  memcpy(*dst, kH264StartCode, kH264StartCodeSize);
  memcpy(*dst + kH264StartCodeSize, src, src_size);

  *dst += size_to_copy;
  *dst_size -= size_to_copy;
}
}  // namespace

namespace media {

namespace {
// Convert VideoFrameLayout to ImageProcessor::PortConfig.
ImageProcessor::PortConfig VideoFrameLayoutToPortConfig(
    const VideoFrameLayout& layout,
    const gfx::Size& visible_size,
    const std::vector<VideoFrame::StorageType>& preferred_storage_types) {
  return ImageProcessor::PortConfig(
      Fourcc::FromVideoPixelFormat(layout.format(), !layout.is_multi_planar()),
      layout.coded_size(), layout.planes(), visible_size,
      preferred_storage_types);
}
}  // namespace

struct V4L2VideoEncodeAccelerator::BitstreamBufferRef {
  BitstreamBufferRef(int32_t id, std::unique_ptr<UnalignedSharedMemory> shm)
      : id(id), shm(std::move(shm)) {}
  const int32_t id;
  const std::unique_ptr<UnalignedSharedMemory> shm;
};

V4L2VideoEncodeAccelerator::InputRecord::InputRecord() = default;

V4L2VideoEncodeAccelerator::InputRecord::InputRecord(const InputRecord&) =
    default;

V4L2VideoEncodeAccelerator::InputRecord::~InputRecord() = default;

V4L2VideoEncodeAccelerator::InputFrameInfo::InputFrameInfo()
    : InputFrameInfo(nullptr, false) {}

V4L2VideoEncodeAccelerator::InputFrameInfo::InputFrameInfo(
    scoped_refptr<VideoFrame> frame,
    bool force_keyframe)
    : frame(frame), force_keyframe(force_keyframe) {}

V4L2VideoEncodeAccelerator::InputFrameInfo::InputFrameInfo(
    scoped_refptr<VideoFrame> frame,
    bool force_keyframe,
    size_t index)
    : frame(std::move(frame)),
      force_keyframe(force_keyframe),
      ip_output_buffer_index(index) {}

V4L2VideoEncodeAccelerator::InputFrameInfo::InputFrameInfo(
    const InputFrameInfo&) = default;

V4L2VideoEncodeAccelerator::InputFrameInfo::~InputFrameInfo() {}

V4L2VideoEncodeAccelerator::V4L2VideoEncodeAccelerator(
    const scoped_refptr<V4L2Device>& device)
    : child_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      native_input_mode_(false),
      output_buffer_byte_size_(0),
      output_format_fourcc_(0),
      encoder_state_(kUninitialized),
      device_(device),
      input_memory_type_(V4L2_MEMORY_USERPTR),
      is_flush_supported_(false),
      encoder_thread_("V4L2EncoderThread"),
      device_poll_thread_("V4L2EncoderDevicePollThread"),
      weak_this_ptr_factory_(this) {
  weak_this_ = weak_this_ptr_factory_.GetWeakPtr();
}

V4L2VideoEncodeAccelerator::~V4L2VideoEncodeAccelerator() {
  DCHECK(!encoder_thread_.IsRunning());
  DCHECK(!device_poll_thread_.IsRunning());
  VLOGF(2);
}

bool V4L2VideoEncodeAccelerator::Initialize(const Config& config,
                                            Client* client) {
  TRACE_EVENT0("media,gpu", "V4L2VEA::Initialize");
  VLOGF(2) << ": " << config.AsHumanReadableString();

  visible_size_ = config.input_visible_size;

  client_ptr_factory_.reset(new base::WeakPtrFactory<Client>(client));
  client_ = client_ptr_factory_->GetWeakPtr();

  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(encoder_state_, kUninitialized);

  output_format_fourcc_ =
      V4L2Device::VideoCodecProfileToV4L2PixFmt(config.output_profile, false);
  if (!output_format_fourcc_) {
    VLOGF(1) << "invalid output_profile="
             << GetProfileName(config.output_profile);
    return false;
  }

  if (!device_->Open(V4L2Device::Type::kEncoder, output_format_fourcc_)) {
    VLOGF(1) << "Failed to open device for profile="
             << GetProfileName(config.output_profile)
             << ", fourcc=" << FourccToString(output_format_fourcc_);
    return false;
  }

  // Ask if V4L2_ENC_CMD_STOP (Flush) is supported.
  struct v4l2_encoder_cmd cmd = {};
  cmd.cmd = V4L2_ENC_CMD_STOP;
  is_flush_supported_ = (device_->Ioctl(VIDIOC_TRY_ENCODER_CMD, &cmd) == 0);
  if (!is_flush_supported_)
    VLOGF(2) << "V4L2_ENC_CMD_STOP is not supported.";

  struct v4l2_capability caps {};
  const __u32 kCapsRequired = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QUERYCAP, &caps);
  if ((caps.capabilities & kCapsRequired) != kCapsRequired) {
    VLOGF(1) << "caps check failed: 0x" << std::hex << caps.capabilities;
    return false;
  }

  if (!encoder_thread_.Start()) {
    VLOGF(1) << "encoder thread failed to start";
    return false;
  }

  bool result = false;
  base::WaitableEvent done;
  encoder_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2VideoEncodeAccelerator::InitializeTask,
                                weak_this_, config, &result, &done));
  done.Wait();
  return result;
}

void V4L2VideoEncodeAccelerator::InitializeTask(const Config& config,
                                                bool* result,
                                                base::WaitableEvent* done) {
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());

  // Signal the event when leaving the method.
  base::ScopedClosureRunner signal_event(
      base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(done)));
  *result = false;

  native_input_mode_ =
      config.storage_type.value_or(Config::StorageType::kShmem) ==
      Config::StorageType::kDmabuf;

  input_queue_ = device_->GetQueue(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
  output_queue_ = device_->GetQueue(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
  if (!input_queue_ || !output_queue_) {
    VLOGF(1) << "Failed to get V4L2Queue.";
    NOTIFY_ERROR(kPlatformFailureError);
    return;
  }

  if (!SetFormats(config.input_format, config.output_profile)) {
    VLOGF(1) << "Failed setting up formats";
    return;
  }

  if (config.input_format != device_input_layout_->format()) {
    VLOGF(2) << "Input format: " << config.input_format << " is not supported "
             << "by the HW. Will try to convert to "
             << device_input_layout_->format();

    // TODO(hiroh): Decide the appropriate planar in some way.
    auto input_layout = VideoFrameLayout::CreateMultiPlanar(
        config.input_format, visible_size_,
        std::vector<ColorPlaneLayout>(
            VideoFrame::NumPlanes(config.input_format)));
    if (!input_layout) {
      VLOGF(1) << "Invalid image processor input layout";
      return;
    }

    if (!CreateImageProcessor(*input_layout, *device_input_layout_,
                              visible_size_)) {
      VLOGF(1) << "Failed to create image processor";
      return;
    }
  }

  if (!InitInputMemoryType(config))
    return;
  if (!InitControls(config))
    return;
  if (!CreateOutputBuffers())
    return;

  encoder_state_ = kInitialized;
  RequestEncodingParametersChangeTask(
      config.initial_bitrate, config.initial_framerate.value_or(
                                  VideoEncodeAccelerator::kDefaultFramerate));
  child_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::RequireBitstreamBuffers, client_,
                                kInputBufferCount,
                                image_processor_.get()
                                    ? image_processor_->input_config().size
                                    : input_allocated_size_,
                                output_buffer_byte_size_));

  // Finish initialization.
  *result = true;
}

bool V4L2VideoEncodeAccelerator::CreateImageProcessor(
    const VideoFrameLayout& input_layout,
    const VideoFrameLayout& output_layout,
    const gfx::Size& visible_size) {
  VLOGF(2);
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_NE(input_layout.format(), output_layout.format());

  // Convert from |config.input_format| to |device_input_layout_->format()|,
  // keeping the size at |visible_size| and requiring the output buffers to
  // be of at least |device_input_layout_->coded_size()|.
  // |input_storage_type| can be STORAGE_SHMEM and STORAGE_MOJO_SHARED_BUFFER.
  // However, it doesn't matter VideoFrame::STORAGE_OWNED_MEMORY is specified
  // for |input_storage_type| here, as long as VideoFrame on Process()'s data
  // can be accessed by VideoFrame::data().
  image_processor_ = ImageProcessorFactory::Create(
      VideoFrameLayoutToPortConfig(input_layout, visible_size,
                                   {VideoFrame::STORAGE_OWNED_MEMORY}),
      VideoFrameLayoutToPortConfig(
          output_layout, visible_size,
          {VideoFrame::STORAGE_DMABUFS, VideoFrame::STORAGE_OWNED_MEMORY}),
      // Try OutputMode::ALLOCATE first because we want v4l2IP chooses
      // ALLOCATE mode. For libyuvIP, it accepts only IMPORT.
      {ImageProcessor::OutputMode::ALLOCATE,
       ImageProcessor::OutputMode::IMPORT},
      kImageProcBufferCount,
      base::BindRepeating(&V4L2VideoEncodeAccelerator::ImageProcessorError,
                          weak_this_));
  if (!image_processor_) {
    VLOGF(1) << "Failed initializing image processor";
    return false;
  }

  // The output of image processor is the input of encoder. Output coded
  // width of processor must be the same as input coded width of encoder.
  // Output coded height of processor can be larger but not smaller than the
  // input coded height of encoder. For example, suppose input size of encoder
  // is 320x193. It is OK if the output of processor is 320x208.
  const auto& ip_output_size = image_processor_->output_config().size;
  if (ip_output_size.width() != output_layout.coded_size().width() ||
      ip_output_size.height() < output_layout.coded_size().height()) {
    VLOGF(1) << "Invalid image processor output coded size "
             << ip_output_size.ToString() << ", expected output coded size is "
             << output_layout.coded_size().ToString();
    return false;
  }

  // Initialize |free_image_processor_output_buffer_indices_|.
  free_image_processor_output_buffer_indices_.resize(kImageProcBufferCount);
  std::iota(free_image_processor_output_buffer_indices_.begin(),
            free_image_processor_output_buffer_indices_.end(), 0);
  return AllocateImageProcessorOutputBuffers(kImageProcBufferCount,
                                             visible_size);
}

bool V4L2VideoEncodeAccelerator::AllocateImageProcessorOutputBuffers(
    size_t count,
    const gfx::Size& visible_size) {
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(image_processor_);
  // Allocate VideoFrames for image processor output if its mode is IMPORT.
  if (image_processor_->output_mode() != ImageProcessor::OutputMode::IMPORT) {
    return true;
  }

  image_processor_output_buffers_.resize(count);
  const auto output_storage_type =
      image_processor_->output_config().storage_type();
  for (size_t i = 0; i < count; i++) {
    switch (output_storage_type) {
      case VideoFrame::STORAGE_OWNED_MEMORY:
        image_processor_output_buffers_[i] = VideoFrame::CreateFrameWithLayout(
            *device_input_layout_, gfx::Rect(visible_size), visible_size,
            base::TimeDelta(), true);
        if (!image_processor_output_buffers_[i]) {
          VLOG(1) << "Failed to create VideoFrame";
          return false;
        }
        break;
      // TODO(crbug.com/910590): Support VideoFrame::STORAGE_DMABUFS.
      default:
        VLOGF(1) << "Unsupported output storage type of image processor: "
                 << output_storage_type;
        return false;
    }
  }
  return true;
}

bool V4L2VideoEncodeAccelerator::InitInputMemoryType(const Config& config) {
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());
  if (image_processor_) {
    const auto storage_type = image_processor_->output_config().storage_type();
    if (storage_type == VideoFrame::STORAGE_DMABUFS) {
      input_memory_type_ = V4L2_MEMORY_DMABUF;
    } else if (VideoFrame::IsStorageTypeMappable(storage_type)) {
      input_memory_type_ = V4L2_MEMORY_USERPTR;
    } else {
      VLOGF(1) << "Unsupported image processor's output StorageType: "
               << storage_type;
      return false;
    }
  } else {
    switch (config.storage_type.value_or(Config::StorageType::kShmem)) {
      case Config::StorageType::kShmem:
        input_memory_type_ = V4L2_MEMORY_USERPTR;
        break;
      case Config::StorageType::kDmabuf:
        input_memory_type_ = V4L2_MEMORY_DMABUF;
        break;
    }
  }
  return true;
}

void V4L2VideoEncodeAccelerator::ImageProcessorError() {
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());
  VLOGF(1) << "Image processor error";
  NOTIFY_ERROR(kPlatformFailureError);
}

void V4L2VideoEncodeAccelerator::Encode(scoped_refptr<VideoFrame> frame,
                                        bool force_keyframe) {
  DVLOGF(4) << "force_keyframe=" << force_keyframe;
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  encoder_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2VideoEncodeAccelerator::EncodeTask,
                                weak_this_, std::move(frame), force_keyframe));
}

void V4L2VideoEncodeAccelerator::UseOutputBitstreamBuffer(
    BitstreamBuffer buffer) {
  DVLOGF(4) << "id=" << buffer.id();
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  encoder_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2VideoEncodeAccelerator::UseOutputBitstreamBufferTask,
                     weak_this_, std::move(buffer)));
}

void V4L2VideoEncodeAccelerator::RequestEncodingParametersChange(
    uint32_t bitrate,
    uint32_t framerate) {
  VLOGF(2) << "bitrate=" << bitrate << ", framerate=" << framerate;
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  encoder_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &V4L2VideoEncodeAccelerator::RequestEncodingParametersChangeTask,
          weak_this_, bitrate, framerate));
}

void V4L2VideoEncodeAccelerator::Destroy() {
  VLOGF(2);
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  // We're destroying; cancel all callbacks.
  client_ptr_factory_.reset();

  // If the encoder thread is running, destroy using posted task.
  if (encoder_thread_.IsRunning()) {
    encoder_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&V4L2VideoEncodeAccelerator::DestroyTask, weak_this_));
    // DestroyTask() will put the encoder into kError state and cause all tasks
    // to no-op.
    encoder_thread_.Stop();
  } else {
    // Otherwise, call the destroy task directly.
    DestroyTask();
  }

  // If a flush is pending, notify client that it did not finish.
  if (flush_callback_)
    std::move(flush_callback_).Run(false);

  // Set to kError state just in case.
  encoder_state_ = kError;

  delete this;
}

void V4L2VideoEncodeAccelerator::Flush(FlushCallback flush_callback) {
  VLOGF(2);
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  encoder_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2VideoEncodeAccelerator::FlushTask,
                                weak_this_, std::move(flush_callback)));
}

void V4L2VideoEncodeAccelerator::FlushTask(FlushCallback flush_callback) {
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());

  if (flush_callback_ || encoder_state_ != kEncoding) {
    VLOGF(1) << "Flush failed: there is a pending flush, "
             << "or VideoEncodeAccelerator is not in kEncoding state";
    NOTIFY_ERROR(kIllegalStateError);
    child_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(flush_callback), false));
    return;
  }
  flush_callback_ = std::move(flush_callback);
  // Push a null frame to indicate Flush.
  EncodeTask(nullptr, false);
}

bool V4L2VideoEncodeAccelerator::IsFlushSupported() {
  return is_flush_supported_;
}

VideoEncodeAccelerator::SupportedProfiles
V4L2VideoEncodeAccelerator::GetSupportedProfiles() {
  scoped_refptr<V4L2Device> device = V4L2Device::Create();
  if (!device)
    return SupportedProfiles();

  return device->GetSupportedEncodeProfiles();
}

void V4L2VideoEncodeAccelerator::FrameProcessed(
    bool force_keyframe,
    base::TimeDelta timestamp,
    size_t output_buffer_index,
    scoped_refptr<VideoFrame> frame) {
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());
  DVLOGF(4) << "force_keyframe=" << force_keyframe
            << ", output_buffer_index=" << output_buffer_index;
  DCHECK_GE(output_buffer_index, 0u);

  encoder_input_queue_.emplace(std::move(frame), force_keyframe,
                               output_buffer_index);
  encoder_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2VideoEncodeAccelerator::Enqueue, weak_this_));
}

void V4L2VideoEncodeAccelerator::ReuseImageProcessorOutputBuffer(
    size_t output_buffer_index) {
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());
  DVLOGF(4) << "output_buffer_index=" << output_buffer_index;
  free_image_processor_output_buffer_indices_.push_back(output_buffer_index);
  InputImageProcessorTask();
}

size_t V4L2VideoEncodeAccelerator::CopyIntoOutputBuffer(
    const uint8_t* bitstream_data,
    size_t bitstream_size,
    std::unique_ptr<BitstreamBufferRef> buffer_ref) {
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());
  uint8_t* dst_ptr = static_cast<uint8_t*>(buffer_ref->shm->memory());
  size_t remaining_dst_size = buffer_ref->shm->size();

  if (!inject_sps_and_pps_) {
    if (bitstream_size <= remaining_dst_size) {
      memcpy(dst_ptr, bitstream_data, bitstream_size);
      return bitstream_size;
    } else {
      VLOGF(1) << "Output data did not fit in the BitstreamBuffer";
      return 0;
    }
  }

  // Cache the newest SPS and PPS found in the stream, and inject them before
  // each IDR found.
  H264Parser parser;
  parser.SetStream(bitstream_data, bitstream_size);
  H264NALU nalu;

  while (parser.AdvanceToNextNALU(&nalu) == H264Parser::kOk) {
    // nalu.size is always without the start code, regardless of the NALU type.
    if (nalu.size + kH264StartCodeSize > remaining_dst_size) {
      VLOGF(1) << "Output data did not fit in the BitstreamBuffer";
      break;
    }

    switch (nalu.nal_unit_type) {
      case H264NALU::kSPS:
        cached_sps_.resize(nalu.size);
        memcpy(cached_sps_.data(), nalu.data, nalu.size);
        cached_h264_header_size_ =
            cached_sps_.size() + cached_pps_.size() + 2 * kH264StartCodeSize;
        break;

      case H264NALU::kPPS:
        cached_pps_.resize(nalu.size);
        memcpy(cached_pps_.data(), nalu.data, nalu.size);
        cached_h264_header_size_ =
            cached_sps_.size() + cached_pps_.size() + 2 * kH264StartCodeSize;
        break;

      case H264NALU::kIDRSlice:
        // Only inject if we have both headers cached, and enough space for both
        // the headers and the NALU itself.
        if (cached_sps_.empty() || cached_pps_.empty() ||
            cached_h264_header_size_ + nalu.size + kH264StartCodeSize >
                remaining_dst_size) {
          VLOGF(1) << "Not enough space to inject a stream header before IDR";
          break;
        }

        CopyNALUPrependingStartCode(cached_sps_.data(), cached_sps_.size(),
                                    &dst_ptr, &remaining_dst_size);
        CopyNALUPrependingStartCode(cached_pps_.data(), cached_pps_.size(),
                                    &dst_ptr, &remaining_dst_size);
        VLOGF(2) << "Stream header injected before IDR";
        break;
    }

    CopyNALUPrependingStartCode(nalu.data, nalu.size, &dst_ptr,
                                &remaining_dst_size);
  }

  return buffer_ref->shm->size() - remaining_dst_size;
}

void V4L2VideoEncodeAccelerator::EncodeTask(scoped_refptr<VideoFrame> frame,
                                            bool force_keyframe) {
  DVLOGF(4) << "force_keyframe=" << force_keyframe;
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_NE(encoder_state_, kUninitialized);

  if (encoder_state_ == kError) {
    DVLOGF(1) << "early out: kError state";
    return;
  }

  if (frame &&
      !ReconfigureFormatIfNeeded(frame->format(), frame->coded_size())) {
    NOTIFY_ERROR(kInvalidArgumentError);
    encoder_state_ = kError;
    return;
  }

  // If a video frame to be encoded is fed, then call VIDIOC_REQBUFS if it has
  // not been called yet.
  if (frame && input_buffer_map_.empty() && !CreateInputBuffers())
    return;

  if (image_processor_) {
    image_processor_input_queue_.emplace(std::move(frame), force_keyframe);
    InputImageProcessorTask();
  } else {
    encoder_input_queue_.emplace(std::move(frame), force_keyframe);
    Enqueue();
  }
}

bool V4L2VideoEncodeAccelerator::ReconfigureFormatIfNeeded(
    VideoPixelFormat format,
    const gfx::Size& new_frame_size) {
  // We should apply the frame size change to ImageProcessor if there is.
  if (image_processor_) {
    // Stride is the same. There is no need of executing S_FMT again.
    if (image_processor_->input_config().size == new_frame_size) {
      return true;
    }

    VLOGF(2) << "Call S_FMT with a new size=" << new_frame_size.ToString()
             << ", the previous size ="
             << device_input_layout_->coded_size().ToString();
    if (!input_buffer_map_.empty()) {
      VLOGF(1) << "Input frame size is changed during encoding";
      NOTIFY_ERROR(kInvalidArgumentError);
      return false;
    }

    // TODO(hiroh): Decide the appropriate planar in some way.
    auto input_layout = VideoFrameLayout::CreateMultiPlanar(
        format, new_frame_size,
        std::vector<ColorPlaneLayout>(VideoFrame::NumPlanes(format)));
    if (!input_layout) {
      VLOGF(1) << "Invalid image processor input layout";
      return false;
    }

    if (!CreateImageProcessor(*input_layout, *device_input_layout_,
                              visible_size_)) {
      NOTIFY_ERROR(kPlatformFailureError);
      return false;
    }
    if (image_processor_->input_config().size.width() !=
        new_frame_size.width()) {
      NOTIFY_ERROR(kPlatformFailureError);
      return false;
    }

    return true;
  }

  // Here we should compare |device_input_layout_->coded_size()|. However, VEA
  // requests a client |input_allocated_size_|, which might be a larger size
  // than |device_input_layout_->coded_size()|. The size is larger if there is
  // an extra data in planes, that happens on MediaTek.
  // This comparison will work because VEAClient within Chrome gives the buffer
  // whose frame size as |input_allocated_size_|. VEAClient for ARC++ might give
  // a different frame size but |input_allocated_size_| is always the same as
  // |device_input_layout_->coded_size()|.
  if (new_frame_size != input_allocated_size_) {
    VLOGF(2) << "Call S_FMT with a new size=" << new_frame_size.ToString()
             << ", the previous size ="
             << device_input_layout_->coded_size().ToString()
             << " (the size requested to client="
             << input_allocated_size_.ToString();
    if (!input_buffer_map_.empty()) {
      VLOGF(1) << "Input frame size is changed during encoding";
      NOTIFY_ERROR(kInvalidArgumentError);
      return false;
    }
    if (!NegotiateInputFormat(device_input_layout_->format(), new_frame_size)) {
      NOTIFY_ERROR(kPlatformFailureError);
      return false;
    }
    if (device_input_layout_->coded_size().width() != new_frame_size.width()) {
      NOTIFY_ERROR(kPlatformFailureError);
      return false;
    }
  }

  return true;
}

void V4L2VideoEncodeAccelerator::InputImageProcessorTask() {
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());
  if (free_image_processor_output_buffer_indices_.empty())
    return;
  if (image_processor_input_queue_.empty())
    return;
  const size_t output_buffer_index =
      free_image_processor_output_buffer_indices_.back();
  free_image_processor_output_buffer_indices_.pop_back();

  InputFrameInfo frame_info = std::move(image_processor_input_queue_.front());
  image_processor_input_queue_.pop();
  auto frame = std::move(frame_info.frame);
  const bool force_keyframe = frame_info.force_keyframe;
  auto timestamp = frame->timestamp();
  if (image_processor_->output_mode() == ImageProcessor::OutputMode::IMPORT) {
    const auto& buf = image_processor_output_buffers_[output_buffer_index];
    auto output_frame = VideoFrame::WrapVideoFrame(
        buf, buf->format(), buf->visible_rect(), buf->natural_size());

    if (!image_processor_->Process(
            std::move(frame), std::move(output_frame),
            base::BindOnce(&V4L2VideoEncodeAccelerator::FrameProcessed,
                           weak_this_, force_keyframe, timestamp,
                           output_buffer_index))) {
      NOTIFY_ERROR(kPlatformFailureError);
    }
  } else {
    if (!image_processor_->Process(
            std::move(frame),
            base::BindOnce(&V4L2VideoEncodeAccelerator::FrameProcessed,
                           weak_this_, force_keyframe, timestamp))) {
      NOTIFY_ERROR(kPlatformFailureError);
    }
  }
}

void V4L2VideoEncodeAccelerator::UseOutputBitstreamBufferTask(
    BitstreamBuffer buffer) {
  DVLOGF(4) << "id=" << buffer.id();
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());

  if (buffer.size() < output_buffer_byte_size_) {
    NOTIFY_ERROR(kInvalidArgumentError);
    return;
  }
  auto shm = std::make_unique<UnalignedSharedMemory>(buffer.TakeRegion(),
                                                     buffer.size(), false);
  if (!shm->MapAt(buffer.offset(), buffer.size())) {
    NOTIFY_ERROR(kPlatformFailureError);
    return;
  }

  bitstream_buffer_pool_.push_back(
      std::make_unique<BitstreamBufferRef>(buffer.id(), std::move(shm)));
  PumpBitstreamBuffers();

  if (encoder_state_ == kInitialized) {
    if (!StartDevicePoll())
      return;
    encoder_state_ = kEncoding;
  }
}

void V4L2VideoEncodeAccelerator::DestroyTask() {
  VLOGF(2);

  // Make tasks posted to encoder_thread_.task_runner() after DestroyTask() not
  // be executed.
  weak_this_ptr_factory_.InvalidateWeakPtrs();

  // DestroyTask() should run regardless of encoder_state_.

  // Stop streaming and the device_poll_thread_.
  StopDevicePoll();

  // Set our state to kError, and early-out all tasks.
  encoder_state_ = kError;

  if (encoder_thread_.task_runner() &&
      encoder_thread_.task_runner()->BelongsToCurrentThread()) {
    DestroyInputBuffers();
    DestroyOutputBuffers();
    input_queue_ = nullptr;
    output_queue_ = nullptr;
    image_processor_ = nullptr;
  }
}

void V4L2VideoEncodeAccelerator::ServiceDeviceTask() {
  DVLOGF(3);
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_NE(encoder_state_, kUninitialized);
  DCHECK_NE(encoder_state_, kInitialized);

  if (encoder_state_ == kError) {
    DVLOGF(1) << "early out: kError state";
    return;
  }

  Dequeue();
  Enqueue();

  // Clear the interrupt fd.
  if (!device_->ClearDevicePollInterrupt())
    return;

  // Device can be polled as soon as either input or output buffers are queued.
  bool poll_device = (input_queue_->QueuedBuffersCount() +
                          output_queue_->QueuedBuffersCount() >
                      0);

  // ServiceDeviceTask() should only ever be scheduled from DevicePollTask(),
  // so either:
  // * device_poll_thread_ is running normally
  // * device_poll_thread_ scheduled us, but then a DestroyTask() shut it down,
  //   in which case we're in kError state, and we should have early-outed
  //   already.
  DCHECK(device_poll_thread_.task_runner());
  // Queue the DevicePollTask() now.
  // base::Unretained(this) is safe, because device_poll_thread_ is owned by
  // *this and stops before *this destruction.
  device_poll_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2VideoEncodeAccelerator::DevicePollTask,
                                base::Unretained(this), poll_device));

  DVLOGF(3) << encoder_input_queue_.size() << "] => DEVICE["
            << input_queue_->FreeBuffersCount() << "+"
            << input_queue_->QueuedBuffersCount() << "/"
            << input_buffer_map_.size() << "->"
            << output_queue_->FreeBuffersCount() << "+"
            << output_queue_->QueuedBuffersCount() << "/"
            << output_queue_->AllocatedBuffersCount() << "] => OUT["
            << bitstream_buffer_pool_.size() << "]";
}

void V4L2VideoEncodeAccelerator::Enqueue() {
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(input_queue_ && output_queue_);
  TRACE_EVENT0("media,gpu", "V4L2VEA::Enqueue");
  DVLOGF(4) << "free_input_buffers: " << input_queue_->FreeBuffersCount()
            << "input_queue: " << encoder_input_queue_.size();
  bool do_streamon = false;
  // Enqueue all the inputs we can.
  const size_t old_inputs_queued = input_queue_->QueuedBuffersCount();
  while (!encoder_input_queue_.empty() &&
         input_queue_->FreeBuffersCount() > 0) {
    // A null frame indicates a flush.
    if (encoder_input_queue_.front().frame == nullptr) {
      DVLOGF(3) << "All input frames needed to be flushed are enqueued.";
      encoder_input_queue_.pop();

      // If we are not streaming, the device is not running and there is no need
      // to call V4L2_ENC_CMD_STOP to request a flush. This also means there is
      // nothing left to process, so we can return flush success back to the
      // client.
      if (!input_queue_->IsStreaming()) {
        child_task_runner_->PostTask(
            FROM_HERE, base::BindOnce(std::move(flush_callback_), true));
        return;
      }
      struct v4l2_encoder_cmd cmd{};
      cmd.cmd = V4L2_ENC_CMD_STOP;
      if (device_->Ioctl(VIDIOC_ENCODER_CMD, &cmd) != 0) {
        VPLOGF(1) << "ioctl() failed: VIDIOC_ENCODER_CMD";
        NOTIFY_ERROR(kPlatformFailureError);
        child_task_runner_->PostTask(
            FROM_HERE, base::BindOnce(std::move(flush_callback_), false));
        return;
      }
      encoder_state_ = kFlushing;
      break;
    }
    if (!EnqueueInputRecord())
      return;
  }
  if (old_inputs_queued == 0 && input_queue_->QueuedBuffersCount() != 0) {
    // We just started up a previously empty queue.
    // Queue state changed; signal interrupt.
    if (!device_->SetDevicePollInterrupt())
      return;
    // Shall call VIDIOC_STREAMON if we haven't yet.
    do_streamon = !input_queue_->IsStreaming();
  }

  if (!input_queue_->IsStreaming() && !do_streamon) {
    // We don't have to enqueue any buffers in the output queue until we enqueue
    // buffers in the input queue. This enables to call S_FMT in Encode() on
    // the first frame.
    return;
  }

  // Enqueue all the outputs we can.
  const size_t old_outputs_queued = output_queue_->QueuedBuffersCount();
  while (output_queue_->FreeBuffersCount() > 0) {
    if (!EnqueueOutputRecord())
      return;
  }
  if (old_outputs_queued == 0 && output_queue_->QueuedBuffersCount() != 0) {
    // We just started up a previously empty queue.
    // Queue state changed; signal interrupt.
    if (!device_->SetDevicePollInterrupt())
      return;
  }

  // STREAMON in CAPTURE queue first and then OUTPUT queue.
  // This is a workaround of a tegra driver bug that STREAMON in CAPTURE queue
  // will never return (i.e. blocks |encoder_thread_| forever) if the STREAMON
  // in CAPTURE queue is called after STREAMON in OUTPUT queue.
  // Once nyan_kitty, which uses tegra driver, reaches EOL, crrev.com/c/1753982
  // should be reverted.
  if (do_streamon) {
    DCHECK(!output_queue_->IsStreaming() && !input_queue_->IsStreaming());
    // When VIDIOC_STREAMON can be executed in OUTPUT queue, it is fine to call
    // STREAMON in CAPTURE queue.
    output_queue_->Streamon();
    input_queue_->Streamon();
  }
}

void V4L2VideoEncodeAccelerator::Dequeue() {
  DVLOGF(4);
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());
  TRACE_EVENT0("media,gpu", "V4L2VEA::Dequeue");

  // Dequeue completed input (VIDEO_OUTPUT) buffers, and recycle to the free
  // list.
  while (input_queue_->QueuedBuffersCount() > 0) {
    DVLOGF(4) << "inputs queued: " << input_queue_->QueuedBuffersCount();
    DCHECK(input_queue_->IsStreaming());

    auto ret = input_queue_->DequeueBuffer();
    if (!ret.first) {
      NOTIFY_ERROR(kPlatformFailureError);
      return;
    }
    if (!ret.second) {
      // We're just out of buffers to dequeue.
      break;
    }

    InputRecord& input_record = input_buffer_map_[ret.second->BufferId()];
    input_record.frame = nullptr;
    if (input_record.ip_output_buffer_index)
      ReuseImageProcessorOutputBuffer(*input_record.ip_output_buffer_index);
  }

  // Dequeue completed output (VIDEO_CAPTURE) buffers, and recycle to the
  // free list.  Notify the client that an output buffer is complete.
  bool buffer_dequeued = false;
  while (output_queue_->QueuedBuffersCount() > 0) {
    DCHECK(output_queue_->IsStreaming());

    auto ret = output_queue_->DequeueBuffer();
    if (!ret.first) {
      NOTIFY_ERROR(kPlatformFailureError);
      return;
    }
    if (!ret.second) {
      // We're just out of buffers to dequeue.
      break;
    }

    output_buffer_queue_.push_back(std::move(ret.second));
    buffer_dequeued = true;
  }

  if (buffer_dequeued)
    PumpBitstreamBuffers();
}

void V4L2VideoEncodeAccelerator::PumpBitstreamBuffers() {
  DVLOGF(4);
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());

  while (!output_buffer_queue_.empty()) {
    auto output_buf = std::move(output_buffer_queue_.front());
    output_buffer_queue_.pop_front();

    size_t bitstream_size = base::checked_cast<size_t>(
        output_buf->GetPlaneBytesUsed(0) - output_buf->GetPlaneDataOffset(0));
    if (bitstream_size > 0) {
      if (bitstream_buffer_pool_.empty()) {
        DVLOGF(4) << "No free bitstream buffer, skip.";
        output_buffer_queue_.push_front(std::move(output_buf));
        break;
      }

      auto buffer_ref = std::move(bitstream_buffer_pool_.back());
      auto buffer_id = buffer_ref->id;
      bitstream_buffer_pool_.pop_back();

      size_t output_data_size = CopyIntoOutputBuffer(
          static_cast<const uint8_t*>(output_buf->GetPlaneMapping(0)) +
              output_buf->GetPlaneDataOffset(0),
          bitstream_size, std::move(buffer_ref));

      DVLOGF(4) << "returning buffer_id=" << buffer_id
                << ", size=" << output_data_size
                << ", key_frame=" << output_buf->IsKeyframe();
      child_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&Client::BitstreamBufferReady, client_, buffer_id,
                         BitstreamBufferMetadata(
                             output_data_size, output_buf->IsKeyframe(),
                             base::TimeDelta::FromMicroseconds(
                                 output_buf->GetTimeStamp().tv_usec +
                                 output_buf->GetTimeStamp().tv_sec *
                                     base::Time::kMicrosecondsPerSecond))));
    }

    if ((encoder_state_ == kFlushing) && output_buf->IsLast()) {
      // Notify client that flush has finished successfully. The flush callback
      // should be called after notifying the last buffer is ready.
      DVLOGF(3) << "Flush completed. Start the encoder again.";
      encoder_state_ = kEncoding;
      child_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(std::move(flush_callback_), true));
      // Start the encoder again.
      struct v4l2_encoder_cmd cmd{};
      cmd.cmd = V4L2_ENC_CMD_START;
      IOCTL_OR_ERROR_RETURN(VIDIOC_ENCODER_CMD, &cmd);
    }
  }

  // We may free some V4L2 output buffers above. Enqueue them if needed.
  if (output_queue_->FreeBuffersCount() > 0) {
    encoder_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&V4L2VideoEncodeAccelerator::Enqueue, weak_this_));
  }
}

bool V4L2VideoEncodeAccelerator::EnqueueInputRecord() {
  DVLOGF(4);
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_GT(input_queue_->FreeBuffersCount(), 0u);
  DCHECK(!encoder_input_queue_.empty());
  TRACE_EVENT0("media,gpu", "V4L2VEA::EnqueueInputRecord");

  // Enqueue an input (VIDEO_OUTPUT) buffer.
  InputFrameInfo frame_info = encoder_input_queue_.front();
  if (frame_info.force_keyframe) {
    std::vector<struct v4l2_ext_control> ctrls;
    struct v4l2_ext_control ctrl{};
    ctrl.id = V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME;
    ctrls.push_back(ctrl);
    if (!SetExtCtrls(ctrls)) {
      VLOGF(1) << "Failed requesting keyframe";
      NOTIFY_ERROR(kPlatformFailureError);
      return false;
    }
  }

  scoped_refptr<VideoFrame> frame = frame_info.frame;

  V4L2WritableBufferRef input_buf = input_queue_->GetFreeBuffer();
  DCHECK(input_buf.IsValid());
  size_t buffer_id = input_buf.BufferId();

  struct timeval timestamp;
  timestamp.tv_sec = static_cast<time_t>(frame->timestamp().InSeconds());
  timestamp.tv_usec =
      frame->timestamp().InMicroseconds() -
      frame->timestamp().InSeconds() * base::Time::kMicrosecondsPerSecond;
  input_buf.SetTimeStamp(timestamp);

  DCHECK_EQ(device_input_layout_->format(), frame->format());
  size_t num_planes = V4L2Device::GetNumPlanesOfV4L2PixFmt(
      Fourcc::FromVideoPixelFormat(device_input_layout_->format(),
                                   !device_input_layout_->is_multi_planar())
          .ToV4L2PixFmt());

  // Create GpuMemoryBufferHandle for native_input_mode.
  gfx::GpuMemoryBufferHandle gmb_handle;
  if (input_buf.Memory() == V4L2_MEMORY_DMABUF) {
    gmb_handle = CreateGpuMemoryBufferHandle(frame.get());
    if (gmb_handle.is_null() || gmb_handle.type != gfx::NATIVE_PIXMAP) {
      VLOGF(1) << "Failed to create native GpuMemoryBufferHandle";
      NOTIFY_ERROR(kPlatformFailureError);
      return false;
    }
  }

  for (size_t i = 0; i < num_planes; ++i) {
    // Single-buffer input format may have multiple color planes, so bytesused
    // of the single buffer should be sum of each color planes' size.
    size_t bytesused = 0;
    if (num_planes == 1) {
      bytesused = VideoFrame::AllocationSize(
          frame->format(), device_input_layout_->coded_size());
    } else {
      bytesused = base::checked_cast<size_t>(
          VideoFrame::PlaneSize(frame->format(), i,
                                device_input_layout_->coded_size())
              .GetArea());
    }

    switch (input_buf.Memory()) {
      case V4L2_MEMORY_USERPTR:
        // Use buffer_size VideoEncodeAccelerator HW requested by S_FMT.
        input_buf.SetPlaneSize(i, device_input_layout_->planes()[i].size);
        break;

      case V4L2_MEMORY_DMABUF: {
        const std::vector<gfx::NativePixmapPlane>& planes =
            gmb_handle.native_pixmap_handle.planes;
        // TODO(crbug.com/901264): The way to pass an offset within a DMA-buf is
        // not defined in V4L2 specification, so we abuse data_offset for now.
        // Fix it when we have the right interface, including any necessary
        // validation and potential alignment
        input_buf.SetPlaneDataOffset(i, planes[i].offset);
        bytesused += planes[i].offset;
        // Workaround: filling length should not be needed. This is a bug of
        // videobuf2 library.
        input_buf.SetPlaneSize(
            i, device_input_layout_->planes()[i].size + planes[i].offset);
        break;
      }
      default:
        NOTREACHED();
        return false;
    }

    input_buf.SetPlaneBytesUsed(i, bytesused);
  }

  switch (input_buf.Memory()) {
    case V4L2_MEMORY_USERPTR: {
      std::vector<void*> user_ptrs;
      for (size_t i = 0; i < num_planes; ++i)
        user_ptrs.push_back(frame->data(i));
      std::move(input_buf).QueueUserPtr(std::move(user_ptrs));
      break;
    }
    case V4L2_MEMORY_DMABUF: {
      std::move(input_buf).QueueDMABuf(gmb_handle.native_pixmap_handle.planes);
      break;
    }
    default:
      NOTREACHED() << "Unknown input memory type: "
                   << static_cast<int>(input_buf.Memory());
      return false;
  }

  // Keep |gmb_handle| alive as long as |frame| is alive so that fds passed
  // to the driver are valid during encoding.
  frame->AddDestructionObserver(
      base::BindOnce([](gfx::GpuMemoryBufferHandle) {}, std::move(gmb_handle)));

  InputRecord& input_record = input_buffer_map_[buffer_id];
  input_record.frame = frame;
  input_record.ip_output_buffer_index = frame_info.ip_output_buffer_index;
  encoder_input_queue_.pop();
  return true;
}

bool V4L2VideoEncodeAccelerator::EnqueueOutputRecord() {
  DVLOGF(4);
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_GT(output_queue_->FreeBuffersCount(), 0u);
  TRACE_EVENT0("media,gpu", "V4L2VEA::EnqueueOutputRecord");

  // Enqueue an output (VIDEO_CAPTURE) buffer.
  V4L2WritableBufferRef output_buf = output_queue_->GetFreeBuffer();
  DCHECK(output_buf.IsValid());
  if (!std::move(output_buf).QueueMMap()) {
    VLOGF(1) << "Failed to QueueMMap.";
    return false;
  }
  return true;
}

bool V4L2VideoEncodeAccelerator::StartDevicePoll() {
  DVLOGF(3);
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(!device_poll_thread_.IsRunning());

  // Start up the device poll thread and schedule its first DevicePollTask().
  if (!device_poll_thread_.Start()) {
    VLOGF(1) << "StartDevicePoll(): Device thread failed to start";
    NOTIFY_ERROR(kPlatformFailureError);
    return false;
  }
  // Enqueue a poll task with no devices to poll on -- it will wait only on the
  // interrupt fd.
  // base::Unretained(this) is safe, because device_poll_thread_ is owned by
  // *this and stops before *this destruction.
  device_poll_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2VideoEncodeAccelerator::DevicePollTask,
                                base::Unretained(this), false));

  return true;
}

bool V4L2VideoEncodeAccelerator::StopDevicePoll() {
  DVLOGF(3);

  // Signal the DevicePollTask() to stop, and stop the device poll thread.
  if (!device_->SetDevicePollInterrupt())
    return false;
  device_poll_thread_.Stop();
  // Clear the interrupt now, to be sure.
  if (!device_->ClearDevicePollInterrupt())
    return false;

  // Tegra driver cannot call Streamoff() when the stream is off, so we check
  // IsStreaming() first.
  if (input_queue_ && input_queue_->IsStreaming() && !input_queue_->Streamoff())
    return false;

  if (output_queue_ && output_queue_->IsStreaming() &&
      !output_queue_->Streamoff())
    return false;

  // Reset all our accounting info.
  while (!encoder_input_queue_.empty())
    encoder_input_queue_.pop();
  for (size_t i = 0; i < input_buffer_map_.size(); ++i) {
    InputRecord& input_record = input_buffer_map_[i];
    input_record.frame = nullptr;
  }

  bitstream_buffer_pool_.clear();

  DVLOGF(3) << "device poll stopped";
  return true;
}

void V4L2VideoEncodeAccelerator::DevicePollTask(bool poll_device) {
  DVLOGF(4);
  DCHECK(device_poll_thread_.task_runner()->BelongsToCurrentThread());

  bool event_pending;
  if (!device_->Poll(poll_device, &event_pending)) {
    NOTIFY_ERROR(kPlatformFailureError);
    return;
  }

  // All processing should happen on ServiceDeviceTask(), since we shouldn't
  // touch encoder state from this thread.
  encoder_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2VideoEncodeAccelerator::ServiceDeviceTask,
                                weak_this_));
}

void V4L2VideoEncodeAccelerator::NotifyError(Error error) {
  // Note that NotifyError() must be called from SetErrorState() only, so that
  // NotifyError() will not be called twice.
  VLOGF(1) << "error=" << error;
  DCHECK(child_task_runner_);
  if (child_task_runner_->BelongsToCurrentThread()) {
    if (client_) {
      client_->NotifyError(error);
      client_ptr_factory_.reset();
    }
    return;
  }

  // Called on encoder_thread_.task_runner().
  child_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::NotifyError, client_, error));
}

void V4L2VideoEncodeAccelerator::SetErrorState(Error error) {
  // We can touch encoder_state_ only if this is the encoder thread or the
  // encoder thread isn't running.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      encoder_thread_.task_runner();
  if (task_runner && !task_runner->BelongsToCurrentThread()) {
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(&V4L2VideoEncodeAccelerator::SetErrorState,
                                  weak_this_, error));
    return;
  }

  // Post NotifyError only if we are already initialized, as the API does
  // not allow doing so before that.
  if (encoder_state_ != kError && encoder_state_ != kUninitialized)
    NotifyError(error);

  encoder_state_ = kError;
}

void V4L2VideoEncodeAccelerator::RequestEncodingParametersChangeTask(
    uint32_t bitrate,
    uint32_t framerate) {
  VLOGF(2) << "bitrate=" << bitrate << ", framerate=" << framerate;
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());
  TRACE_EVENT2("media,gpu", "V4L2VEA::RequestEncodingParametersChangeTask",
               "bitrate", bitrate, "framerate", framerate);

  DCHECK_GT(bitrate, 0u);
  DCHECK_GT(framerate, 0u);

  std::vector<struct v4l2_ext_control> ctrls;
  struct v4l2_ext_control ctrl{};
  ctrl.id = V4L2_CID_MPEG_VIDEO_BITRATE;
  ctrl.value = bitrate;
  ctrls.push_back(ctrl);
  if (!SetExtCtrls(ctrls)) {
    VLOGF(1) << "Failed changing bitrate";
    NOTIFY_ERROR(kPlatformFailureError);
    return;
  }

  struct v4l2_streamparm parms{};
  parms.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  // Note that we are provided "frames per second" but V4L2 expects "time per
  // frame"; hence we provide the reciprocal of the framerate here.
  parms.parm.output.timeperframe.numerator = 1;
  parms.parm.output.timeperframe.denominator = framerate;
  IOCTL_OR_ERROR_RETURN(VIDIOC_S_PARM, &parms);
}

bool V4L2VideoEncodeAccelerator::SetOutputFormat(
    VideoCodecProfile output_profile) {
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(!input_queue_->IsStreaming());
  DCHECK(!output_queue_->IsStreaming());

  DCHECK(!visible_size_.IsEmpty());
  output_buffer_byte_size_ = GetEncodeBitstreamBufferSize(visible_size_);

  struct v4l2_format format{};
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  format.fmt.pix_mp.width = visible_size_.width();
  format.fmt.pix_mp.height = visible_size_.height();
  format.fmt.pix_mp.pixelformat = output_format_fourcc_;
  format.fmt.pix_mp.plane_fmt[0].sizeimage =
      base::checked_cast<__u32>(output_buffer_byte_size_);
  format.fmt.pix_mp.num_planes = 1;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_FMT, &format);
  DCHECK_EQ(format.fmt.pix_mp.pixelformat, output_format_fourcc_);

  // Device might have adjusted the required output size.
  size_t adjusted_output_buffer_size =
      base::checked_cast<size_t>(format.fmt.pix_mp.plane_fmt[0].sizeimage);
  output_buffer_byte_size_ = adjusted_output_buffer_size;

  return true;
}

bool V4L2VideoEncodeAccelerator::NegotiateInputFormat(
    VideoPixelFormat input_format,
    const gfx::Size& size) {
  VLOGF(2);
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(!input_queue_->IsStreaming());
  DCHECK(!output_queue_->IsStreaming());

  // First see if the device can use the provided format directly.
  std::vector<uint32_t> pix_fmt_candidates;
  uint32_t pix_fmt =
      Fourcc::FromVideoPixelFormat(input_format, false).ToV4L2PixFmt();
  if (pix_fmt)
    pix_fmt_candidates.push_back(pix_fmt);
  // Second try preferred input formats for both single-planar and
  // multi-planar.
  for (auto preferred_format :
       device_->PreferredInputFormat(V4L2Device::Type::kEncoder)) {
    pix_fmt_candidates.push_back(preferred_format);
  }

  for (const auto pix_fmt : pix_fmt_candidates) {
    size_t planes_count = V4L2Device::GetNumPlanesOfV4L2PixFmt(pix_fmt);
    DCHECK_GT(planes_count, 0u);
    DCHECK_LE(planes_count, static_cast<size_t>(VIDEO_MAX_PLANES));
    DVLOGF(3) << "Trying S_FMT with " << FourccToString(pix_fmt);

    struct v4l2_format format{};
    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    format.fmt.pix_mp.width = size.width();
    format.fmt.pix_mp.height = size.height();
    format.fmt.pix_mp.pixelformat = pix_fmt;
    format.fmt.pix_mp.num_planes = planes_count;
    if (device_->Ioctl(VIDIOC_S_FMT, &format) == 0 &&
        format.fmt.pix_mp.pixelformat == pix_fmt) {
      DVLOGF(3) << "Success: S_FMT with " << FourccToString(pix_fmt);
      device_input_layout_ = V4L2Device::V4L2FormatToVideoFrameLayout(format);
      if (!device_input_layout_) {
        VLOGF(1) << "Invalid device_input_layout_";
        return false;
      }
      DVLOG(3) << "Negotiated device_input_layout_: " << *device_input_layout_;
      if (!gfx::Rect(device_input_layout_->coded_size())
               .Contains(gfx::Rect(size))) {
        VLOGF(1) << "Input size " << size.ToString()
                 << " exceeds encoder capability. Size encoder can handle: "
                 << device_input_layout_->coded_size().ToString();
        return false;
      }
      if (native_input_mode_) {
        input_allocated_size_ =
            gfx::Size(device_input_layout_->planes()[0].stride,
                      device_input_layout_->coded_size().height());
      } else {
        // TODO(crbug.com/914700): Remove this once
        // Client::RequireBitstreamBuffers uses input's VideoFrameLayout to
        // allocate input buffer.
        input_allocated_size_ = V4L2Device::AllocatedSizeFromV4L2Format(format);
      }
      return true;
    }
  }
  return false;
}

bool V4L2VideoEncodeAccelerator::SetFormats(VideoPixelFormat input_format,
                                            VideoCodecProfile output_profile) {
  VLOGF(2);
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(!input_queue_->IsStreaming());
  DCHECK(!output_queue_->IsStreaming());

  if (!SetOutputFormat(output_profile))
    return false;

  if (!NegotiateInputFormat(input_format, visible_size_))
    return false;

  struct v4l2_rect visible_rect;
  visible_rect.left = 0;
  visible_rect.top = 0;
  visible_rect.width = visible_size_.width();
  visible_rect.height = visible_size_.height();

  struct v4l2_selection selection_arg{};
  selection_arg.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  selection_arg.target = V4L2_SEL_TGT_CROP;
  selection_arg.r = visible_rect;

  // The width and height might be adjusted by driver.
  // Need to read it back and set to visible_size_.
  if (device_->Ioctl(VIDIOC_S_SELECTION, &selection_arg) == 0) {
    DVLOGF(2) << "VIDIOC_S_SELECTION is supported";
    visible_rect = selection_arg.r;
  } else {
    VLOGF(2) << "Fallback to VIDIOC_S/G_CROP";
    struct v4l2_crop crop{};
    crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    crop.c = visible_rect;
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_CROP, &crop);
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_G_CROP, &crop);
    visible_rect = crop.c;
  }

  visible_size_.SetSize(visible_rect.width, visible_rect.height);
  VLOGF(2) << "After adjusted by driver, visible_size_="
           << visible_size_.ToString();

  return true;
}

bool V4L2VideoEncodeAccelerator::IsCtrlExposed(uint32_t ctrl_id) {
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());
  struct v4l2_queryctrl query_ctrl{};
  query_ctrl.id = ctrl_id;

  return device_->Ioctl(VIDIOC_QUERYCTRL, &query_ctrl) == 0;
}

bool V4L2VideoEncodeAccelerator::SetExtCtrls(
    std::vector<struct v4l2_ext_control> ctrls) {
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());
  struct v4l2_ext_controls ext_ctrls{};
  ext_ctrls.ctrl_class = V4L2_CTRL_CLASS_MPEG;
  ext_ctrls.count = ctrls.size();
  ext_ctrls.controls = &ctrls[0];
  return device_->Ioctl(VIDIOC_S_EXT_CTRLS, &ext_ctrls) == 0;
}

bool V4L2VideoEncodeAccelerator::InitControls(const Config& config) {
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());
  std::vector<struct v4l2_ext_control> ctrls;
  struct v4l2_ext_control ctrl{};

  // Enable frame-level bitrate control. This is the only mandatory control.
  ctrl.id = V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE;
  ctrl.value = 1;
  ctrls.push_back(ctrl);
  if (!SetExtCtrls(ctrls)) {
    VLOGF(1) << "Failed enabling bitrate control";
    NOTIFY_ERROR(kPlatformFailureError);
    return false;
  }

  ctrls.clear();
  if (output_format_fourcc_ == V4L2_PIX_FMT_H264) {
#ifndef V4L2_CID_MPEG_VIDEO_H264_SPS_PPS_BEFORE_IDR
#define V4L2_CID_MPEG_VIDEO_H264_SPS_PPS_BEFORE_IDR (V4L2_CID_MPEG_BASE + 388)
#endif
    // Request to inject SPS and PPS before each IDR, if the device supports
    // that feature. Otherwise we'll have to cache and inject ourselves.
    if (IsCtrlExposed(V4L2_CID_MPEG_VIDEO_H264_SPS_PPS_BEFORE_IDR)) {
      memset(&ctrl, 0, sizeof(ctrl));
      ctrl.id = V4L2_CID_MPEG_VIDEO_H264_SPS_PPS_BEFORE_IDR;
      ctrl.value = 1;
      ctrls.push_back(ctrl);
      if (!SetExtCtrls(ctrls)) {
        NOTIFY_ERROR(kPlatformFailureError);
        return false;
      }
      ctrls.clear();
      inject_sps_and_pps_ = false;
      DVLOGF(2) << "Device supports injecting SPS+PPS before each IDR";
    } else {
      inject_sps_and_pps_ = true;
      DVLOGF(2) << "Will inject SPS+PPS before each IDR, unsupported by device";
    }

    // Optional controls.
    // No B-frames, for lowest decoding latency.
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_MPEG_VIDEO_B_FRAMES;
    ctrl.value = 0;
    ctrls.push_back(ctrl);

    // Quantization parameter maximum value (for variable bitrate control).
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_MPEG_VIDEO_H264_MAX_QP;
    ctrl.value = 51;
    ctrls.push_back(ctrl);

    // Set H.264 profile.
    int32_t profile_value =
        V4L2Device::VideoCodecProfileToV4L2H264Profile(config.output_profile);
    if (profile_value < 0) {
      NOTIFY_ERROR(kInvalidArgumentError);
      return false;
    }
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_MPEG_VIDEO_H264_PROFILE;
    ctrl.value = profile_value;
    ctrls.push_back(ctrl);

    // Set H.264 output level from config. Use Level 4.0 as fallback default.
    uint8_t h264_level =
        config.h264_output_level.value_or(H264SPS::kLevelIDC4p0);
    constexpr size_t kH264MacroblockSizeInPixels = 16;
    const uint32_t framerate = config.initial_framerate.value_or(
        VideoEncodeAccelerator::kDefaultFramerate);
    const uint32_t mb_width =
        base::bits::Align(config.input_visible_size.width(),
                          kH264MacroblockSizeInPixels) /
        kH264MacroblockSizeInPixels;
    const uint32_t mb_height =
        base::bits::Align(config.input_visible_size.height(),
                          kH264MacroblockSizeInPixels) /
        kH264MacroblockSizeInPixels;
    const uint32_t framesize_in_mbs = mb_width * mb_height;

    // Check whether the h264 level is valid.
    if (!CheckH264LevelLimits(config.output_profile, h264_level,
                              config.initial_bitrate, framerate,
                              framesize_in_mbs)) {
      base::Optional<uint8_t> valid_level =
          FindValidH264Level(config.output_profile, config.initial_bitrate,
                             framerate, framesize_in_mbs);
      if (!valid_level) {
        VLOGF(1) << "Could not find a valid h264 level for"
                 << " profile=" << config.output_profile
                 << " bitrate=" << config.initial_bitrate
                 << " framerate=" << framerate
                 << " size=" << config.input_visible_size.ToString();
        NOTIFY_ERROR(kInvalidArgumentError);
        return false;
      }

      h264_level = *valid_level;
    }

    int32_t level_value = V4L2Device::H264LevelIdcToV4L2H264Level(h264_level);
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_MPEG_VIDEO_H264_LEVEL;
    ctrl.value = level_value;
    ctrls.push_back(ctrl);

    // Ask not to put SPS and PPS into separate bitstream buffers.
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_MPEG_VIDEO_HEADER_MODE;
    ctrl.value = V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_1ST_FRAME;
    ctrls.push_back(ctrl);
  }

  // Enable macroblock-level bitrate control.
  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_MPEG_VIDEO_MB_RC_ENABLE;
  ctrl.value = 1;
  ctrls.push_back(ctrl);

  // Set GOP length, or default 0 to disable periodic key frames.
  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_MPEG_VIDEO_GOP_SIZE;
  ctrl.value = config.gop_length.value_or(0);
  ctrls.push_back(ctrl);

  // Ignore return value as these controls are optional.
  SetExtCtrls(ctrls);

  // Optional Exynos specific controls.
  ctrls.clear();
  // Enable "tight" bitrate mode. For this to work properly, frame- and mb-level
  // bitrate controls have to be enabled as well.
  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_MPEG_MFC51_VIDEO_RC_REACTION_COEFF;
  ctrl.value = 1;
  ctrls.push_back(ctrl);

  // Force bitrate control to average over a GOP (for tight bitrate
  // tolerance).
  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_MPEG_MFC51_VIDEO_RC_FIXED_TARGET_BIT;
  ctrl.value = 1;
  ctrls.push_back(ctrl);

  // Ignore return value as these controls are optional.
  SetExtCtrls(ctrls);

  return true;
}

bool V4L2VideoEncodeAccelerator::CreateInputBuffers() {
  VLOGF(2);
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(!input_queue_->IsStreaming());

  if (input_queue_->AllocateBuffers(kInputBufferCount, input_memory_type_) <
      kInputBufferCount) {
    VLOGF(1) << "Failed to allocate V4L2 input buffers.";
    return false;
  }

  DCHECK(input_buffer_map_.empty());
  input_buffer_map_.resize(input_queue_->AllocatedBuffersCount());
  return true;
}

bool V4L2VideoEncodeAccelerator::CreateOutputBuffers() {
  VLOGF(2);
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(!output_queue_->IsStreaming());

  if (output_queue_->AllocateBuffers(kOutputBufferCount, V4L2_MEMORY_MMAP) <
      kOutputBufferCount) {
    VLOGF(1) << "Failed to allocate V4L2 output buffers.";
    return false;
  }
  return true;
}

void V4L2VideoEncodeAccelerator::DestroyInputBuffers() {
  VLOGF(2);
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());

  if (!input_queue_ || input_queue_->AllocatedBuffersCount() == 0)
    return;

  DCHECK(!input_queue_->IsStreaming());
  input_queue_->DeallocateBuffers();
  input_buffer_map_.clear();
}

void V4L2VideoEncodeAccelerator::DestroyOutputBuffers() {
  VLOGF(2);
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());

  if (!output_queue_ || output_queue_->AllocatedBuffersCount() == 0)
    return;

  DCHECK(!output_queue_->IsStreaming());
  output_queue_->DeallocateBuffers();
}

}  // namespace media
