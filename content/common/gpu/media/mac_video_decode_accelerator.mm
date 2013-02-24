// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/mac_video_decode_accelerator.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#import "base/mac/foundation_util.h"
#import "base/memory/ref_counted_memory.h"
#import "base/message_loop.h"
#include "base/native_library.h"
#include "ui/gfx/video_decode_acceleration_support_mac.h"
#include "ui/surface/io_surface_support_mac.h"

namespace content {

// Helper macros for dealing with failure.  If |result| evaluates false, emit
// |log| to ERROR, register |error| with the decoder, and return |ret_val|
// (which may be omitted for functions that return void).
#define RETURN_ON_FAILURE(result, log, error, ret_val)             \
  do {                                                             \
    if (!(result)) {                                               \
      DLOG(ERROR) << log;                                          \
      StopOnError(error);                                          \
      return ret_val;                                              \
    }                                                              \
  } while (0)

namespace {

enum { kNumPictureBuffers = 4 };

class ScopedContextSetter {
 public:
  ScopedContextSetter(CGLContextObj context)
      : old_context_(NULL),
        did_succeed_(false) {
    old_context_ = CGLGetCurrentContext();
    did_succeed_ = CGLSetCurrentContext(context) == kCGLNoError;
  }

  ~ScopedContextSetter() {
    if (did_succeed_)
      CGLSetCurrentContext(old_context_);
  }

  bool did_succeed() const {
    return did_succeed_;
  }

 private:
  CGLContextObj old_context_;
  bool did_succeed_;
};

}  // namespace

static bool BindImageToTexture(CGLContextObj context,
                               CVImageBufferRef image,
                               uint32 texture_id) {
  ScopedContextSetter scoped_context_setter(context);
  if (!scoped_context_setter.did_succeed()) {
    DVLOG(1) << "Unable to set OpenGL context.";
    return false;
  }

  IOSurfaceSupport* io_surface_support = IOSurfaceSupport::Initialize();
  DCHECK(io_surface_support);

  CFTypeRef io_surface =
      io_surface_support->CVPixelBufferGetIOSurface(image);
  if (!io_surface) {
    DVLOG(1) << "Unable to get IOSurface for CVPixelBuffer.";
    return false;
  }

  glEnable(GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture_id);
  if (io_surface_support->CGLTexImageIOSurface2D(
          context,
          GL_TEXTURE_RECTANGLE_ARB,
          GL_RGB,
          io_surface_support->IOSurfaceGetWidth(io_surface),
          io_surface_support->IOSurfaceGetHeight(io_surface),
          GL_YCBCR_422_APPLE,
          GL_UNSIGNED_SHORT_8_8_APPLE,
          io_surface,
          0) != kCGLNoError) {
    DVLOG(1) << "Failed to bind image to texture.";
    return false;
  }
  return glGetError() == GL_NO_ERROR;
}

MacVideoDecodeAccelerator::MacVideoDecodeAccelerator(
    CGLContextObj cgl_context, media::VideoDecodeAccelerator::Client* client)
    : client_(client),
      cgl_context_(cgl_context),
      did_build_config_record_(false) {
}

bool MacVideoDecodeAccelerator::Initialize(media::VideoCodecProfile profile) {
  DCHECK(CalledOnValidThread());

  // MacVDA still fails on too many videos to be useful, even to users who
  // ignore the GPU blacklist.  Fail unconditionally here until enough of
  // crbug.com/133828's blockers are resolved.
  if (true)
    return false;

  if (profile < media::H264PROFILE_MIN || profile > media::H264PROFILE_MAX)
    return false;

  IOSurfaceSupport* io_surface_support = IOSurfaceSupport::Initialize();
  if (!io_surface_support)
    return false;

  MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
      &MacVideoDecodeAccelerator::NotifyInitializeDone, base::AsWeakPtr(this)));
  return true;
}

