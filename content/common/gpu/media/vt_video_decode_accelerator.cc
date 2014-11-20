// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <CoreVideo/CoreVideo.h>
#include <OpenGL/CGLIOSurface.h>
#include <OpenGL/gl.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/sys_byteorder.h"
#include "base/thread_task_runner_handle.h"
#include "content/common/gpu/media/vt_video_decode_accelerator.h"
#include "content/public/common/content_switches.h"
#include "media/base/limits.h"
#include "media/filters/h264_parser.h"
#include "ui/gl/scoped_binders.h"

using content_common_gpu_media::kModuleVt;
using content_common_gpu_media::InitializeStubs;
using content_common_gpu_media::IsVtInitialized;
using content_common_gpu_media::StubPathMap;

#define NOTIFY_STATUS(name, status)                             \
    do {                                                        \
      DLOG(ERROR) << name << " failed with status " << status;  \
      NotifyError(PLATFORM_FAILURE);                            \
    } while (0)

namespace content {

// Size to use for NALU length headers in AVC format (can be 1, 2, or 4).
static const int kNALUHeaderLength = 4;

// We request 5 picture buffers from the client, each of which has a texture ID
// that we can bind decoded frames to. We need enough to satisfy preroll, and
// enough to avoid unnecessary stalling, but no more than that. The resource
// requirements are low, as we don't need the textures to be backed by storage.
static const int kNumPictureBuffers = media::limits::kMaxVideoFrames + 1;

// Route decoded frame callbacks back into the VTVideoDecodeAccelerator.
static void OutputThunk(
    void* decompression_output_refcon,
    void* source_frame_refcon,
    OSStatus status,
    VTDecodeInfoFlags info_flags,
    CVImageBufferRef image_buffer,
    CMTime presentation_time_stamp,
    CMTime presentation_duration) {
  VTVideoDecodeAccelerator* vda =
      reinterpret_cast<VTVideoDecodeAccelerator*>(decompression_output_refcon);
  vda->Output(source_frame_refcon, status, image_buffer);
}

VTVideoDecodeAccelerator::Task::Task(TaskType type) : type(type) {
}

VTVideoDecodeAccelerator::Task::~Task() {
}

VTVideoDecodeAccelerator::Frame::Frame(int32_t bitstream_id)
    : bitstream_id(bitstream_id) {
}

VTVideoDecodeAccelerator::Frame::~Frame() {
}

VTVideoDecodeAccelerator::VTVideoDecodeAccelerator(
    CGLContextObj cgl_context,
    const base::Callback<bool(void)>& make_context_current)
    : cgl_context_(cgl_context),
      make_context_current_(make_context_current),
      client_(NULL),
      state_(STATE_DECODING),
      format_(NULL),
      session_(NULL),
      gpu_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      decoder_thread_("VTDecoderThread"),
      weak_this_factory_(this) {
  DCHECK(!make_context_current_.is_null());
  callback_.decompressionOutputCallback = OutputThunk;
  callback_.decompressionOutputRefCon = this;
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

VTVideoDecodeAccelerator::~VTVideoDecodeAccelerator() {
}

bool VTVideoDecodeAccelerator::Initialize(
    media::VideoCodecProfile profile,
    Client* client) {
  DCHECK(gpu_thread_checker_.CalledOnValidThread());
  client_ = client;

  // Only H.264 is supported.
  if (profile < media::H264PROFILE_MIN || profile > media::H264PROFILE_MAX)
    return false;

  // Require --no-sandbox until VideoToolbox library loading is part of sandbox
  // startup (and this VDA is ready for regular users).
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoSandbox))
    return false;

  if (!IsVtInitialized()) {
    // CoreVideo is also required, but the loader stops after the first
    // path is loaded. Instead we rely on the transitive dependency from
    // VideoToolbox to CoreVideo.
    // TODO(sandersd): Fallback to PrivateFrameworks for VideoToolbox.
    StubPathMap paths;
    paths[kModuleVt].push_back(FILE_PATH_LITERAL(
        "/System/Library/Frameworks/VideoToolbox.framework/VideoToolbox"));
    if (!InitializeStubs(paths))
      return false;
  }

  // Spawn a thread to handle parsing and calling VideoToolbox.
  if (!decoder_thread_.Start())
    return false;

  return true;
}

