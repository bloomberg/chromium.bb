// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_VIDEO_SENDER_MOCK_VIDEO_ENCODER_CONTROLLER_H_
#define MEDIA_CAST_VIDEO_SENDER_MOCK_VIDEO_ENCODER_CONTROLLER_H_

#include "media/cast/cast_config.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

class MockVideoEncoderController : public VideoEncoderController {
 public:
  MockVideoEncoderController();
  virtual ~MockVideoEncoderController();

  MOCK_METHOD1(SetBitRate, void(int new_bit_rate));

  MOCK_METHOD1(SkipNextFrame, void(bool skip_next_frame));

  MOCK_METHOD0(GenerateKeyFrame, void());

  MOCK_METHOD1(LatestFrameIdToReference, void(uint32 frame_id));

  MOCK_CONST_METHOD0(NumberOfSkippedFrames, int());
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_VIDEO_SENDER_MOCK_VIDEO_ENCODER_CONTROLLER_H_

