// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_message_queue.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace midi {
namespace {

const uint8 kGMOn[] = { 0xf0, 0x7e, 0x7f, 0x09, 0x01, 0xf7 };
const uint8 kPartialGMOn1st[] = { 0xf0 };
const uint8 kPartialGMOn2nd[] = { 0x7e, 0x7f, 0x09, 0x01 };
const uint8 kPartialGMOn3rd[] = { 0xf7 };
const uint8 kGSOn[] = {
  0xf0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7f, 0x00, 0x41, 0xf7,
};
const uint8 kNoteOn[] = { 0x90, 0x3c, 0x7f };
const uint8 kPartialNoteOn1st[] = { 0x90 };
const uint8 kPartialNoteOn2nd[] = { 0x3c };
const uint8 kPartialNoteOn3rd[] = { 0x7f };

const uint8 kNoteOnWithRunningStatus[] = {
  0x90, 0x3c, 0x7f, 0x3c, 0x7f, 0x3c, 0x7f,
};
const uint8 kChannelPressure[] = { 0xd0, 0x01 };
const uint8 kChannelPressureWithRunningStatus[] = {
  0xd0, 0x01, 0x01, 0x01,
};
const uint8 kTimingClock[] = { 0xf8 };
const uint8 kSystemCommonMessageTuneRequest[] = { 0xf6 };
const uint8 kMTCFrame[] = { 0xf1, 0x00 };
const uint8 kBrokenData1[] = { 0x92 };
const uint8 kBrokenData2[] = { 0xf7 };
const uint8 kBrokenData3[] = { 0xf2, 0x00 };
const uint8 kDataByte0[] = { 0x00 };

const uint8 kReservedMessage1[] = { 0xf4 };
const uint8 kReservedMessage2[] = { 0xf5 };
const uint8 kReservedMessage1WithDataBytes[] = {
  0xf4, 0x01, 0x01, 0x01, 0x01, 0x01
};

template <typename T, size_t N>
void Add(MidiMessageQueue* queue, const T(&array)[N]) {
  queue->Add(array, N);
}

template <typename T, size_t N>
::testing::AssertionResult ExpectEqualSequence(
    const char* expr1, const char* expr2,
    const T(&expected)[N], const std::vector<T>& actual) {
  if (actual.size() != N) {
    return ::testing::AssertionFailure()
        << "expected: " << ::testing::PrintToString(expected)
        << ", actual: " << ::testing::PrintToString(actual);
  }
  for (size_t i = 0; i < N; ++i) {
    if (expected[i] != actual[i]) {
      return ::testing::AssertionFailure()
          << "expected: " << ::testing::PrintToString(expected)
          << ", actual: " << ::testing::PrintToString(actual);
    }
  }
  return ::testing::AssertionSuccess();
}

#define EXPECT_MESSAGE(expected, actual)  \
  EXPECT_PRED_FORMAT2(ExpectEqualSequence, expected, actual)

TEST(MidiMessageQueueTest, EmptyData) {
  MidiMessageQueue queue(false);
  std::vector<uint8> message;
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
}

TEST(MidiMessageQueueTest, RunningStatusDisabled) {
  MidiMessageQueue queue(false);
  Add(&queue, kGMOn);
  Add(&queue, kBrokenData1);
  Add(&queue, kNoteOnWithRunningStatus);
  Add(&queue, kBrokenData2);
  Add(&queue, kChannelPressureWithRunningStatus);
  Add(&queue, kBrokenData3);
  Add(&queue, kNoteOn);
  Add(&queue, kBrokenData1);
  Add(&queue, kGSOn);
  Add(&queue, kBrokenData2);
  Add(&queue, kTimingClock);
  Add(&queue, kBrokenData3);

  std::vector<uint8> message;
  queue.Get(&message);
  EXPECT_MESSAGE(kGMOn, message);
  queue.Get(&message);
  EXPECT_MESSAGE(kNoteOn, message) << "Running status should be ignored";
  queue.Get(&message);
  EXPECT_MESSAGE(kChannelPressure, message)
      << "Running status should be ignored";
  queue.Get(&message);
  EXPECT_MESSAGE(kNoteOn, message);
  queue.Get(&message);
  EXPECT_MESSAGE(kGSOn, message);
  queue.Get(&message);
  EXPECT_MESSAGE(kTimingClock, message);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
}

TEST(MidiMessageQueueTest, RunningStatusEnabled) {
  MidiMessageQueue queue(true);
  Add(&queue, kGMOn);
  Add(&queue, kBrokenData1);
  Add(&queue, kNoteOnWithRunningStatus);
  Add(&queue, kBrokenData2);
  Add(&queue, kChannelPressureWithRunningStatus);
  Add(&queue, kBrokenData3);
  Add(&queue, kNoteOn);
  Add(&queue, kBrokenData1);
  Add(&queue, kGSOn);
  Add(&queue, kBrokenData2);
  Add(&queue, kTimingClock);
  Add(&queue, kDataByte0);

  std::vector<uint8> message;
  queue.Get(&message);
  EXPECT_MESSAGE(kGMOn, message);
  queue.Get(&message);
  EXPECT_MESSAGE(kNoteOn, message);
  queue.Get(&message);
  EXPECT_MESSAGE(kNoteOn, message)
      << "Running status should be converted into a canonical MIDI message";
  queue.Get(&message);
  EXPECT_MESSAGE(kNoteOn, message)
      << "Running status should be converted into a canonical MIDI message";
  queue.Get(&message);
  EXPECT_MESSAGE(kChannelPressure, message);
  queue.Get(&message);
  EXPECT_MESSAGE(kChannelPressure, message)
      << "Running status should be converted into a canonical MIDI message";
  queue.Get(&message);
  EXPECT_MESSAGE(kChannelPressure, message)
      << "Running status should be converted into a canonical MIDI message";
  queue.Get(&message);
  EXPECT_MESSAGE(kNoteOn, message);
  queue.Get(&message);
  EXPECT_MESSAGE(kGSOn, message);
  queue.Get(&message);
  EXPECT_MESSAGE(kTimingClock, message);
  queue.Get(&message);
  EXPECT_TRUE(message.empty())
      << "Running status must not be applied to real time messages";
}

TEST(MidiMessageQueueTest, RunningStatusEnabledWithRealTimeEvent) {
  MidiMessageQueue queue(true);
  const uint8 kNoteOnWithRunningStatusWithTimingClock[] = {
    0x90, 0xf8, 0x3c, 0xf8, 0x7f, 0xf8, 0x3c, 0xf8, 0x7f, 0xf8, 0x3c, 0xf8,
    0x7f,
  };
  Add(&queue, kNoteOnWithRunningStatusWithTimingClock);
  std::vector<uint8> message;
  queue.Get(&message);
  EXPECT_MESSAGE(kTimingClock, message);
  queue.Get(&message);
  EXPECT_MESSAGE(kTimingClock, message);
  queue.Get(&message);
  EXPECT_MESSAGE(kNoteOn, message);
  queue.Get(&message);
  EXPECT_MESSAGE(kTimingClock, message);
  queue.Get(&message);
  EXPECT_MESSAGE(kTimingClock, message);
  queue.Get(&message);
  EXPECT_MESSAGE(kNoteOn, message);
  queue.Get(&message);
  EXPECT_MESSAGE(kTimingClock, message);
  queue.Get(&message);
  EXPECT_MESSAGE(kTimingClock, message);
  queue.Get(&message);
  EXPECT_MESSAGE(kNoteOn, message);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
}

TEST(MidiMessageQueueTest, RunningStatusEnabledWithSystemCommonMessage) {
  MidiMessageQueue queue(true);
  const uint8 kNoteOnWithRunningStatusWithSystemCommonMessage[] = {
    0x90, 0x3c, 0x7f, 0xf1, 0x00, 0x3c, 0x7f, 0xf8, 0x90, 0x3c, 0x7f,
  };
  Add(&queue, kNoteOnWithRunningStatusWithSystemCommonMessage);
  std::vector<uint8> message;
  queue.Get(&message);
  EXPECT_MESSAGE(kNoteOn, message);
  queue.Get(&message);
  EXPECT_MESSAGE(kMTCFrame, message);
  queue.Get(&message);
  EXPECT_MESSAGE(kTimingClock, message) << "Running status should be reset";
  queue.Get(&message);
  EXPECT_MESSAGE(kNoteOn, message);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
}

TEST(MidiMessageQueueTest, Issue540016) {
  const uint8 kData[] = { 0xf4, 0x3a };
  MidiMessageQueue queue(false);
  Add(&queue, kData);
  std::vector<uint8> message;
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
}

TEST(MidiMessageQueueTest, ReconstructNonSysExMessage) {
  MidiMessageQueue queue(true);
  std::vector<uint8> message;

  Add(&queue, kPartialNoteOn1st);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());