void MacVideoDecodeAccelerator::Decode(
    const media::BitstreamBuffer& bitstream_buffer) {
  DCHECK(CalledOnValidThread());
  RETURN_ON_FAILURE(client_,
                    "Call to Decode() during invalid state.", ILLEGAL_STATE,);

  base::SharedMemory memory(bitstream_buffer.handle(), true);
  RETURN_ON_FAILURE(memory.Map(bitstream_buffer.size()),
                    "Failed to SharedMemory::Map().", UNREADABLE_INPUT,);

  h264_parser_.SetStream(static_cast<const uint8_t*>(memory.memory()),
                         bitstream_buffer.size());
  while (true) {
    H264NALU nalu;
    H264Parser::Result result = h264_parser_.AdvanceToNextNALU(&nalu);
    if (result == H264Parser::kEOStream) {
      if (bitstream_nalu_count_.count(bitstream_buffer.id()) == 0) {
        MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
            &MacVideoDecodeAccelerator::NotifyInputBufferRead,
            base::AsWeakPtr(this), bitstream_buffer.id()));
      }
      return;
    }
    RETURN_ON_FAILURE(result == H264Parser::kOk,
                      "Unable to parse bitstream.", UNREADABLE_INPUT,);
    if (!did_build_config_record_) {
      std::vector<uint8_t> config_record;
      RETURN_ON_FAILURE(config_record_builder_.ProcessNALU(&h264_parser_, nalu,
                                                           &config_record),
                        "Unable to build AVC configuraiton record.",
                        UNREADABLE_INPUT,);
      if (!config_record.empty()) {
        did_build_config_record_ = true;
        if (!CreateDecoder(config_record))
          return;
      }
    }
    // If the decoder has been created and this is a slice type then pass it
    // to the decoder.
    if (vda_support_.get() && nalu.nal_unit_type >= 1 &&
        nalu.nal_unit_type <= 5) {
      bitstream_nalu_count_[bitstream_buffer.id()]++;
      DecodeNALU(nalu, bitstream_buffer.id());
    }
  }
}

void MacVideoDecodeAccelerator::AssignPictureBuffers(
    const std::vector<media::PictureBuffer>& buffers) {
  DCHECK(CalledOnValidThread());
  RETURN_ON_FAILURE(client_,
                    "Call to AssignPictureBuffers() during invalid state.",
                    ILLEGAL_STATE,);
  available_pictures_.insert(
      available_pictures_.end(), buffers.begin(), buffers.end());
  SendImages();
}

void MacVideoDecodeAccelerator::ReusePictureBuffer(int32 picture_buffer_id) {
  DCHECK(CalledOnValidThread());
  RETURN_ON_FAILURE(client_,
                    "Call to ReusePictureBuffe() during invalid state.",
                    ILLEGAL_STATE,);

  std::map<int32, UsedPictureInfo>::iterator it =
      used_pictures_.find(picture_buffer_id);
  RETURN_ON_FAILURE(it != used_pictures_.end(),
                    "Missing picture buffer id: " << picture_buffer_id,
                    INVALID_ARGUMENT,);
  UsedPictureInfo info = it->second;
  used_pictures_.erase(it);
  available_pictures_.push_back(info.picture_buffer);
  SendImages();
}

void MacVideoDecodeAccelerator::Flush() {
  DCHECK(CalledOnValidThread());
  RETURN_ON_FAILURE(vda_support_,
                    "Call to Flush() during invalid state.", ILLEGAL_STATE,);
  vda_support_->Flush(true);
  MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
      &MacVideoDecodeAccelerator::NotifyFlushDone, base::AsWeakPtr(this)));
}

void MacVideoDecodeAccelerator::Reset() {
  DCHECK(CalledOnValidThread());
  RETURN_ON_FAILURE(vda_support_,
                    "Call to Reset() during invalid state.", ILLEGAL_STATE,);
  vda_support_->Flush(false);
  MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
      &MacVideoDecodeAccelerator::NotifyResetDone, base::AsWeakPtr(this)));
}

void MacVideoDecodeAccelerator::Cleanup() {
  DCHECK(CalledOnValidThread());
  if (vda_support_) {
    vda_support_->Destroy();
    vda_support_ = NULL;
  }
  client_ = NULL;
  decoded_images_.clear();
}

void MacVideoDecodeAccelerator::Destroy() {
  DCHECK(CalledOnValidThread());
  Cleanup();
  delete this;
}

MacVideoDecodeAccelerator::~MacVideoDecodeAccelerator() {
  DCHECK(CalledOnValidThread());
  DCHECK(!vda_support_);
  DCHECK(!client_);
  DCHECK(decoded_images_.empty());
}

void MacVideoDecodeAccelerator::OnFrameReady(
    int32 bitstream_buffer_id,
    scoped_refptr<base::RefCountedBytes> bytes,
    CVImageBufferRef image,
    int status) {
  DCHECK(CalledOnValidThread());
  RETURN_ON_FAILURE(status == noErr,
                    "Decoding image failed with error code: " << status,
                    PLATFORM_FAILURE,);
  if (!client_)
    return;
  if (image) {
    DecodedImageInfo info;
    info.image.reset(image, base::scoped_policy::RETAIN);
    info.bitstream_buffer_id = bitstream_buffer_id;
    decoded_images_.push_back(info);
    SendImages();
  }
  std::map<int32, int>::iterator bitstream_count_it =
      bitstream_nalu_count_.find(bitstream_buffer_id);
  if (--bitstream_count_it->second == 0) {
    bitstream_nalu_count_.erase(bitstream_count_it);
    MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
        &MacVideoDecodeAccelerator::NotifyInputBufferRead,
        base::AsWeakPtr(this), bitstream_buffer_id));
  }
}

