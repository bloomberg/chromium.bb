// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/mac_video_decode_accelerator.h"

#include "base/bind.h"
#include "base/file_path.h"
#import "base/mac/foundation_util.h"
#import "base/memory/ref_counted_memory.h"
#import "base/message_loop.h"
#include "base/location.h"
#include "base/native_library.h"
#include "ui/surface/io_surface_support_mac.h"
#include "ui/gfx/video_decode_acceleration_support_mac.h"

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
    media::VideoDecodeAccelerator::Client* client)
    : client_(client),
      cgl_context_(NULL),
      nalu_len_field_size_(0),
      did_request_pictures_(false) {
}

void MacVideoDecodeAccelerator::SetCGLContext(CGLContextObj cgl_context) {
  DCHECK(CalledOnValidThread());
  cgl_context_ = cgl_context;
}

bool MacVideoDecodeAccelerator::SetConfigInfo(
    uint32_t frame_width,
    uint32_t frame_height,
    const std::vector<uint8_t>& avc_data) {
  DCHECK(CalledOnValidThread());
  frame_size_ = gfx::Size(frame_width, frame_height);
  nalu_len_field_size_ = (avc_data[4] & 0x03) + 1;

  DCHECK(!vda_support_.get());
  vda_support_ = new gfx::VideoDecodeAccelerationSupport();
  gfx::VideoDecodeAccelerationSupport::Status status = vda_support_->Create(
      frame_size_.width(), frame_size_.height(), kCVPixelFormatType_422YpCbCr8,
      &avc_data.front(), avc_data.size());
  RETURN_ON_FAILURE(status == gfx::VideoDecodeAccelerationSupport::SUCCESS,
                    "Creating video decoder failed with error: " << status,
                    PLATFORM_FAILURE, false);
  return true;
}

bool MacVideoDecodeAccelerator::Initialize(media::VideoCodecProfile profile) {
  DCHECK(CalledOnValidThread());

  IOSurfaceSupport* io_surface_support = IOSurfaceSupport::Initialize();
  if (!io_surface_support)
    return false;

  MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
      &MacVideoDecodeAccelerator::NotifyInitializeDone, this));
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

  size_t buffer_size = bitstream_buffer.size();
  RETURN_ON_FAILURE(buffer_size > nalu_len_field_size_,
                    "Bitstream contains invalid data.", INVALID_ARGUMENT,);

  // The decoder can only handle slice types 1-5.
  const uint8_t* buffer = static_cast<const uint8_t*>(memory.memory());
  uint8_t nalu_type = buffer[nalu_len_field_size_] & 0x1f;
  if (nalu_type < 1 || nalu_type > 5) {
    MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
        &MacVideoDecodeAccelerator::NotifyInputBufferRead, this,
        bitstream_buffer.id()));
    return;
  }

  // Keep a ref counted copy of the buffer.
  std::vector<uint8_t> vbuffer(buffer, buffer + buffer_size);
  scoped_refptr<base::RefCountedBytes> bytes(
      base::RefCountedBytes::TakeVector(&vbuffer));

  // Store the buffer size at the beginning of the buffer as the decoder
  // expects.
  size_t frame_buffer_size = buffer_size - nalu_len_field_size_;
  const uint64_t max_frame_buffer_size =
      (1llu << (nalu_len_field_size_ * 8)) - 1;
  DCHECK_LE(nalu_len_field_size_, 4u);
  RETURN_ON_FAILURE(frame_buffer_size <= max_frame_buffer_size,
                    "Bitstream buffer is too large.", INVALID_ARGUMENT,);
  for (size_t i = 0; i < nalu_len_field_size_; ++i) {
    size_t shift = nalu_len_field_size_ * 8 - (i + 1) * 8;
    bytes->data()[i] = (frame_buffer_size >> shift) & 0xff;
  }

  vda_support_->Decode(bytes->front(), bytes->size(),
                       base::Bind(&MacVideoDecodeAccelerator::OnFrameReady,
                                  this, bitstream_buffer.id(), bytes));

  if (!did_request_pictures_) {
    did_request_pictures_ = true;
    MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
        &MacVideoDecodeAccelerator::RequestPictures, this));
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
  RETURN_ON_FAILURE(client_,
                    "Call to Flush() during invalid state.", ILLEGAL_STATE,);
  vda_support_->Flush(true);
  MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
      &MacVideoDecodeAccelerator::NotifyFlushDone, this));
}

void MacVideoDecodeAccelerator::Reset() {
  DCHECK(CalledOnValidThread());
  RETURN_ON_FAILURE(client_,
                    "Call to Reset() during invalid state.", ILLEGAL_STATE,);
  vda_support_->Flush(false);
  MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
      &MacVideoDecodeAccelerator::NotifyResetDone, this));
}

void MacVideoDecodeAccelerator::Destroy() {
  DCHECK(CalledOnValidThread());
  if (vda_support_) {
    vda_support_->Destroy();
    vda_support_ = NULL;
  }
  client_ = NULL;
  decoded_images_.clear();
}

MacVideoDecodeAccelerator::~MacVideoDecodeAccelerator() {
  DCHECK(CalledOnValidThread());
  Destroy();
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
  // TODO(sail): this assumes Decode() is handed a single NALU at a time. Make
  // that assumption go away.
  client_->NotifyEndOfBitstreamBuffer(bitstream_buffer_id);
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
  Destroy();
}

void MacVideoDecodeAccelerator::NotifyInitializeDone() {
  if (client_)
    client_->NotifyInitializeDone();
}

void MacVideoDecodeAccelerator::RequestPictures() {
  if (client_)
    client_->ProvidePictureBuffers(kNumPictureBuffers, frame_size_);
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
