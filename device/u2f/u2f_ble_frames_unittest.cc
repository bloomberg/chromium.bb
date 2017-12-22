// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/u2f/u2f_ble_frames.h"

#include <vector>

#include "device/u2f/u2f_command_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::vector<uint8_t> GetSomeData(size_t size) {
  std::vector<uint8_t> data(size);
  for (size_t i = 0; i < size; ++i)
    data[i] = static_cast<uint8_t>((i * i) & 0xFF);
  return data;
}

}  // namespace

namespace device {

TEST(U2fBleFramesTest, InitializationFragment) {
  const std::vector<uint8_t> data = GetSomeData(25);
  constexpr uint16_t kDataLength = 21123;

  U2fBleFrameInitializationFragment fragment(
      U2fCommandType::CMD_MSG, kDataLength, base::make_span(data));

  std::vector<uint8_t> buffer;
  const size_t binary_size = fragment.Serialize(&buffer);
  EXPECT_EQ(buffer.size(), binary_size);

  EXPECT_EQ(data.size() + 3, binary_size);

  U2fBleFrameInitializationFragment parsed_fragment;
  ASSERT_TRUE(
      U2fBleFrameInitializationFragment::Parse(buffer, &parsed_fragment));

  EXPECT_EQ(kDataLength, parsed_fragment.data_length());
  EXPECT_EQ(base::make_span(data), parsed_fragment.fragment());
  EXPECT_EQ(U2fCommandType::CMD_MSG, parsed_fragment.command());
}

TEST(U2fBleFramesTest, ContinuationFragment) {
  const auto data = GetSomeData(25);
  constexpr uint8_t kSequence = 61;

  U2fBleFrameContinuationFragment fragment(base::make_span(data), kSequence);

  std::vector<uint8_t> buffer;
  const size_t binary_size = fragment.Serialize(&buffer);
  EXPECT_EQ(buffer.size(), binary_size);

  EXPECT_EQ(data.size() + 1, binary_size);

  U2fBleFrameContinuationFragment parsed_fragment;
  ASSERT_TRUE(U2fBleFrameContinuationFragment::Parse(buffer, &parsed_fragment));

  EXPECT_EQ(base::make_span(data), parsed_fragment.fragment());
  EXPECT_EQ(kSequence, parsed_fragment.sequence());
}

TEST(U2fBleFramesTest, SplitAndAssemble) {
  for (size_t size : {0,  1,  16, 17, 18, 20, 21, 22, 35,  36,
                      37, 39, 40, 41, 54, 55, 56, 60, 100, 65535}) {
    SCOPED_TRACE(size);

    U2fBleFrame frame(U2fCommandType::CMD_PING, GetSomeData(size));

    auto fragments = frame.ToFragments(20);

    EXPECT_EQ(frame.command(), fragments.first.command());
    EXPECT_EQ(frame.data().size(),
              static_cast<size_t>(fragments.first.data_length()));

    U2fBleFrameAssembler assembler(fragments.first);
    while (!fragments.second.empty()) {
      ASSERT_TRUE(assembler.AddFragment(fragments.second.front()));
      fragments.second.pop();
    }

    EXPECT_TRUE(assembler.IsDone());
    ASSERT_TRUE(assembler.GetFrame());

    auto result_frame = std::move(*assembler.GetFrame());
    EXPECT_EQ(frame.command(), result_frame.command());
    EXPECT_EQ(frame.data(), result_frame.data());
  }
}

TEST(U2fBleFramesTest, FrameAssemblerError) {
  U2fBleFrame frame(U2fCommandType::CMD_PING, GetSomeData(30));

  auto fragments = frame.ToFragments(20);
  ASSERT_EQ(1u, fragments.second.size());

  fragments.second.front() =
      U2fBleFrameContinuationFragment(fragments.second.front().fragment(), 51);

  U2fBleFrameAssembler assembler(fragments.first);
  EXPECT_FALSE(assembler.IsDone());
  EXPECT_FALSE(assembler.GetFrame());
  EXPECT_FALSE(assembler.AddFragment(fragments.second.front()));
  EXPECT_FALSE(assembler.IsDone());
  EXPECT_FALSE(assembler.GetFrame());
}

TEST(U2fBleFramesTest, FrameGettersAndValidity) {
  {
    U2fBleFrame frame(U2fCommandType::CMD_KEEPALIVE, std::vector<uint8_t>(2));
    EXPECT_FALSE(frame.IsValid());
  }
  {
    U2fBleFrame frame(U2fCommandType::CMD_ERROR, {});
    EXPECT_FALSE(frame.IsValid());
  }

  for (auto code : {U2fBleFrame::KeepaliveCode::TUP_NEEDED,
                    U2fBleFrame::KeepaliveCode::PROCESSING}) {
    U2fBleFrame frame(U2fCommandType::CMD_KEEPALIVE,
                      std::vector<uint8_t>(1, static_cast<uint8_t>(code)));
    EXPECT_TRUE(frame.IsValid());
    EXPECT_EQ(code, frame.GetKeepaliveCode());
  }

  for (auto code : {
           U2fBleFrame::ErrorCode::INVALID_CMD,
           U2fBleFrame::ErrorCode::INVALID_PAR,
           U2fBleFrame::ErrorCode::INVALID_SEQ,
           U2fBleFrame::ErrorCode::INVALID_LEN,
           U2fBleFrame::ErrorCode::REQ_TIMEOUT, U2fBleFrame::ErrorCode::NA_1,
           U2fBleFrame::ErrorCode::NA_2, U2fBleFrame::ErrorCode::NA_3,
       }) {
    U2fBleFrame frame(U2fCommandType::CMD_ERROR, {static_cast<uint8_t>(code)});
    EXPECT_TRUE(frame.IsValid());
    EXPECT_EQ(code, frame.GetErrorCode());
  }
}

}  // namespace device
