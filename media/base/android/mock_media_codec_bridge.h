// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MOCK_MEDIA_CODEC_BRIDGE_H_
#define MEDIA_BASE_ANDROID_MOCK_MEDIA_CODEC_BRIDGE_H_

#include "media/base/android/media_codec_bridge.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class MockMediaCodecBridge : public MediaCodecBridge {
 public:
  MockMediaCodecBridge();
  ~MockMediaCodecBridge() override;
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(Flush, MediaCodecStatus());
  MOCK_METHOD1(GetOutputSize, MediaCodecStatus(gfx::Size* size));
  MOCK_METHOD1(GetOutputSamplingRate, MediaCodecStatus(int* sampling_rate));
  MOCK_METHOD1(GetOutputChannelCount, MediaCodecStatus(int* channel_count));
  MOCK_METHOD4(QueueInputBuffer,
               MediaCodecStatus(int index,
                                const uint8_t* data,
                                size_t data_size,
                                base::TimeDelta presentation_time));
  MOCK_METHOD8(QueueSecureInputBuffer,
               MediaCodecStatus(int index,
                                const uint8_t* data,
                                size_t data_size,
                                const std::string& key_id,
                                const std::string& iv,
                                const std::vector<SubsampleEntry>& subsamples,
                                const EncryptionScheme& encryption_scheme,
                                base::TimeDelta presentation_time));
  MOCK_METHOD1(QueueEOS, void(int input_buffer_index));
  MOCK_METHOD2(DequeueInputBuffer,
               MediaCodecStatus(base::TimeDelta timeout, int* index));
  MOCK_METHOD7(DequeueOutputBuffer,
               MediaCodecStatus(base::TimeDelta timeout,
                                int* index,
                                size_t* offset,
                                size_t* size,
                                base::TimeDelta* presentation_time,
                                bool* end_of_stream,
                                bool* key_frame));
  MOCK_METHOD2(ReleaseOutputBuffer, void(int index, bool render));
  MOCK_METHOD3(GetInputBuffer,
               MediaCodecStatus(int input_buffer_index,
                                uint8_t** data,
                                size_t* capacity));
  MOCK_METHOD4(
      CopyFromOutputBuffer,
      MediaCodecStatus(int index, size_t offset, void* dst, size_t num));
  MOCK_METHOD0(GetName, std::string());
  MOCK_METHOD1(SetSurface, bool(jobject surface));
  MOCK_METHOD2(SetVideoBitrate, void(int bps, int frame_rate));
  MOCK_METHOD0(RequestKeyFrameSoon, void());
  MOCK_METHOD0(IsAdaptivePlaybackSupported, bool());
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MOCK_MEDIA_CODEC_BRIDGE_H_
