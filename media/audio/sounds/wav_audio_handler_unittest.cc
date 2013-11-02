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
  ASSERT_EQ(static_cast<uint16>(2), handler.num_channels());
  ASSERT_EQ(static_cast<uint16>(16), handler.bits_per_sample());
  ASSERT_EQ(static_cast<uint32>(48000), handler.sample_rate());
  ASSERT_EQ(static_cast<uint32>(96000), handler.byte_rate());

  ASSERT_EQ(4, handler.size());
  scoped_ptr<AudioBus> bus = AudioBus::Create(
      handler.num_channels(),
      handler.size() / handler.num_channels());
  size_t bytes_written;
  ASSERT_TRUE(handler.CopyTo(bus.get(), 0, &bytes_written));
  ASSERT_EQ(static_cast<size_t>(handler.size()), bytes_written);
}

}  // namespace media
