// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/u2f/u2f_ble_frames.h"

#include <algorithm>
#include <limits>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"

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
          base::queue<U2fBleFrameContinuationFragment>>
U2fBleFrame::ToFragments(size_t max_fragment_size) const {
  DCHECK_LE(data_.size(), std::numeric_limits<uint16_t>::max());
  DCHECK_GE(max_fragment_size, 3u);

  // Cast is necessary to ignore too high bits.
  auto data_view =
      base::make_span(data_.data(), static_cast<uint16_t>(data_.size()));

  // Subtract 3 to account for CMD, HLEN and LLEN bytes.
  const size_t init_fragment_size =
      std::min(max_fragment_size - 3, data_view.size());

  U2fBleFrameInitializationFragment initial_fragment(
      command_, data_view.size(), data_view.first(init_fragment_size));

  base::queue<U2fBleFrameContinuationFragment> other_fragments;
  data_view = data_view.subspan(init_fragment_size);

  while (!data_view.empty()) {
    // Subtract 1 to account for SEQ byte.
    const size_t cont_fragment_size =
        std::min(max_fragment_size - 1, data_view.size());
    // High bit must stay cleared.
    other_fragments.emplace(data_view.first(cont_fragment_size),
                            other_fragments.size() & 0x7F);

    data_view = data_view.subspan(cont_fragment_size);
  }

  return {initial_fragment, std::move(other_fragments)};
}

U2fBleFrameFragment::U2fBleFrameFragment() = default;

U2fBleFrameFragment::U2fBleFrameFragment(const U2fBleFrameFragment& frame) =
    default;
U2fBleFrameFragment::~U2fBleFrameFragment() = default;

U2fBleFrameFragment::U2fBleFrameFragment(base::span<const uint8_t> fragment)
    : fragment_(fragment) {}

bool U2fBleFrameInitializationFragment::Parse(
    base::span<const uint8_t> data,
    U2fBleFrameInitializationFragment* fragment) {
  if (data.size() < 3)
    return false;

  const auto command = static_cast<U2fCommandType>(data[0]);
  const uint16_t data_length = (static_cast<uint16_t>(data[1]) << 8) + data[2];
  if (static_cast<size_t>(data_length) + 3 < data.size())
    return false;

  *fragment =
      U2fBleFrameInitializationFragment(command, data_length, data.subspan(3));
  return true;
}

size_t U2fBleFrameInitializationFragment::Serialize(
    std::vector<uint8_t>* buffer) const {
  buffer->push_back(static_cast<uint8_t>(command_));
  buffer->push_back((data_length_ >> 8) & 0xFF);
  buffer->push_back(data_length_ & 0xFF);
  buffer->insert(buffer->end(), fragment().begin(), fragment().end());
  return fragment().size() + 3;
}

bool U2fBleFrameContinuationFragment::Parse(
    base::span<const uint8_t> data,
    U2fBleFrameContinuationFragment* fragment) {
  if (data.empty())
    return false;
  const uint8_t sequence = data[0];
  *fragment = U2fBleFrameContinuationFragment(data.subspan(1), sequence);
  return true;
}

size_t U2fBleFrameContinuationFragment::Serialize(
    std::vector<uint8_t>* buffer) const {
  buffer->push_back(sequence_);
  buffer->insert(buffer->end(), fragment().begin(), fragment().end());
  return fragment().size() + 1;
}

U2fBleFrameAssembler::U2fBleFrameAssembler(
    const U2fBleFrameInitializationFragment& fragment)
    : data_length_(fragment.data_length()),
      frame_(fragment.command(),
             std::vector<uint8_t>(fragment.fragment().begin(),
                                  fragment.fragment().end())) {}

bool U2fBleFrameAssembler::AddFragment(
    const U2fBleFrameContinuationFragment& fragment) {
  if (fragment.sequence() != sequence_number_)
    return false;
  sequence_number_ = (sequence_number_ + 1) & 0x7F;

  if (static_cast<size_t>(data_length_) <
      frame_.data().size() + fragment.fragment().size()) {
    return false;
  }

  frame_.data().insert(frame_.data().end(), fragment.fragment().begin(),
                       fragment.fragment().end());
  return true;
}

bool U2fBleFrameAssembler::IsDone() const {
  return frame_.data().size() == data_length_;
}

U2fBleFrame* U2fBleFrameAssembler::GetFrame() {
  return IsDone() ? &frame_ : nullptr;
}

U2fBleFrameAssembler::~U2fBleFrameAssembler() = default;

}  // namespace device
