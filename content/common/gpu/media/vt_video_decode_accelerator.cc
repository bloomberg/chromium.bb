// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <CoreVideo/CoreVideo.h>
#include <OpenGL/CGLIOSurface.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/sys_byteorder.h"
#include "base/thread_task_runner_handle.h"
#include "content/common/gpu/media/vt_video_decode_accelerator.h"
#include "content/public/common/content_switches.h"
#include "media/filters/h264_parser.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_cgl.h"

using content_common_gpu_media::kModuleVt;
using content_common_gpu_media::InitializeStubs;
using content_common_gpu_media::IsVtInitialized;
using content_common_gpu_media::StubPathMap;

namespace content {

// Size of NALU length headers in AVCC/MPEG-4 format (can be 1, 2, or 4).
static const int kNALUHeaderLength = 4;

// We only request 5 picture buffers from the client which are used to hold the
// decoded samples. These buffers are then reused when the client tells us that
// it is done with the buffer.
static const int kNumPictureBuffers = 5;

// Route decoded frame callbacks back into the VTVideoDecodeAccelerator.
static void OutputThunk(
    void* decompression_output_refcon,
    void* source_frame_refcon,
    OSStatus status,
    VTDecodeInfoFlags info_flags,
    CVImageBufferRef image_buffer,
    CMTime presentation_time_stamp,
    CMTime presentation_duration) {
  // TODO(sandersd): Implement flush-before-delete to guarantee validity.
  VTVideoDecodeAccelerator* vda =
      reinterpret_cast<VTVideoDecodeAccelerator*>(decompression_output_refcon);
  int32_t bitstream_id = reinterpret_cast<intptr_t>(source_frame_refcon);
  vda->Output(bitstream_id, status, image_buffer);
}

VTVideoDecodeAccelerator::DecodedFrame::DecodedFrame(
    int32_t bitstream_id,
    CVImageBufferRef image_buffer)
    : bitstream_id(bitstream_id),
      image_buffer(image_buffer) {
}

VTVideoDecodeAccelerator::DecodedFrame::~DecodedFrame() {
}

VTVideoDecodeAccelerator::VTVideoDecodeAccelerator(CGLContextObj cgl_context)
    : cgl_context_(cgl_context),
      client_(NULL),
      format_(NULL),
      session_(NULL),
      gpu_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_this_factory_(this),
      decoder_thread_("VTDecoderThread") {
  callback_.decompressionOutputCallback = OutputThunk;
  callback_.decompressionOutputRefCon = this;
}

VTVideoDecodeAccelerator::~VTVideoDecodeAccelerator() {
}

bool VTVideoDecodeAccelerator::Initialize(
    media::VideoCodecProfile profile,
    Client* client) {
  DCHECK(CalledOnValidThread());
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

// TODO(sandersd): Proper error reporting instead of CHECKs.
void VTVideoDecodeAccelerator::ConfigureDecoder(
    const std::vector<const uint8_t*>& nalu_data_ptrs,
    const std::vector<size_t>& nalu_data_sizes) {
  DCHECK(decoder_thread_.message_loop_proxy()->BelongsToCurrentThread());
  // Construct a new format description from the parameter sets.
  // TODO(sandersd): Replace this with custom code to support OS X < 10.9.
  format_.reset();
  CHECK(!CMVideoFormatDescriptionCreateFromH264ParameterSets(
      kCFAllocatorDefault,
      nalu_data_ptrs.size(),      // parameter_set_count
      &nalu_data_ptrs.front(),    // &parameter_set_pointers
      &nalu_data_sizes.front(),   // &parameter_set_sizes
      kNALUHeaderLength,          // nal_unit_header_length
      format_.InitializeInto()));
  CMVideoDimensions coded_dimensions =
      CMVideoFormatDescriptionGetDimensions(format_);

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

  // TODO(sandersd): Check if the session is already compatible.
  // TODO(sandersd): Flush.
  session_.reset();
  CHECK(!VTDecompressionSessionCreate(
      kCFAllocatorDefault,
      format_,              // video_format_description
      decoder_config,       // video_decoder_specification
      image_config,         // destination_image_buffer_attributes
      &callback_,           // output_callback
      session_.InitializeInto()));

  // If the size has changed, trigger a request for new picture buffers.
  gfx::Size new_coded_size(coded_dimensions.width, coded_dimensions.height);
  if (coded_size_ != new_coded_size) {
    coded_size_ = new_coded_size;
    gpu_task_runner_->PostTask(FROM_HERE, base::Bind(
        &VTVideoDecodeAccelerator::SizeChangedTask,
        weak_this_factory_.GetWeakPtr(),
        coded_size_));;
  }
}

void VTVideoDecodeAccelerator::Decode(const media::BitstreamBuffer& bitstream) {
  DCHECK(CalledOnValidThread());
  // TODO(sandersd): Test what happens if bitstream buffers are passed to VT out
  // of order.
  decoder_thread_.message_loop_proxy()->PostTask(FROM_HERE, base::Bind(
      &VTVideoDecodeAccelerator::DecodeTask, base::Unretained(this),
      bitstream));
}

// TODO(sandersd): Proper error reporting instead of CHECKs.
void VTVideoDecodeAccelerator::DecodeTask(
    const media::BitstreamBuffer bitstream) {
  DCHECK(decoder_thread_.message_loop_proxy()->BelongsToCurrentThread());

  // Map the bitstream buffer.
  base::SharedMemory memory(bitstream.handle(), true);
  size_t size = bitstream.size();
  CHECK(memory.Map(size));
  const uint8_t* buf = static_cast<uint8_t*>(memory.memory());

  // NALUs are stored with Annex B format in the bitstream buffer (start codes),
  // but VideoToolbox expects AVCC/MPEG-4 format (length headers), so we must
  // rewrite the data.
  //
  // 1. Locate relevant NALUs and compute the size of the translated data.
  //    Also record any parameter sets for VideoToolbox initialization.
  size_t data_size = 0;
  std::vector<media::H264NALU> nalus;
  std::vector<const uint8_t*> config_nalu_data_ptrs;
  std::vector<size_t> config_nalu_data_sizes;
  parser_.SetStream(buf, size);
  media::H264NALU nalu;
  while (true) {
    media::H264Parser::Result result = parser_.AdvanceToNextNALU(&nalu);
    if (result == media::H264Parser::kEOStream)
      break;
    CHECK_EQ(result, media::H264Parser::kOk);
    // TODO(sandersd): Check that these are only at the start.
    if (nalu.nal_unit_type == media::H264NALU::kSPS ||
        nalu.nal_unit_type == media::H264NALU::kPPS ||
        nalu.nal_unit_type == media::H264NALU::kSPSExt) {
      DVLOG(2) << "Parameter set " << nalu.nal_unit_type;
      config_nalu_data_ptrs.push_back(nalu.data);
      config_nalu_data_sizes.push_back(nalu.size);
    } else {
      nalus.push_back(nalu);
      data_size += kNALUHeaderLength + nalu.size;
    }
  }

  // 2. Initialize VideoToolbox.
  // TODO(sandersd): Reinitialize when there are new parameter sets.
  if (!session_)
    ConfigureDecoder(config_nalu_data_ptrs, config_nalu_data_sizes);

  // 3. Allocate a memory-backed CMBlockBuffer for the translated data.
  base::ScopedCFTypeRef<CMBlockBufferRef> data;
  CHECK(!CMBlockBufferCreateWithMemoryBlock(
      kCFAllocatorDefault,
      NULL,                 // &memory_block
      data_size,            // block_length
      kCFAllocatorDefault,  // block_allocator
      NULL,                 // &custom_block_source
      0,                    // offset_to_data
      data_size,            // data_length
      0,                    // flags
      data.InitializeInto()));

  // 4. Copy NALU data, inserting length headers.
  size_t offset = 0;
  for (size_t i = 0; i < nalus.size(); i++) {
    media::H264NALU& nalu = nalus[i];
    uint32_t header = base::HostToNet32(static_cast<uint32_t>(nalu.size));
    CHECK(!CMBlockBufferReplaceDataBytes(
        &header, data, offset, kNALUHeaderLength));
    offset += kNALUHeaderLength;
    CHECK(!CMBlockBufferReplaceDataBytes(nalu.data, data, offset, nalu.size));
    offset += nalu.size;
  }

  // 5. Package the data for VideoToolbox and request decoding.
  base::ScopedCFTypeRef<CMSampleBufferRef> frame;
  CHECK(!CMSampleBufferCreate(
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
      frame.InitializeInto()));

  // Asynchronous Decompression allows for parallel submission of frames
  // (without it, DecodeFrame() does not return until the frame has been
  // decoded). We don't enable Temporal Processing so that frames are always
  // returned in decode order; this makes it easier to avoid deadlock.
  VTDecodeFrameFlags decode_flags =
      kVTDecodeFrame_EnableAsynchronousDecompression;

  intptr_t bitstream_id = bitstream.id();
  CHECK(!VTDecompressionSessionDecodeFrame(
      session_,
      frame,                                  // sample_buffer
      decode_flags,                           // decode_flags
      reinterpret_cast<void*>(bitstream_id),  // source_frame_refcon
      NULL));                                 // &info_flags_out
}

// This method may be called on any VideoToolbox thread.
// TODO(sandersd): Proper error reporting instead of CHECKs.
void VTVideoDecodeAccelerator::Output(
    int32_t bitstream_id,
    OSStatus status,
    CVImageBufferRef image_buffer) {
  CHECK(!status);
  CHECK_EQ(CFGetTypeID(image_buffer), CVPixelBufferGetTypeID());
  CFRetain(image_buffer);
  gpu_task_runner_->PostTask(FROM_HERE, base::Bind(
      &VTVideoDecodeAccelerator::OutputTask,
      weak_this_factory_.GetWeakPtr(),
      DecodedFrame(bitstream_id, image_buffer)));
}

void VTVideoDecodeAccelerator::OutputTask(DecodedFrame frame) {
  DCHECK(CalledOnValidThread());
  decoded_frames_.push(frame);
  SendPictures();
}

void VTVideoDecodeAccelerator::SizeChangedTask(gfx::Size coded_size) {
  DCHECK(CalledOnValidThread());
  texture_size_ = coded_size;
  // TODO(sandersd): Dismiss existing picture buffers.
  client_->ProvidePictureBuffers(
      kNumPictureBuffers, texture_size_, GL_TEXTURE_RECTANGLE_ARB);
}

void VTVideoDecodeAccelerator::AssignPictureBuffers(
    const std::vector<media::PictureBuffer>& pictures) {
  DCHECK(CalledOnValidThread());

  for (size_t i = 0; i < pictures.size(); i++) {
    CHECK(!texture_ids_.count(pictures[i].id()));
    available_picture_ids_.push(pictures[i].id());
    texture_ids_[pictures[i].id()] = pictures[i].texture_id();
  }

  // Pictures are not marked as uncleared until this method returns. They will
  // become broken if they are used before that happens.
  gpu_task_runner_->PostTask(FROM_HERE, base::Bind(
      &VTVideoDecodeAccelerator::SendPictures,
      weak_this_factory_.GetWeakPtr()));
}

void VTVideoDecodeAccelerator::ReusePictureBuffer(int32_t picture_id) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(CFGetRetainCount(picture_bindings_[picture_id]), 1);
  picture_bindings_.erase(picture_id);
  available_picture_ids_.push(picture_id);
  SendPictures();
}

// TODO(sandersd): Proper error reporting instead of CHECKs.
void VTVideoDecodeAccelerator::SendPictures() {
  DCHECK(CalledOnValidThread());
  if (available_picture_ids_.empty() || decoded_frames_.empty())
    return;

  gfx::ScopedCGLSetCurrentContext scoped_set_current_context(cgl_context_);
  glEnable(GL_TEXTURE_RECTANGLE_ARB);

  while (!available_picture_ids_.empty() && !decoded_frames_.empty()) {
    int32_t picture_id = available_picture_ids_.front();
    available_picture_ids_.pop();
    DecodedFrame frame = decoded_frames_.front();
    decoded_frames_.pop();
    IOSurfaceRef surface = CVPixelBufferGetIOSurface(frame.image_buffer);

    gfx::ScopedTextureBinder
        texture_binder(GL_TEXTURE_RECTANGLE_ARB, texture_ids_[picture_id]);
    CHECK(!CGLTexImageIOSurface2D(
        cgl_context_,                 // ctx
        GL_TEXTURE_RECTANGLE_ARB,     // target
        GL_RGB,                       // internal_format
        texture_size_.width(),        // width
        texture_size_.height(),       // height
        GL_YCBCR_422_APPLE,           // format
        GL_UNSIGNED_SHORT_8_8_APPLE,  // type
        surface,                      // io_surface
        0));                          // plane

    picture_bindings_[picture_id] = frame.image_buffer;
    client_->PictureReady(media::Picture(picture_id, frame.bitstream_id));
    client_->NotifyEndOfBitstreamBuffer(frame.bitstream_id);
  }

  glDisable(GL_TEXTURE_RECTANGLE_ARB);
}

void VTVideoDecodeAccelerator::Flush() {
  DCHECK(CalledOnValidThread());
  // TODO(sandersd): Trigger flush, sending frames.
}

void VTVideoDecodeAccelerator::Reset() {
  DCHECK(CalledOnValidThread());
  // TODO(sandersd): Trigger flush, discarding frames.
}

void VTVideoDecodeAccelerator::Destroy() {
  DCHECK(CalledOnValidThread());
  // TODO(sandersd): Trigger flush, discarding frames, and wait for them.
  delete this;
}

bool VTVideoDecodeAccelerator::CanDecodeOnIOThread() {
  return false;
}

}  // namespace content