bool VTVideoDecodeAccelerator::FinishDelayedFrames() {
  DCHECK(decoder_thread_.message_loop_proxy()->BelongsToCurrentThread());
  if (session_) {
    OSStatus status = VTDecompressionSessionFinishDelayedFrames(session_);
    if (status) {
      NOTIFY_STATUS("VTDecompressionSessionFinishDelayedFrames()", status);
      return false;
    }
  }
  return true;
}

bool VTVideoDecodeAccelerator::ConfigureDecoder() {
  DCHECK(decoder_thread_.message_loop_proxy()->BelongsToCurrentThread());
  DCHECK(!last_sps_.empty());
  DCHECK(!last_pps_.empty());

  // Build the configuration records.
  std::vector<const uint8_t*> nalu_data_ptrs;
  std::vector<size_t> nalu_data_sizes;
  nalu_data_ptrs.reserve(3);
  nalu_data_sizes.reserve(3);
  nalu_data_ptrs.push_back(&last_sps_.front());
  nalu_data_sizes.push_back(last_sps_.size());
  if (!last_spsext_.empty()) {
    nalu_data_ptrs.push_back(&last_spsext_.front());
    nalu_data_sizes.push_back(last_spsext_.size());
  }
  nalu_data_ptrs.push_back(&last_pps_.front());
  nalu_data_sizes.push_back(last_pps_.size());

  // Construct a new format description from the parameter sets.
  // TODO(sandersd): Replace this with custom code to support OS X < 10.9.
  format_.reset();
  OSStatus status = CMVideoFormatDescriptionCreateFromH264ParameterSets(
      kCFAllocatorDefault,
      nalu_data_ptrs.size(),      // parameter_set_count
      &nalu_data_ptrs.front(),    // &parameter_set_pointers
      &nalu_data_sizes.front(),   // &parameter_set_sizes
      kNALUHeaderLength,          // nal_unit_header_length
      format_.InitializeInto());
  if (status) {
    NOTIFY_STATUS("CMVideoFormatDescriptionCreateFromH264ParameterSets()",
                  status);
    return false;
  }

  // Store the new configuration data.
  CMVideoDimensions coded_dimensions =
      CMVideoFormatDescriptionGetDimensions(format_);
  coded_size_.SetSize(coded_dimensions.width, coded_dimensions.height);

  // If the session is compatible, there's nothing else to do.
  if (session_ &&
      VTDecompressionSessionCanAcceptFormatDescription(session_, format_)) {
    return true;
  }

  // Prepare VideoToolbox configuration dictionaries.
  base::ScopedCFTypeRef<CFMutableDictionaryRef> decoder_config(
      CFDictionaryCreateMutable(
          kCFAllocatorDefault,
          1,  // capacity
          &kCFTypeDictionaryKeyCallBacks,
          &kCFTypeDictionaryValueCallBacks));

  CFDictionarySetValue(
      decoder_config,
      // kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder
      CFSTR("EnableHardwareAcceleratedVideoDecoder"),
      kCFBooleanTrue);

  base::ScopedCFTypeRef<CFMutableDictionaryRef> image_config(
      CFDictionaryCreateMutable(
          kCFAllocatorDefault,
          4,  // capacity
          &kCFTypeDictionaryKeyCallBacks,
          &kCFTypeDictionaryValueCallBacks));

#define CFINT(i) CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i)
  // TODO(sandersd): RGBA option for 4:4:4 video.
  int32_t pixel_format = kCVPixelFormatType_422YpCbCr8;
  base::ScopedCFTypeRef<CFNumberRef> cf_pixel_format(CFINT(pixel_format));
  base::ScopedCFTypeRef<CFNumberRef> cf_width(CFINT(coded_dimensions.width));
  base::ScopedCFTypeRef<CFNumberRef> cf_height(CFINT(coded_dimensions.height));
#undef CFINT
  CFDictionarySetValue(
      image_config, kCVPixelBufferPixelFormatTypeKey, cf_pixel_format);
  CFDictionarySetValue(image_config, kCVPixelBufferWidthKey, cf_width);
  CFDictionarySetValue(image_config, kCVPixelBufferHeightKey, cf_height);
  CFDictionarySetValue(
      image_config, kCVPixelBufferOpenGLCompatibilityKey, kCFBooleanTrue);

  // TODO(sandersd): Does the old session need to be flushed first?
  session_.reset();
  status = VTDecompressionSessionCreate(
      kCFAllocatorDefault,
      format_,              // video_format_description
      decoder_config,       // video_decoder_specification
      image_config,         // destination_image_buffer_attributes
      &callback_,           // output_callback
      session_.InitializeInto());
  if (status) {
    NOTIFY_STATUS("VTDecompressionSessionCreate()", status);
    return false;
  }

  return true;
}

