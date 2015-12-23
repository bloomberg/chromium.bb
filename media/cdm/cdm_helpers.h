// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CDM_HELPERS_H_
#define MEDIA_CDM_CDM_HELPERS_H_

#include <stdint.h>

#include "base/macros.h"
#include "media/cdm/api/content_decryption_module.h"

namespace media {

class DecryptedBlockImpl : public cdm::DecryptedBlock {
 public:
  DecryptedBlockImpl();
  ~DecryptedBlockImpl() final;

  // cdm::DecryptedBlock implementation.
  void SetDecryptedBuffer(cdm::Buffer* buffer) final;
  cdm::Buffer* DecryptedBuffer() final;
  void SetTimestamp(int64_t timestamp) final;
  int64_t Timestamp() const final;

 private:
  cdm::Buffer* buffer_;
  int64_t timestamp_;

  DISALLOW_COPY_AND_ASSIGN(DecryptedBlockImpl);
};

class VideoFrameImpl : public cdm::VideoFrame {
 public:
  VideoFrameImpl();
  ~VideoFrameImpl() final;

  // cdm::VideoFrame implementation.
  void SetFormat(cdm::VideoFormat format) final;
  cdm::VideoFormat Format() const final;
  void SetSize(cdm::Size size) final;
  cdm::Size Size() const final;
  void SetFrameBuffer(cdm::Buffer* frame_buffer) final;
  cdm::Buffer* FrameBuffer() final;
  void SetPlaneOffset(cdm::VideoFrame::VideoPlane plane, uint32_t offset) final;
  uint32_t PlaneOffset(VideoPlane plane) final;
  void SetStride(VideoPlane plane, uint32_t stride) final;
  uint32_t Stride(VideoPlane plane) final;
  void SetTimestamp(int64_t timestamp) final;
  int64_t Timestamp() const final;

 private:
  // The video buffer format.
  cdm::VideoFormat format_;

  // Width and height of the video frame.
  cdm::Size size_;

  // The video frame buffer.
  cdm::Buffer* frame_buffer_;

  // Array of data pointers to each plane in the video frame buffer.
  uint32_t plane_offsets_[kMaxPlanes];

  // Array of strides for each plane, typically greater or equal to the width
  // of the surface divided by the horizontal sampling period.  Note that
  // strides can be negative.
  uint32_t strides_[kMaxPlanes];

  // Presentation timestamp in microseconds.
  int64_t timestamp_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameImpl);
};

class AudioFramesImpl : public cdm::AudioFrames {
 public:
  AudioFramesImpl();
  ~AudioFramesImpl() final;

  // cdm::AudioFrames implementation.
  void SetFrameBuffer(cdm::Buffer* buffer) final;
  cdm::Buffer* FrameBuffer() final;
  void SetFormat(cdm::AudioFormat format) final;
  cdm::AudioFormat Format() const final;

  cdm::Buffer* PassFrameBuffer();

 private:
  cdm::Buffer* buffer_;
  cdm::AudioFormat format_;

  DISALLOW_COPY_AND_ASSIGN(AudioFramesImpl);
};

}  // namespace media

#endif  // MEDIA_CDM_CDM_HELPERS_H_