  Add(&queue, kPartialNoteOn2nd);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());

  Add(&queue, kPartialNoteOn3rd);
  queue.Get(&message);
  EXPECT_MESSAGE(kNoteOn, message);

  queue.Get(&message);
  EXPECT_TRUE(message.empty());
}

TEST(MidiMessageQueueTest, ReconstructBrokenNonSysExMessage) {
  MidiMessageQueue queue(true);
  std::vector<uint8> message;

  Add(&queue, kPartialNoteOn1st);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());

  Add(&queue, kPartialNoteOn2nd);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());

  Add(&queue, kPartialGMOn1st);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());

  Add(&queue, kPartialNoteOn3rd);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
}

TEST(MidiMessageQueueTest, ReconstructSysExMessage) {
  MidiMessageQueue queue(true);
  std::vector<uint8> message;

  Add(&queue, kPartialGMOn1st);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());

  Add(&queue, kPartialGMOn2nd);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());

  Add(&queue, kPartialGMOn3rd);
  queue.Get(&message);
  EXPECT_MESSAGE(kGMOn, message);

  queue.Get(&message);
  EXPECT_TRUE(message.empty());
}

TEST(MidiMessageQueueTest, ReconstructBrokenSysExMessage) {
  MidiMessageQueue queue(true);
  std::vector<uint8> message;

  Add(&queue, kPartialGMOn1st);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());

  Add(&queue, kPartialGMOn2nd);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());

  Add(&queue, kPartialNoteOn1st);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());

  Add(&queue, kPartialGMOn3rd);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());

  queue.Get(&message);
  EXPECT_TRUE(message.empty());
}