void VTVideoDecodeAccelerator::DecodeTask(
    const media::BitstreamBuffer& bitstream,
    Frame* frame) {
  DCHECK(decoder_thread_.message_loop_proxy()->BelongsToCurrentThread());

  // Map the bitstream buffer.
  base::SharedMemory memory(bitstream.handle(), true);
  size_t size = bitstream.size();
  if (!memory.Map(size)) {
    DLOG(ERROR) << "Failed to map bitstream buffer";
    NotifyError(PLATFORM_FAILURE);
    return;
  }
  const uint8_t* buf = static_cast<uint8_t*>(memory.memory());

  // NALUs are stored with Annex B format in the bitstream buffer (start codes),
  // but VideoToolbox expects AVC format (length headers), so we must rewrite
  // the data.
  //
  // Locate relevant NALUs and compute the size of the rewritten data. Also
  // record any parameter sets for VideoToolbox initialization.
  bool config_changed = false;
  size_t data_size = 0;
  std::vector<media::H264NALU> nalus;
  parser_.SetStream(buf, size);
  media::H264NALU nalu;
  while (true) {
    media::H264Parser::Result result = parser_.AdvanceToNextNALU(&nalu);
    if (result == media::H264Parser::kEOStream)
      break;
    if (result != media::H264Parser::kOk) {
      DLOG(ERROR) << "Failed to find H.264 NALU";
      NotifyError(PLATFORM_FAILURE);
      return;
    }
    switch (nalu.nal_unit_type) {
      case media::H264NALU::kSPS:
        last_sps_.assign(nalu.data, nalu.data + nalu.size);
        last_spsext_.clear();
        config_changed = true;
        break;
      case media::H264NALU::kSPSExt:
        // TODO(sandersd): Check that the previous NALU was an SPS.
        last_spsext_.assign(nalu.data, nalu.data + nalu.size);
        config_changed = true;
        break;
      case media::H264NALU::kPPS:
        last_pps_.assign(nalu.data, nalu.data + nalu.size);
        config_changed = true;
        break;
      case media::H264NALU::kSliceDataA:
      case media::H264NALU::kSliceDataB:
      case media::H264NALU::kSliceDataC:
        DLOG(ERROR) << "Coded slide data partitions not implemented.";
        NotifyError(PLATFORM_FAILURE);
        return;
      case media::H264NALU::kIDRSlice:
      case media::H264NALU::kNonIDRSlice:
        // TODO(sandersd): Compute pic_order_count.
      default:
        nalus.push_back(nalu);
        data_size += kNALUHeaderLength + nalu.size;
        break;
    }
  }

  // Initialize VideoToolbox.
  // TODO(sandersd): Instead of assuming that the last SPS and PPS units are
  // always the correct ones, maintain a cache of recent SPS and PPS units and
  // select from them using the slice header.
  if (config_changed) {
    if (last_sps_.size() == 0 || last_pps_.size() == 0) {
      DLOG(ERROR) << "Invalid configuration data";
      NotifyError(INVALID_ARGUMENT);
      return;
    }
    if (!ConfigureDecoder())
      return;
  }

  // If there are no non-configuration units, drop the bitstream buffer by
  // returning an empty frame.
  if (!data_size) {
    if (!FinishDelayedFrames())
      return;
    gpu_task_runner_->PostTask(FROM_HERE, base::Bind(
        &VTVideoDecodeAccelerator::DecodeDone, weak_this_, frame));
    return;
  }

  // If the session is not configured by this point, fail.
  if (!session_) {
    DLOG(ERROR) << "Image slice without configuration";
    NotifyError(INVALID_ARGUMENT);
    return;
  }

  // Create a memory-backed CMBlockBuffer for the translated data.
  // TODO(sandersd): Pool of memory blocks.
  base::ScopedCFTypeRef<CMBlockBufferRef> data;
  OSStatus status = CMBlockBufferCreateWithMemoryBlock(
      kCFAllocatorDefault,
      NULL,                 // &memory_block
      data_size,            // block_length
      kCFAllocatorDefault,  // block_allocator
      NULL,                 // &custom_block_source
      0,                    // offset_to_data
      data_size,            // data_length
      0,                    // flags
      data.InitializeInto());
  if (status) {
    NOTIFY_STATUS("CMBlockBufferCreateWithMemoryBlock()", status);
    return;
  }

  // Copy NALU data into the CMBlockBuffer, inserting length headers.
  size_t offset = 0;
  for (size_t i = 0; i < nalus.size(); i++) {
    media::H264NALU& nalu = nalus[i];
    uint32_t header = base::HostToNet32(static_cast<uint32_t>(nalu.size));
    status = CMBlockBufferReplaceDataBytes(
        &header, data, offset, kNALUHeaderLength);
    if (status) {
      NOTIFY_STATUS("CMBlockBufferReplaceDataBytes()", status);
      return;
    }
    offset += kNALUHeaderLength;
    status = CMBlockBufferReplaceDataBytes(nalu.data, data, offset, nalu.size);
    if (status) {
      NOTIFY_STATUS("CMBlockBufferReplaceDataBytes()", status);
      return;
    }
    offset += nalu.size;
  }

  // Package the data in a CMSampleBuffer.
  base::ScopedCFTypeRef<CMSampleBufferRef> sample;
  status = CMSampleBufferCreate(
      kCFAllocatorDefault,
      data,                 // data_buffer
      true,                 // data_ready
      NULL,                 // make_data_ready_callback
      NULL,                 // make_data_ready_refcon
      format_,              // format_description
      1,                    // num_samples
      0,                    // num_sample_timing_entries
      NULL,                 // &sample_timing_array
      0,                    // num_sample_size_entries
      NULL,                 // &sample_size_array
      sample.InitializeInto());
  if (status) {
    NOTIFY_STATUS("CMSampleBufferCreate()", status);
    return;
  }

  // Update the frame data.
  frame->coded_size = coded_size_;

  // Send the frame for decoding.
  // Asynchronous Decompression allows for parallel submission of frames
  // (without it, DecodeFrame() does not return until the frame has been
  // decoded). We don't enable Temporal Processing so that frames are always
  // returned in decode order; this makes it easier to avoid deadlock.
  VTDecodeFrameFlags decode_flags =
      kVTDecodeFrame_EnableAsynchronousDecompression;
  status = VTDecompressionSessionDecodeFrame(
      session_,
      sample,                                 // sample_buffer
      decode_flags,                           // decode_flags
      reinterpret_cast<void*>(frame),         // source_frame_refcon
      NULL);                                  // &info_flags_out
  if (status) {
    NOTIFY_STATUS("VTDecompressionSessionDecodeFrame()", status);
    return;
  }
}

