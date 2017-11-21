// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_video_frame_adapter.h"

#include "base/logging.h"
#include "third_party/webrtc/common_video/include/video_frame_buffer.h"
#include "third_party/webrtc/rtc_base/refcountedobject.h"

namespace {

template <typename Base>
class FrameAdapter : public Base {
 public:
  explicit FrameAdapter(const scoped_refptr<media::VideoFrame>& frame)
      : frame_(std::move(frame)) {}

 private:
  int width() const override { return frame_->visible_rect().width(); }
  int height() const override { return frame_->visible_rect().height(); }

  const uint8_t* DataY() const override {
    return frame_->visible_data(media::VideoFrame::kYPlane);
  }

  const uint8_t* DataU() const override {
    return frame_->visible_data(media::VideoFrame::kUPlane);
  }

  const uint8_t* DataV() const override {
    return frame_->visible_data(media::VideoFrame::kVPlane);
  }

  int StrideY() const override {
    return frame_->stride(media::VideoFrame::kYPlane);
  }

  int StrideU() const override {
    return frame_->stride(media::VideoFrame::kUPlane);
  }

  int StrideV() const override {
    return frame_->stride(media::VideoFrame::kVPlane);
  }

  scoped_refptr<media::VideoFrame> frame_;
};

template <typename BaseWithA>
class FrameAdapterWithA : public FrameAdapter<BaseWithA> {
 public:
  FrameAdapterWithA(const scoped_refptr<media::VideoFrame>& frame)
      : FrameAdapter<BaseWithA>(std::move(frame)), frame_(std::move(frame)) {}

 private:
  const uint8_t* DataA() const override {
    return frame_->visible_data(media::VideoFrame::kAPlane);
  }

  int StrideA() const override {
    return frame_->stride(media::VideoFrame::kAPlane);
  }

  scoped_refptr<media::VideoFrame> frame_;
};

void IsValidFrame(const scoped_refptr<media::VideoFrame>& frame) {
  // Paranoia checks.
  DCHECK(frame);
  DCHECK(media::VideoFrame::IsValidConfig(
      frame->format(), frame->storage_type(), frame->coded_size(),
      frame->visible_rect(), frame->natural_size()));
  DCHECK(media::PIXEL_FORMAT_I420 == frame->format() ||
         media::PIXEL_FORMAT_YV12 == frame->format() ||
         media::PIXEL_FORMAT_YV12A == frame->format());
  CHECK(reinterpret_cast<void*>(frame->data(media::VideoFrame::kYPlane)));
  CHECK(reinterpret_cast<void*>(frame->data(media::VideoFrame::kUPlane)));
  CHECK(reinterpret_cast<void*>(frame->data(media::VideoFrame::kVPlane)));
  CHECK(frame->stride(media::VideoFrame::kYPlane));
  CHECK(frame->stride(media::VideoFrame::kUPlane));
  CHECK(frame->stride(media::VideoFrame::kVPlane));
}

}  // anonymous namespace

namespace content {

WebRtcVideoFrameAdapter::WebRtcVideoFrameAdapter(
    const scoped_refptr<media::VideoFrame>& frame,
    const CopyTextureFrameCallback& copy_texture_callback)
    : frame_(frame), copy_texture_callback_(copy_texture_callback) {}

WebRtcVideoFrameAdapter::~WebRtcVideoFrameAdapter() {
}

webrtc::VideoFrameBuffer::Type WebRtcVideoFrameAdapter::type() const {
  return Type::kNative;
}

int WebRtcVideoFrameAdapter::width() const {
  return frame_->visible_rect().width();
}

int WebRtcVideoFrameAdapter::height() const {
  return frame_->visible_rect().height();
}

rtc::scoped_refptr<webrtc::I420BufferInterface>
WebRtcVideoFrameAdapter::ToI420() {
  if (frame_->HasTextures()) {
    if (copy_texture_callback_.is_null()) {
      DLOG(ERROR) << "Texture backed frame cannot be copied.";
      return nullptr;
    }

    scoped_refptr<media::VideoFrame> new_frame;
    copy_texture_callback_.Run(frame_, &new_frame);
    if (!new_frame)
      return nullptr;
    frame_ = new_frame;
  }

  IsValidFrame(frame_);
  if (media::PIXEL_FORMAT_YV12A == frame_->format()) {
    return new rtc::RefCountedObject<
        FrameAdapterWithA<webrtc::I420ABufferInterface>>(frame_);
  }
  return new rtc::RefCountedObject<FrameAdapter<webrtc::I420BufferInterface>>(
      frame_);
}

}  // namespace content
