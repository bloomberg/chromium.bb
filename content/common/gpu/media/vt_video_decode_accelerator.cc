// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <CoreVideo/CoreVideo.h>
#include <OpenGL/CGLIOSurface.h>

#include "base/bind.h"
#include "base/thread_task_runner_handle.h"
#include "content/common/gpu/media/vt_video_decode_accelerator.h"
#include "media/filters/h264_parser.h"

using content_common_gpu_media::kModuleVt;
using content_common_gpu_media::InitializeStubs;
using content_common_gpu_media::IsVtInitialized;
using content_common_gpu_media::StubPathMap;

namespace content {

// Size of length headers prepended to NALUs in MPEG-4 framing. (1, 2, or 4.)
static const int kNALUHeaderLength = 4;

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
  int32_t* bitstream_id_ptr = reinterpret_cast<int32_t*>(source_frame_refcon);
  int32_t bitstream_id = *bitstream_id_ptr;
  delete bitstream_id_ptr;
  CFRetain(image_buffer);
  vda->Output(bitstream_id, status, info_flags, image_buffer);
}

VTVideoDecodeAccelerator::VTVideoDecodeAccelerator(CGLContextObj cgl_context)
    : cgl_context_(cgl_context),
      client_(NULL),
      decoder_thread_("VTDecoderThread"),
      format_(NULL),
      session_(NULL),
      weak_this_factory_(this) {
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

  // TODO(sandersd): Move VideoToolbox library loading to sandbox startup;
  // until then, --no-sandbox is required.
  if (!IsVtInitialized()) {
    StubPathMap paths;
    // CoreVideo is also required, but the loader stops after the first
    // path is loaded. Instead we rely on the transitive dependency from
    // VideoToolbox to CoreVideo.
    // TODO(sandersd): Fallback to PrivateFrameworks for VideoToolbox.
    paths[kModuleVt].push_back(FILE_PATH_LITERAL(
        "/System/Library/Frameworks/VideoToolbox.framework/VideoToolbox"));
    if (!InitializeStubs(paths))
      return false;
  }

  // Spawn a thread to handle parsing and calling VideoToolbox.
  if (!decoder_thread_.Start())
    return false;

  // Note that --ignore-gpu-blacklist is still required to get here.
  return true;
}

// TODO(sandersd): Proper error reporting instead of CHECKs.
void VTVideoDecodeAccelerator::ConfigureDecoder(
    const std::vector<const uint8_t*>& nalu_data_ptrs,
    const std::vector<size_t>& nalu_data_sizes) {
  format_.reset();
  CHECK(!CMVideoFormatDescriptionCreateFromH264ParameterSets(
      kCFAllocatorDefault,
      nalu_data_ptrs.size(),      // parameter_set_count
      &nalu_data_ptrs.front(),    // &parameter_set_pointers
      &nalu_data_sizes.front(),   // &parameter_set_sizes
      kNALUHeaderLength,          // nal_unit_header_length
      format_.InitializeInto()
  ));

  // TODO(sandersd): Check if the size has changed and handle picture requests.
  CMVideoDimensions coded_size = CMVideoFormatDescriptionGetDimensions(format_);
  coded_size_.SetSize(coded_size.width, coded_size.height);

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

  // TODO(sandersd): ARGB for video that is not 4:2:0.
  int32_t pixel_format = '2vuy';
#define CFINT(i) CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i)
  base::ScopedCFTypeRef<CFNumberRef> cf_pixel_format(CFINT(pixel_format));
  base::ScopedCFTypeRef<CFNumberRef> cf_width(CFINT(coded_size.width));
  base::ScopedCFTypeRef<CFNumberRef> cf_height(CFINT(coded_size.height));
#undef CFINT
  CFDictionarySetValue(
      image_config, kCVPixelBufferPixelFormatTypeKey, cf_pixel_format);
  CFDictionarySetValue(image_config, kCVPixelBufferWidthKey, cf_width);
  CFDictionarySetValue(image_config, kCVPixelBufferHeightKey, cf_height);
  CFDictionarySetValue(
      image_config, kCVPixelBufferOpenGLCompatibilityKey, kCFBooleanTrue);

  // TODO(sandersd): Skip if the session is compatible.
  // TODO(sandersd): Flush frames when resetting.
  session_.reset();
  CHECK(!VTDecompressionSessionCreate(
      kCFAllocatorDefault,
      format_,              // video_format_description
      decoder_config,       // video_decoder_specification
      image_config,         // destination_image_buffer_attributes
      &callback_,           // output_callback
      session_.InitializeInto()
  ));
  DVLOG(2) << "Created VTDecompressionSession";
}

void VTVideoDecodeAccelerator::Decode(const media::BitstreamBuffer& bitstream) {
  DCHECK(CalledOnValidThread());
  decoder_thread_.message_loop_proxy()->PostTask(FROM_HERE, base::Bind(
      &VTVideoDecodeAccelerator::DecodeTask, base::Unretained(this),
      bitstream));
}

void VTVideoDecodeAccelerator::DecodeTask(
    const media::BitstreamBuffer bitstream) {
  DCHECK(decoder_thread_.message_loop_proxy()->BelongsToCurrentThread());

  // Map the bitstream buffer.
  base::SharedMemory memory(bitstream.handle(), true);
  size_t size = bitstream.size();
  CHECK(memory.Map(size));
  const uint8_t* buf = static_cast<uint8_t*>(memory.memory());

  // Locate relevant NALUs in the buffer.
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
    if (nalu.nal_unit_type == media::H264NALU::kSPS ||
        nalu.nal_unit_type == media::H264NALU::kPPS ||
        nalu.nal_unit_type == media::H264NALU::kSPSExt) {
      config_nalu_data_ptrs.push_back(nalu.data);
      config_nalu_data_sizes.push_back(nalu.size);
    }
    nalus.push_back(nalu);
    // Each NALU will have a 4-byte length header prepended.
    data_size += kNALUHeaderLength + nalu.size;
  }

  if (!config_nalu_data_ptrs.empty())
    ConfigureDecoder(config_nalu_data_ptrs, config_nalu_data_sizes);

  // TODO(sandersd): Rewrite slice NALU headers and send for decoding.
}

// This method may be called on any VideoToolbox thread.
void VTVideoDecodeAccelerator::Output(
    int32_t bitstream_id,
    OSStatus status,
    VTDecodeInfoFlags info_flags,
    CVImageBufferRef image_buffer) {
  // TODO(sandersd): Store the frame in a queue.
  CFRelease(image_buffer);
}

void VTVideoDecodeAccelerator::AssignPictureBuffers(
    const std::vector<media::PictureBuffer>& pictures) {
  DCHECK(CalledOnValidThread());
}

void VTVideoDecodeAccelerator::ReusePictureBuffer(int32_t picture_id) {
  DCHECK(CalledOnValidThread());
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
