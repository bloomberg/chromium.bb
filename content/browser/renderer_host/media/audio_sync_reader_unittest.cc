// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_sync_reader.h"

#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

#include "base/memory/shared_memory.h"
#include "base/sync_socket.h"
#include "base/time/time.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::TestWithParam;

namespace content {

static_assert(
    std::is_unsigned<decltype(
        media::AudioOutputBufferParameters::bitstream_data_size)>::value,
    "If |bitstream_data_size| is ever made signed, add tests for negative "
    "buffer sizes.");
enum OverflowTestCase {
  kZero,
  kNoOverflow,
  kOverflowByOne,
  kOverflowByOneThousand,
  kOverflowByMax
};

static const OverflowTestCase overflow_test_case_values[]{
    kZero, kNoOverflow, kOverflowByOne, kOverflowByOneThousand, kOverflowByMax};

class AudioSyncReaderBitstreamTest : public TestWithParam<OverflowTestCase> {
 public:
  AudioSyncReaderBitstreamTest() {}
  ~AudioSyncReaderBitstreamTest() override {}

 private:
  TestBrowserThreadBundle threads;
};

TEST_P(AudioSyncReaderBitstreamTest, BitstreamBufferOverflow_DoesNotWriteOOB) {
  const int kSampleRate = 44100;
  const int kBitsPerSample = 32;
  const int kFramesPerBuffer = 1;
  media::AudioParameters params(media::AudioParameters::AUDIO_BITSTREAM_AC3,
                                media::CHANNEL_LAYOUT_STEREO, kSampleRate,
                                kBitsPerSample, kFramesPerBuffer);

  auto socket = std::make_unique<base::CancelableSyncSocket>();
  std::unique_ptr<media::AudioBus> output_bus = media::AudioBus::Create(params);
  std::unique_ptr<AudioSyncReader> reader =
      AudioSyncReader::Create(params, socket.get());
  const base::SharedMemory* shmem = reader->shared_memory();
  media::AudioOutputBuffer* buffer =
      reinterpret_cast<media::AudioOutputBuffer*>(shmem->memory());
  reader->RequestMoreData(base::TimeDelta(), base::TimeTicks(), 0);

  uint32_t signal;
  EXPECT_EQ(socket->Receive(&signal, sizeof(signal)), sizeof(signal));

  // So far, this is an ordinary stream.
  // Now |reader| expects data to be writted to the shared memory. The renderer
  // says how much data was written.
  switch (GetParam()) {
    case kZero:
      buffer->params.bitstream_data_size = 0;
      break;
    case kNoOverflow:
      buffer->params.bitstream_data_size =
          shmem->mapped_size() - sizeof(media::AudioOutputBufferParameters);
      break;
    case kOverflowByOne:
      buffer->params.bitstream_data_size =
          shmem->mapped_size() - sizeof(media::AudioOutputBufferParameters) + 1;
      break;
    case kOverflowByOneThousand:
      buffer->params.bitstream_data_size =
          shmem->mapped_size() - sizeof(media::AudioOutputBufferParameters) +
          1000;
      break;
    case kOverflowByMax:
      buffer->params.bitstream_data_size = std::numeric_limits<decltype(
          buffer->params.bitstream_data_size)>::max();
      break;
  }

  ++signal;
  EXPECT_EQ(socket->Send(&signal, sizeof(signal)), sizeof(signal));

  // The purpose of the test is to ensure this call doesn't result in undefined
  // behavior, which should be verified by sanitizers.
  reader->Read(output_bus.get());
}

INSTANTIATE_TEST_CASE_P(AudioSyncReaderTest,
                        AudioSyncReaderBitstreamTest,
                        ::testing::ValuesIn(overflow_test_case_values));

}  // namespace content
