// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_FAKE_MOCK_VIDEO_ENCODE_ACCELERATOR_H_
#define MEDIA_CAST_TEST_FAKE_MOCK_VIDEO_ENCODE_ACCELERATOR_H_

#include "media/video/video_encode_accelerator.h"

#include <list>

#include "media/base/bitstream_buffer.h"

namespace media {
namespace cast {
namespace test {

class FakeVideoEncodeAccelerator : public VideoEncodeAccelerator {
 public:
  explicit FakeVideoEncodeAccelerator(VideoEncodeAccelerator::Client* client);

  virtual void Initialize(media::VideoFrame::Format input_format,
                          const gfx::Size& input_visible_size,
                          VideoCodecProfile output_profile,
                          uint32 initial_bitrate) OVERRIDE;

  virtual void Encode(const scoped_refptr<VideoFrame>& frame,
                      bool force_keyframe) OVERRIDE;

  virtual void UseOutputBitstreamBuffer(const BitstreamBuffer& buffer) OVERRIDE;

  virtual void RequestEncodingParametersChange(uint32 bitrate,
                                               uint32 framerate) OVERRIDE;

  virtual void Destroy() OVERRIDE;

 private:
  virtual ~FakeVideoEncodeAccelerator();

  VideoEncodeAccelerator::Client* client_;
  bool first_;

  std::list<int32> available_buffer_ids_;

  DISALLOW_COPY_AND_ASSIGN(FakeVideoEncodeAccelerator);
};

}  // namespace test
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TEST_FAKE_MOCK_VIDEO_ENCODE_ACCELERATOR_H_
