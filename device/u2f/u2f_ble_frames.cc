// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/u2f/u2f_ble_frames.h"

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"

#include <algorithm>

namespace device {

U2fBleFrame::U2fBleFrame() = default;

U2fBleFrame::U2fBleFrame(U2fCommandType command, std::vector<uint8_t> data)
    : command_(command), data_(std::move(data)) {}

U2fBleFrame::U2fBleFrame(U2fBleFrame&&) = default;
U2fBleFrame& U2fBleFrame::operator=(U2fBleFrame&&) = default;

U2fBleFrame::~U2fBleFrame() = default;

bool U2fBleFrame::IsValid() const {
  switch (command_) {
    case U2fCommandType::CMD_PING:
    case U2fCommandType::CMD_MSG:
      return true;
    case U2fCommandType::CMD_KEEPALIVE:
    case U2fCommandType::CMD_ERROR:
      return data_.size() == 1;
    case U2fCommandType::UNDEFINED:
    default:
      return false;
  }
}

U2fBleFrame::KeepaliveCode U2fBleFrame::GetKeepaliveCode() const {
  DCHECK_EQ(command_, U2fCommandType::CMD_KEEPALIVE);
  DCHECK_EQ(data_.size(), 1u);
  return static_cast<KeepaliveCode>(data_[0]);
}

U2fBleFrame::ErrorCode U2fBleFrame::GetErrorCode() const {
  DCHECK_EQ(command_, U2fCommandType::CMD_ERROR);
  DCHECK_EQ(data_.size(), 1u);
  return static_cast<ErrorCode>(data_[0]);
}

std::pair<U2fBleFrameInitializationFragment,
          std::vector<U2fBleFrameContinuationFragment>>
U2fBleFrame::ToFragments(size_t max_fragment_size) const {
  DCHECK_LE(data_.size(), 0xFFFFu);
  DCHECK_GE(max_fragment_size, 3u);

  size_t data_fragment_size = std::min(max_fragment_size - 3, data_.size());
  U2fBleFrameInitializationFragment initial_fragment(
      command_, static_cast<uint16_t>(data_.size()), data_.data(),
      data_fragment_size);

  size_t num_continuation_fragments = 0;
  std::vector<U2fBleFrameContinuationFragment> other_fragments;
  for (size_t pos = data_fragment_size; pos < data_.size();
       pos += max_fragment_size - 1) {
    data_fragment_size = std::min(data_.size() - pos, max_fragment_size - 1);
    other_fragments.push_back(
        U2fBleFrameContinuationFragment(&data_[pos], data_fragment_size,
                                        (num_continuation_fragments++) & 0x7F));
  }

  return std::make_pair(initial_fragment, std::move(other_fragments));
}

bool U2fBleFrameInitializationFragment::Parse(
    const std::vector<uint8_t>& data,
    U2fBleFrameInitializationFragment* fragment) {
  if (data.size() < 3)
    return false;

  const auto command = static_cast<U2fCommandType>(data[0]);
  const uint16_t data_length = (static_cast<uint16_t>(data[1]) << 8) + data[2];
  if (static_cast<size_t>(data_length) + 3 < data.size())
    return false;

  *fragment = U2fBleFrameInitializationFragment(
      command, data_length, data.data() + 3, data.size() - 3);
  return true;
}

size_t U2fBleFrameInitializationFragment::Serialize(
    std::vector<uint8_t>* buffer) const {
  buffer->push_back(static_cast<uint8_t>(command_));
  buffer->push_back((data_length_ >> 8) & 0xFF);
  buffer->push_back(data_length_ & 0xFF);
  buffer->insert(buffer->end(), data(), data() + size());
  return size() + 3;
}

bool U2fBleFrameContinuationFragment::Parse(
    const std::vector<uint8_t>& data,
    U2fBleFrameContinuationFragment* fragment) {
  if (data.empty())
    return false;
  const uint8_t sequence = data[0];
  *fragment = U2fBleFrameContinuationFragment(data.data() + 1, data.size() - 1,
                                              sequence);
  return true;
}

size_t U2fBleFrameContinuationFragment::Serialize(
    std::vector<uint8_t>* buffer) const {
  buffer->push_back(sequence_);
  buffer->insert(buffer->end(), data(), data() + size());
  return size() + 1;
}

U2fBleFrameAssembler::U2fBleFrameAssembler(
    const U2fBleFrameInitializationFragment& fragment)
    : frame_(fragment.command(), std::vector<uint8_t>()) {
  std::vector<uint8_t>& data = frame_.data();
  data.reserve(fragment.data_length());
  data.assign(fragment.data(), fragment.data() + fragment.size());
}

bool U2fBleFrameAssembler::AddFragment(
    const U2fBleFrameContinuationFragment& fragment) {
  if (fragment.sequence() != sequence_number_)
    return false;
  if (++sequence_number_ > 0x7F)
    sequence_number_ = 0;

  std::vector<uint8_t>& data = frame_.data();
  if (data.size() + fragment.size() > data.capacity())
    return false;
  data.insert(data.end(), fragment.data(), fragment.data() + fragment.size());
  return true;
}

bool U2fBleFrameAssembler::IsDone() const {
  return frame_.data().size() == frame_.data().capacity();
}

U2fBleFrame* U2fBleFrameAssembler::GetFrame() {
  return IsDone() ? &frame_ : nullptr;
}

U2fBleFrameAssembler::~U2fBleFrameAssembler() = default;

}  // namespace device