// This method may be called on any VideoToolbox thread.
void VTVideoDecodeAccelerator::Output(
    void* source_frame_refcon,
    OSStatus status,
    CVImageBufferRef image_buffer) {
  if (status) {
    NOTIFY_STATUS("Decoding", status);
  } else if (CFGetTypeID(image_buffer) != CVPixelBufferGetTypeID()) {
    DLOG(ERROR) << "Decoded frame is not a CVPixelBuffer";
    NotifyError(PLATFORM_FAILURE);
  } else {
    Frame* frame = reinterpret_cast<Frame*>(source_frame_refcon);
    frame->image.reset(image_buffer, base::scoped_policy::RETAIN);
    gpu_task_runner_->PostTask(FROM_HERE, base::Bind(
        &VTVideoDecodeAccelerator::DecodeDone, weak_this_, frame));
  }
}

void VTVideoDecodeAccelerator::DecodeDone(Frame* frame) {
  DCHECK(gpu_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(frame->bitstream_id, pending_frames_.front()->bitstream_id);
  Task task(TASK_FRAME);
  task.frame = pending_frames_.front();
  pending_frames_.pop();
  pending_tasks_.push(task);
  ProcessTasks();
}

void VTVideoDecodeAccelerator::FlushTask(TaskType type) {
  DCHECK(decoder_thread_.message_loop_proxy()->BelongsToCurrentThread());
  FinishDelayedFrames();

  // Always queue a task, even if FinishDelayedFrames() fails, so that
  // destruction always completes.
  gpu_task_runner_->PostTask(FROM_HERE, base::Bind(
      &VTVideoDecodeAccelerator::FlushDone, weak_this_, type));
}

void VTVideoDecodeAccelerator::FlushDone(TaskType type) {
  DCHECK(gpu_thread_checker_.CalledOnValidThread());
  pending_tasks_.push(Task(type));
  ProcessTasks();
}

void VTVideoDecodeAccelerator::Decode(const media::BitstreamBuffer& bitstream) {
  DCHECK(gpu_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(assigned_bitstream_ids_.count(bitstream.id()), 0u);
  assigned_bitstream_ids_.insert(bitstream.id());
  Frame* frame = new Frame(bitstream.id());
  pending_frames_.push(make_linked_ptr(frame));
  decoder_thread_.message_loop_proxy()->PostTask(FROM_HERE, base::Bind(
      &VTVideoDecodeAccelerator::DecodeTask, base::Unretained(this),
      bitstream, frame));
}

void VTVideoDecodeAccelerator::AssignPictureBuffers(
    const std::vector<media::PictureBuffer>& pictures) {
  DCHECK(gpu_thread_checker_.CalledOnValidThread());

  for (const media::PictureBuffer& picture : pictures) {
    DCHECK(!texture_ids_.count(picture.id()));
    assigned_picture_ids_.insert(picture.id());
    available_picture_ids_.push_back(picture.id());
    texture_ids_[picture.id()] = picture.texture_id();
  }

  // Pictures are not marked as uncleared until after this method returns, and
  // they will be broken if they are used before that happens. So, schedule
  // future work after that happens.
  gpu_task_runner_->PostTask(FROM_HERE, base::Bind(
      &VTVideoDecodeAccelerator::ProcessTasks, weak_this_));
}

void VTVideoDecodeAccelerator::ReusePictureBuffer(int32_t picture_id) {
  DCHECK(gpu_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(CFGetRetainCount(picture_bindings_[picture_id]), 1);
  picture_bindings_.erase(picture_id);
  if (assigned_picture_ids_.count(picture_id) != 0) {
    available_picture_ids_.push_back(picture_id);
    ProcessTasks();
  } else {
    client_->DismissPictureBuffer(picture_id);
  }
}

void VTVideoDecodeAccelerator::ProcessTasks() {
  DCHECK(gpu_thread_checker_.CalledOnValidThread());

  while (!pending_tasks_.empty()) {
    const Task& task = pending_tasks_.front();

    switch (state_) {
      case STATE_DECODING:
        if (!ProcessTask(task))
          return;
        pending_tasks_.pop();
        break;

      case STATE_ERROR:
        // Do nothing until Destroy() is called.
        return;

      case STATE_DESTROYING:
        // Discard tasks until destruction is complete.
        if (task.type == TASK_DESTROY) {
          delete this;
          return;
        }
        pending_tasks_.pop();
        break;
    }
  }
}

bool VTVideoDecodeAccelerator::ProcessTask(const Task& task) {
  DCHECK(gpu_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, STATE_DECODING);

  switch (task.type) {
    case TASK_FRAME:
      return ProcessFrame(*task.frame);

    case TASK_FLUSH:
      DCHECK_EQ(task.type, pending_flush_tasks_.front());
      pending_flush_tasks_.pop();
      client_->NotifyFlushDone();
      return true;

    case TASK_RESET:
      DCHECK_EQ(task.type, pending_flush_tasks_.front());
      pending_flush_tasks_.pop();
      client_->NotifyResetDone();
      return true;

    case TASK_DESTROY:
      NOTREACHED() << "Can't destroy while in STATE_DECODING.";
      NotifyError(ILLEGAL_STATE);
      return false;
  }
}

bool VTVideoDecodeAccelerator::ProcessFrame(const Frame& frame) {
  DCHECK(gpu_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, STATE_DECODING);
  // If the next pending flush is for a reset, then the frame will be dropped.
  bool resetting = !pending_flush_tasks_.empty() &&
                   pending_flush_tasks_.front() == TASK_RESET;
  if (!resetting && frame.image.get()) {
    // If the |coded_size| has changed, request new picture buffers and then
    // wait for them.
    // TODO(sandersd): If GpuVideoDecoder didn't specifically check the size of
    // textures, this would be unnecessary, as the size is actually a property
    // of the texture binding, not the texture. We rebind every frame, so the
    // size passed to ProvidePictureBuffers() is meaningless.
    if (picture_size_ != frame.coded_size) {
      // Dismiss current pictures.
      for (int32_t picture_id : assigned_picture_ids_)
        client_->DismissPictureBuffer(picture_id);
      assigned_picture_ids_.clear();
      available_picture_ids_.clear();

      // Request new pictures.
      picture_size_ = frame.coded_size;
      client_->ProvidePictureBuffers(
          kNumPictureBuffers, coded_size_, GL_TEXTURE_RECTANGLE_ARB);
      return false;
    }
    if (!SendFrame(frame))
      return false;
  }
  assigned_bitstream_ids_.erase(frame.bitstream_id);
  client_->NotifyEndOfBitstreamBuffer(frame.bitstream_id);
  return true;
}

bool VTVideoDecodeAccelerator::SendFrame(const Frame& frame) {
  DCHECK(gpu_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, STATE_DECODING);

  if (available_picture_ids_.empty())
    return false;

  int32_t picture_id = available_picture_ids_.back();
  IOSurfaceRef surface = CVPixelBufferGetIOSurface(frame.image.get());

  if (!make_context_current_.Run()) {
    DLOG(ERROR) << "Failed to make GL context current";
    NotifyError(PLATFORM_FAILURE);
    return false;
  }

  glEnable(GL_TEXTURE_RECTANGLE_ARB);
  gfx::ScopedTextureBinder
      texture_binder(GL_TEXTURE_RECTANGLE_ARB, texture_ids_[picture_id]);
  CGLError status = CGLTexImageIOSurface2D(
      cgl_context_,                 // ctx
      GL_TEXTURE_RECTANGLE_ARB,     // target
      GL_RGB,                       // internal_format
      frame.coded_size.width(),     // width
      frame.coded_size.height(),    // height
      GL_YCBCR_422_APPLE,           // format
      GL_UNSIGNED_SHORT_8_8_APPLE,  // type
      surface,                      // io_surface
      0);                           // plane
  if (status != kCGLNoError) {
    NOTIFY_STATUS("CGLTexImageIOSurface2D()", status);
    return false;
  }
  glDisable(GL_TEXTURE_RECTANGLE_ARB);

  available_picture_ids_.pop_back();
  picture_bindings_[picture_id] = frame.image;
  client_->PictureReady(media::Picture(
      picture_id, frame.bitstream_id, gfx::Rect(frame.coded_size)));
  return true;
}

void VTVideoDecodeAccelerator::NotifyError(Error error) {
  if (!gpu_thread_checker_.CalledOnValidThread()) {
    gpu_task_runner_->PostTask(FROM_HERE, base::Bind(
        &VTVideoDecodeAccelerator::NotifyError, weak_this_, error));
  } else if (state_ == STATE_DECODING) {
    state_ = STATE_ERROR;
    client_->NotifyError(error);
  }
}

void VTVideoDecodeAccelerator::QueueFlush(TaskType type) {
  DCHECK(gpu_thread_checker_.CalledOnValidThread());
  pending_flush_tasks_.push(type);
  decoder_thread_.message_loop_proxy()->PostTask(FROM_HERE, base::Bind(
      &VTVideoDecodeAccelerator::FlushTask, base::Unretained(this),
      type));

  // If this is a new flush request, see if we can make progress.
  if (pending_flush_tasks_.size() == 1)
    ProcessTasks();
}

void VTVideoDecodeAccelerator::Flush() {
  DCHECK(gpu_thread_checker_.CalledOnValidThread());
  QueueFlush(TASK_FLUSH);
}

void VTVideoDecodeAccelerator::Reset() {
  DCHECK(gpu_thread_checker_.CalledOnValidThread());
  QueueFlush(TASK_RESET);
}

void VTVideoDecodeAccelerator::Destroy() {
  DCHECK(gpu_thread_checker_.CalledOnValidThread());

  // In a forceful shutdown, the decoder thread may be dead already.
  if (!decoder_thread_.IsRunning()) {
    delete this;
    return;
  }

  // For a graceful shutdown, return assigned buffers and flush before
  // destructing |this|.
  for (int32_t bitstream_id : assigned_bitstream_ids_)
    client_->NotifyEndOfBitstreamBuffer(bitstream_id);
  assigned_bitstream_ids_.clear();
  state_ = STATE_DESTROYING;
  QueueFlush(TASK_DESTROY);
}

bool VTVideoDecodeAccelerator::CanDecodeOnIOThread() {
  return false;
}

}  // namespace content