TEST(MidiMessageQueueTest, OneByteMessage) {
  MidiMessageQueue queue(true);
  std::vector<uint8> message;

  Add(&queue, kSystemCommonMessageTuneRequest);
  Add(&queue, kSystemCommonMessageTuneRequest);
  Add(&queue, kSystemCommonMessageTuneRequest);
  Add(&queue, kSystemCommonMessageTuneRequest);
  Add(&queue, kNoteOn);
  Add(&queue, kSystemCommonMessageTuneRequest);
  Add(&queue, kNoteOn);
  Add(&queue, kNoteOn);
  Add(&queue, kSystemCommonMessageTuneRequest);

  queue.Get(&message);
  EXPECT_MESSAGE(kSystemCommonMessageTuneRequest, message);
  queue.Get(&message);
  EXPECT_MESSAGE(kSystemCommonMessageTuneRequest, message);
  queue.Get(&message);
  EXPECT_MESSAGE(kSystemCommonMessageTuneRequest, message);
  queue.Get(&message);
  EXPECT_MESSAGE(kSystemCommonMessageTuneRequest, message);
  queue.Get(&message);
  EXPECT_MESSAGE(kNoteOn, message);
  queue.Get(&message);
  EXPECT_MESSAGE(kSystemCommonMessageTuneRequest, message);
  queue.Get(&message);
  EXPECT_MESSAGE(kNoteOn, message);
  queue.Get(&message);
  EXPECT_MESSAGE(kNoteOn, message);
  queue.Get(&message);
  EXPECT_MESSAGE(kSystemCommonMessageTuneRequest, message);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
}

TEST(MidiMessageQueueTest, OneByteMessageInjectedInNonSysExMessage) {
  MidiMessageQueue queue(true);
  std::vector<uint8> message;

  Add(&queue, kPartialNoteOn1st);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());

  Add(&queue, kPartialNoteOn2nd);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());

  Add(&queue, kSystemCommonMessageTuneRequest);
  queue.Get(&message);
  EXPECT_MESSAGE(kSystemCommonMessageTuneRequest, message);

  queue.Get(&message);
  EXPECT_TRUE(message.empty());

  Add(&queue, kPartialNoteOn3rd);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
}

TEST(MidiMessageQueueTest, OneByteMessageInjectedInSysExMessage) {
  MidiMessageQueue queue(true);
  std::vector<uint8> message;

  Add(&queue, kPartialGMOn1st);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());

  Add(&queue, kPartialGMOn2nd);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());

  Add(&queue, kSystemCommonMessageTuneRequest);
  queue.Get(&message);
  EXPECT_MESSAGE(kSystemCommonMessageTuneRequest, message);

  queue.Get(&message);
  EXPECT_TRUE(message.empty());

  Add(&queue, kPartialGMOn3rd);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
}

