// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "media/audio/sounds/test_data.h"
#include "media/audio/sounds/wav_audio_handler.h"
#include "media/base/audio_bus.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(WavAudioHandlerTest, SampleDataTest) {
  WavAudioHandler handler(base::StringPiece(kTestAudioData,
                                            arraysize(kTestAudioData)));
  ASSERT_EQ(2u, handler.num_channels());
  ASSERT_EQ(16u, handler.bits_per_sample());
  ASSERT_EQ(48000u, handler.sample_rate());
  ASSERT_EQ(1u, handler.total_frames());
  ASSERT_EQ(20u, handler.GetDuration().InMicroseconds());

  ASSERT_EQ(4U, handler.data().size());
  const char kData[] = "\x01\x00\x01\x00";
  ASSERT_EQ(base::StringPiece(kData, arraysize(kData) - 1), handler.data());

  scoped_ptr<AudioBus> bus = AudioBus::Create(
      handler.num_channels(), handler.data().size() / handler.num_channels());

  size_t bytes_written;
  ASSERT_TRUE(handler.CopyTo(bus.get(), 0, &bytes_written));
  ASSERT_EQ(static_cast<size_t>(handler.data().size()), bytes_written);
}

}  // namespace media
