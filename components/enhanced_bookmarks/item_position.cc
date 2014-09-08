// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enhanced_bookmarks/item_position.h"

#include "base/logging.h"

namespace {
const unsigned kPositionBase = 64;
const char kPositionAlphabet[] =
    ".0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz";
}  // namespace

namespace enhanced_bookmarks {

ItemPosition::ItemPosition() {
}

ItemPosition::ItemPosition(const PositionVector& position)
    : position_(position) {
}

ItemPosition::~ItemPosition() {
}

// static
ItemPosition ItemPosition::CreateInitialPosition() {
  PositionVector position(1, kPositionBase / 2);
  return ItemPosition(position);
}

// static
ItemPosition ItemPosition::CreateBefore(const ItemPosition& other) {
  DCHECK(other.IsValid());
  return ItemPosition(CreateBeforeImpl(other.position_, 0));
}

// static
ItemPosition::PositionVector ItemPosition::CreateBeforeImpl(
    const PositionVector& other,
    size_t from_index) {
  DCHECK_LT(from_index, other.size());
  PositionVector before(other.begin() + from_index, other.end());

  // Decrement the last character instead of going half-way to 0 in order to
  // make sure chaining CreateBefore calls result in logarithmic rather than
  // linear length growth.
  before[before.size() - 1] /= 2;
  if (before[before.size() - 1] != 0) {
    // If the last digit didn't change to 0, we're done!
    return before;
  }

  // Reset trailing zeros, then decrement the last non-zero digit.
  int index = before.size() - 1;
  while (index >= 0 && before[index] == 0) {
    before[index--] = kPositionBase / 2;
  }

  // Negative index means all digits were zeros. Put that many zeros to the
  // front of the string to get one that is comes before the input given.
  // This will cause the returned string to be twice as long as the input one,
  // (and about twice as long as needed for a valid return value), however that
  // means this function can be called many times more before we need to
  // increase the string size again. Increasing it with the minimum length
  // would result in a linear string size growth.
  if (index < 0) {
    before.insert(before.begin(), before.size(), 0);
  } else {
    before[index] /= 2;
  }
  return before;
}

// static
ItemPosition ItemPosition::CreateAfter(const ItemPosition& other) {
  DCHECK(other.IsValid());
  return ItemPosition(CreateAfterImpl(other.position_, 0));
}

// static
ItemPosition::PositionVector ItemPosition::CreateAfterImpl(
    const PositionVector& other,
    size_t from_index) {
  DCHECK_LE(from_index, other.size());
  if (from_index == other.size()) {
    return PositionVector(1, kPositionBase / 2);
  }

  PositionVector after(other.begin() + from_index, other.end());

  // Instead of splitting the position with infinity, increment the last digit
  // possible, and reset all digits after. This makes sure chaining createAfter
  // will result in a logarithmic rather than linear length growth.
  size_t index = after.size() - 1;
  do {
    after[index] += (kPositionBase - after[index] + 1) / 2;
    if (after[index] != kPositionBase)
      return after;
    after[index] = kPositionBase / 2;
  } while (index-- > 0);

  // All digits must have been at the maximal value already, so the string
  // length has to increase. Double it's size to ensure CreateAfter can be
  // called exponentially more times every time this needs to happen.
  after.insert(after.begin(), after.size(), kPositionBase - 1);

  return after;
}

// static
ItemPosition ItemPosition::CreateBetween(const ItemPosition& before,
                                         const ItemPosition& after) {
  DCHECK(before.IsValid() && after.IsValid());
  return ItemPosition(CreateBetweenImpl(before.position_, after.position_));
}

// static
ItemPosition::PositionVector ItemPosition::CreateBetweenImpl(
    const PositionVector& before,
    const PositionVector& after) {
  DCHECK(before < after);

  PositionVector between;
  for (size_t i = 0; i < before.size(); i++) {
    if (before[i] == after[i]) {
      // Add the common prefix to the return value.
      between.push_back(before[i]);
      continue;
    }
    if (before[i] < after[i] - 1) {
      // Split the difference between the two characters.
      between.push_back((before[i] + after[i]) / 2);
      return between;
    }
    // The difference between before[i] and after[i] is one character. A valid
    // return is that character, plus something that comes after the remaining
    // characters of before.
    between.push_back(before[i]);
    PositionVector suffix = CreateAfterImpl(before, i + 1);
    between.insert(between.end(), suffix.begin(), suffix.end());
    return between;
  }

  // |before| must be a prefix of |after|, so return that prefix followed by
  // something that comes before the remaining digits of |after|.
  PositionVector suffix = CreateBeforeImpl(after, before.size());
  between.insert(between.end(), suffix.begin(), suffix.end());
  return between;
}

std::string ItemPosition::ToString() const {
  DCHECK_GT(arraysize(kPositionAlphabet), kPositionBase);

  std::string str;
  str.reserve(position_.size());
  for (size_t i = 0; i < position_.size(); i++) {
    unsigned char val = position_[i];
    CHECK_LT(val, kPositionBase);
    str.push_back(kPositionAlphabet[position_[i]]);
  }
  return str;
}

bool ItemPosition::IsValid() const {
  return !position_.empty() && position_[position_.size() - 1] != 0;
}

bool ItemPosition::Equals(const ItemPosition& other) const {
  return position_ == other.position_;
}

bool ItemPosition::LessThan(const ItemPosition& other) const {
  return position_ < other.position_;
}

}  // namespace enhanced_bookmarks