TEST(MidiMessageQueueTest, ReservedMessage) {
  MidiMessageQueue queue(true);
  std::vector<uint8> message;

  Add(&queue, kReservedMessage1);
  Add(&queue, kNoteOn);
  Add(&queue, kReservedMessage2);
  Add(&queue, kNoteOn);
  Add(&queue, kReservedMessage1WithDataBytes);
  Add(&queue, kNoteOn);
  Add(&queue, kReservedMessage2);
  Add(&queue, kReservedMessage1WithDataBytes);
  Add(&queue, kNoteOn);
  Add(&queue, kReservedMessage1WithDataBytes);
  Add(&queue, kReservedMessage2);
  Add(&queue, kNoteOn);

  queue.Get(&message);
  EXPECT_MESSAGE(kNoteOn, message);
  queue.Get(&message);
  EXPECT_MESSAGE(kNoteOn, message);
  queue.Get(&message);
  EXPECT_MESSAGE(kNoteOn, message);
  queue.Get(&message);
  EXPECT_MESSAGE(kNoteOn, message);
  queue.Get(&message);
  EXPECT_MESSAGE(kNoteOn, message);

  Add(&queue, kReservedMessage1);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
  queue.Get(&message);
  EXPECT_TRUE(message.empty());

  Add(&queue, kNoteOn);
  queue.Get(&message);
  EXPECT_MESSAGE(kNoteOn, message);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());

  Add(&queue, kReservedMessage2);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
  queue.Get(&message);
  EXPECT_TRUE(message.empty());

  Add(&queue, kNoteOn);
  queue.Get(&message);
  EXPECT_MESSAGE(kNoteOn, message);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());

  Add(&queue, kReservedMessage1WithDataBytes);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
  queue.Get(&message);
  EXPECT_TRUE(message.empty());

  Add(&queue, kNoteOn);
  queue.Get(&message);
  EXPECT_MESSAGE(kNoteOn, message);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());

  Add(&queue, kReservedMessage2);
  Add(&queue, kReservedMessage1WithDataBytes);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
  queue.Get(&message);
  EXPECT_TRUE(message.empty());

  Add(&queue, kNoteOn);
  queue.Get(&message);
  EXPECT_MESSAGE(kNoteOn, message);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());

  Add(&queue, kReservedMessage1WithDataBytes);
  Add(&queue, kReservedMessage2);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
  queue.Get(&message);
  EXPECT_TRUE(message.empty());

  Add(&queue, kNoteOn);
  queue.Get(&message);
  EXPECT_MESSAGE(kNoteOn, message);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
}

TEST(MidiMessageQueueTest, ReservedMessageInjectedInNonSysExMessage) {
  MidiMessageQueue queue(true);
  std::vector<uint8> message;

  // Inject |kReservedMessage1|
  Add(&queue, kPartialNoteOn1st);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
  Add(&queue, kPartialNoteOn2nd);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
  Add(&queue, kReservedMessage1);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
  Add(&queue, kPartialNoteOn3rd);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());

  // Inject |kReservedMessage2|
  Add(&queue, kPartialNoteOn1st);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
  Add(&queue, kPartialNoteOn2nd);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
  Add(&queue, kReservedMessage2);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
  Add(&queue, kPartialNoteOn3rd);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());

  // Inject |kReservedMessage1WithDataBytes|
  Add(&queue, kPartialNoteOn1st);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
  Add(&queue, kPartialNoteOn2nd);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
  Add(&queue, kReservedMessage1WithDataBytes);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
  Add(&queue, kPartialNoteOn3rd);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
}

TEST(MidiMessageQueueTest, ReservedMessageInjectedInSysExMessage) {
  MidiMessageQueue queue(true);
  std::vector<uint8> message;

  // Inject |kReservedMessage1|
  Add(&queue, kPartialGMOn1st);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
  Add(&queue, kPartialGMOn2nd);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
  Add(&queue, kReservedMessage1);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
  Add(&queue, kPartialGMOn3rd);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());

  // Inject |kReservedMessage2|
  Add(&queue, kPartialGMOn1st);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
  Add(&queue, kPartialGMOn2nd);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
  Add(&queue, kReservedMessage2);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
  Add(&queue, kPartialGMOn3rd);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());

  // Inject |kReservedMessage1WithDataBytes|
  Add(&queue, kPartialGMOn1st);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
  Add(&queue, kPartialGMOn2nd);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
  Add(&queue, kReservedMessage1WithDataBytes);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
  Add(&queue, kPartialGMOn3rd);
  queue.Get(&message);
  EXPECT_TRUE(message.empty());
}

}  // namespace
}  // namespace midi
}  // namespace media
