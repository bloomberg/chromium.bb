// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/simple_cdm_allocator.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "media/base/video_frame.h"
#include "media/cdm/cdm_helpers.h"
#include "media/cdm/simple_cdm_buffer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

namespace {

class SimpleCdmVideoFrame : public VideoFrameImpl {
 public:
  SimpleCdmVideoFrame() = default;
  ~SimpleCdmVideoFrame() final = default;

  // VideoFrameImpl implementation.
  scoped_refptr<media::VideoFrame> TransformToVideoFrame(
      gfx::Size natural_size) final {
    DCHECK(FrameBuffer());

    cdm::Buffer* buffer = FrameBuffer();
    gfx::Size frame_size(Size().width, Size().height);
    scoped_refptr<media::VideoFrame> frame =
        media::VideoFrame::WrapExternalYuvData(
            PIXEL_FORMAT_YV12, frame_size, gfx::Rect(frame_size), natural_size,
            Stride(kYPlane), Stride(kUPlane), Stride(kVPlane),
            buffer->Data() + PlaneOffset(kYPlane),
            buffer->Data() + PlaneOffset(kUPlane),
            buffer->Data() + PlaneOffset(kVPlane),
            base::TimeDelta::FromMicroseconds(Timestamp()));

    // The FrameBuffer needs to remain around until |frame| is destroyed.
    frame->AddDestructionObserver(
        base::Bind(&cdm::Buffer::Destroy, base::Unretained(buffer)));

    // Clear FrameBuffer so that SimpleCdmVideoFrame no longer has a reference
    // to it.
    SetFrameBuffer(nullptr);
    return frame;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SimpleCdmVideoFrame);
};

}  // namespace

SimpleCdmAllocator::SimpleCdmAllocator() = default;

SimpleCdmAllocator::~SimpleCdmAllocator() = default;

// Creates a new SimpleCdmBuffer on every request. It does not keep track of
// the memory allocated, so the caller is responsible for calling Destroy()
// on the buffer when it is no longer needed.
cdm::Buffer* SimpleCdmAllocator::CreateCdmBuffer(size_t capacity) {
  if (!capacity)
    return nullptr;

  return SimpleCdmBuffer::Create(capacity);
}

// Creates a new SimpleCdmVideoFrame on every request.
std::unique_ptr<VideoFrameImpl> SimpleCdmAllocator::CreateCdmVideoFrame() {
  return base::MakeUnique<SimpleCdmVideoFrame>();
}

}  // namespace media