void MacVideoDecodeAccelerator::SendImages() {
  if (!client_) {
    DCHECK(decoded_images_.empty());
    return;
  }

  while (available_pictures_.size() && decoded_images_.size()) {
    DecodedImageInfo info = decoded_images_.front();
    decoded_images_.pop_front();
    media::PictureBuffer picture_buffer = available_pictures_.front();
    available_pictures_.pop_front();

    RETURN_ON_FAILURE(BindImageToTexture(
        cgl_context_, info.image, picture_buffer.texture_id()),
        "Error binding image to texture.", PLATFORM_FAILURE,);

    used_pictures_.insert(std::make_pair(
        picture_buffer.id(), UsedPictureInfo(picture_buffer, info.image)));
    client_->PictureReady(
        media::Picture(picture_buffer.id(), info.bitstream_buffer_id));
  }
}

void MacVideoDecodeAccelerator::StopOnError(
    media::VideoDecodeAccelerator::Error error) {
  if (client_)
    client_->NotifyError(error);
  Cleanup();
}

bool MacVideoDecodeAccelerator::CreateDecoder(
    const std::vector<uint8_t>& extra_data) {
  DCHECK(client_);
  DCHECK(!vda_support_.get());

  vda_support_ = new gfx::VideoDecodeAccelerationSupport();
  gfx::VideoDecodeAccelerationSupport::Status status = vda_support_->Create(
      config_record_builder_.coded_width(),
      config_record_builder_.coded_height(),
      kCVPixelFormatType_422YpCbCr8,
      &extra_data[0], extra_data.size());
  RETURN_ON_FAILURE(status == gfx::VideoDecodeAccelerationSupport::SUCCESS,
                    "Creating video decoder failed with error: " << status,
                    PLATFORM_FAILURE, false);

  MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
      &MacVideoDecodeAccelerator::RequestPictures, base::AsWeakPtr(this)));
  return true;
}

void MacVideoDecodeAccelerator::DecodeNALU(const H264NALU& nalu,
                                           int32 bitstream_buffer_id) {
  // Assume the NALU length field size is 4 bytes.
  const int kNALULengthFieldSize = 4;
  std::vector<uint8_t> data(kNALULengthFieldSize + nalu.size);

  // Store the buffer size at the beginning of the buffer as the decoder
  // expects.
  for (size_t i = 0; i < kNALULengthFieldSize; ++i) {
    size_t shift = kNALULengthFieldSize * 8 - (i + 1) * 8;
    data[i] = (nalu.size >> shift) & 0xff;
  }

  // Copy the NALU data.
  memcpy(&data[kNALULengthFieldSize], nalu.data, nalu.size);

  // Keep a ref counted copy of the buffer.
  scoped_refptr<base::RefCountedBytes> bytes(
      base::RefCountedBytes::TakeVector(&data));
  vda_support_->Decode(bytes->front(), bytes->size(), base::Bind(
      &MacVideoDecodeAccelerator::OnFrameReady,
      base::AsWeakPtr(this), bitstream_buffer_id, bytes));
}

void MacVideoDecodeAccelerator::NotifyInitializeDone() {
  if (client_)
    client_->NotifyInitializeDone();
}

void MacVideoDecodeAccelerator::RequestPictures() {
  if (client_) {
    client_->ProvidePictureBuffers(
        kNumPictureBuffers,
        gfx::Size(config_record_builder_.coded_width(),
                  config_record_builder_.coded_height()),
        GL_TEXTURE_RECTANGLE_ARB);
  }
}

void MacVideoDecodeAccelerator::NotifyFlushDone() {
  if (client_)
    client_->NotifyFlushDone();
}

void MacVideoDecodeAccelerator::NotifyResetDone() {
  decoded_images_.clear();
  if (client_)
    client_->NotifyResetDone();
}

void MacVideoDecodeAccelerator::NotifyInputBufferRead(int input_buffer_id) {
  if (client_)
    client_->NotifyEndOfBitstreamBuffer(input_buffer_id);
}

MacVideoDecodeAccelerator::UsedPictureInfo::UsedPictureInfo(
    const media::PictureBuffer& pic,
    const base::mac::ScopedCFTypeRef<CVImageBufferRef>& image)
    : picture_buffer(pic),
      image(image, base::scoped_policy::RETAIN) {
}

MacVideoDecodeAccelerator::UsedPictureInfo::~UsedPictureInfo() {
}

MacVideoDecodeAccelerator::DecodedImageInfo::DecodedImageInfo() {
}

MacVideoDecodeAccelerator::DecodedImageInfo::~DecodedImageInfo() {
}

}  // namespace content
