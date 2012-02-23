// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/string_ordinal.h"

#include <algorithm>
#include <cstddef>

#include "base/basictypes.h"
#include "base/logging.h"

namespace {

// Constants for StringOrdinal digits.
const char kZeroDigit = 'a';
const char kMinDigit = 'b';
const char kMidDigit = 'n';
const char kMaxDigit = 'z';
const int kMidDigitValue = kMidDigit - kZeroDigit;
const int kMaxDigitValue = kMaxDigit - kZeroDigit;
const int kRadix = kMaxDigitValue + 1;
COMPILE_ASSERT(kMidDigitValue == 13, kMidDigitValue_incorrect);
COMPILE_ASSERT(kMaxDigitValue == 25, kMaxDigitValue_incorrect);
COMPILE_ASSERT(kRadix == 26, kRadix_incorrect);

// Helper Functions

// Returns the length that string value.substr(0, length) would be with
// trailing zeros removed.
size_t GetLengthWithoutTrailingZeros(const std::string& value, size_t length) {
  DCHECK(!value.empty());

  size_t end_position = value.find_last_not_of(kZeroDigit, length - 1);

  // If no non kZeroDigit is found then the string is a string of all zeros
  // digits so we return 0 as the correct length.
  if (end_position == std::string::npos)
    return 0;

  return end_position + 1;
}

// Return the digit value at position i, padding with kZeroDigit if required.
int GetPositionValue(const std::string& str, size_t i) {
  return (i < str.length()) ? (str[i] - kZeroDigit) : 0;
}

// Add kMidDigitValue to the value at position index. This returns false if
// adding the half results in an overflow past the first digit, otherwise it
// returns true. This is used by ComputeMidpoint.
bool AddHalf(size_t position, std::string& value) {
  DCHECK_GT(position, 0U);
  DCHECK_LT(position, value.length());

  // We can't perform this operation directly on the string because
  // overflow can occur and mess up the values.
  int new_position_value = value[position] + kMidDigitValue;

  if (new_position_value <= kMaxDigit) {
    value[position] = new_position_value;
  } else {
    value[position] = new_position_value - kRadix;
    ++value[position - 1];

    for (size_t i = position - 1; value[i] > kMaxDigit; --i) {
      if (i == 0U) {
        // If the first digit is too large we have no previous digit
        // to increase, so we fail.
        return false;
      }
      value[i] -= kRadix;
      ++value[i - 1];
    }
  }

  return true;
}

// Drops off the last digit of value and then all trailing zeros iff that
// doesn't change its ordering as greater than |start|.
void DropUnneededDigits(const std::string& start, std::string* value) {
  CHECK_GT(*value, start);

  size_t drop_length = GetLengthWithoutTrailingZeros(*value, value->length());
  // See if the value can have its last digit removed without affecting
  // the ordering.
  if (drop_length > 1) {
    // We must drop the trailing zeros before comparing |shorter_value| to
    // |start| because if we don't we may have |shorter_value|=|start|+|a|*
    // where |shorter_value| > |start| but not when it drops the trailing |a|s
    // to become a valid StringOrdinal value.
    int truncated_length = GetLengthWithoutTrailingZeros(*value,
                                                         drop_length - 1);

    if (truncated_length != 0 && value->compare(0, truncated_length, start) > 0)
      drop_length = truncated_length;
  }

  value->resize(drop_length);
}

// Compute the midpoint string that is between |start| and |end|.
std::string ComputeMidpoint(const std::string& start,
                            const std::string& end) {
  size_t max_size = std::max(start.length(), end.length()) + 1;
  std::string midpoint(max_size, kZeroDigit);

  bool add_half = false;
  for (size_t i = 0; i < max_size; ++i) {
    int char_value = GetPositionValue(start, i);
    char_value += GetPositionValue(end, i);

    midpoint[i] += (char_value / 2);
    if (add_half) {
      // AddHalf only returns false if (midpoint[0] > kMaxDigit), which
      // implies the midpoint of two strings in (0, 1) is >= 1, which is a
      // contradiction.
      CHECK(AddHalf(i, midpoint));
    }

    add_half = (char_value % 2 == 1);
  }
  DCHECK(!add_half);

  return midpoint;
}

// Create a StringOrdinal that is lexigraphically greater than |start| and
// lexigraphically less than |end|. The returned StringOrdinal will be roughly
// between |start| and |end|.
StringOrdinal CreateStringOrdinalBetween(const StringOrdinal& start,
                                         const StringOrdinal& end) {
  CHECK(start.IsValid());
  CHECK(end.IsValid());
  CHECK(start.LessThan(end));
  const std::string& start_string = start.ToString();
  const std::string& end_string = end.ToString();
  DCHECK_LT(start_string, end_string);

  std::string midpoint = ComputeMidpoint(start_string, end_string);

  DropUnneededDigits(start_string, &midpoint);

  DCHECK_GT(midpoint, start_string);
  DCHECK_LT(midpoint, end_string);

  StringOrdinal midpoint_ordinal(midpoint);
  DCHECK(midpoint_ordinal.IsValid());
  return midpoint_ordinal;
}

// Returns true iff the input string matches the format [a-z]*[b-z].
bool IsValidStringOrdinal(const std::string& value) {
  if (value.empty())
    return false;

  for (size_t i = 0; i < value.length(); ++i) {
    if (value[i] < kZeroDigit || value[i] > kMaxDigit)
      return false;
  }

  return value[value.length() - 1] != kZeroDigit;
}

} // namespace

StringOrdinal::StringOrdinal(const std::string& string_ordinal)
    : string_ordinal_(string_ordinal),
      is_valid_(IsValidStringOrdinal(string_ordinal_)) {
}

StringOrdinal::StringOrdinal() : string_ordinal_(""),
                                 is_valid_(false) {
}

StringOrdinal StringOrdinal::CreateInitialOrdinal() {
  return StringOrdinal(std::string(1, kMidDigit));
}

bool StringOrdinal::IsValid() const {
  return is_valid_;
}

bool StringOrdinal::LessThan(const StringOrdinal& other) const {
  CHECK(IsValid());
  CHECK(other.IsValid());
  return string_ordinal_ < other.string_ordinal_;
}

bool StringOrdinal::GreaterThan(const StringOrdinal& other) const {
  CHECK(IsValid());
  CHECK(other.IsValid());
  return string_ordinal_ > other.string_ordinal_;
}

bool StringOrdinal::Equal(const StringOrdinal& other) const {
  CHECK(IsValid());
  CHECK(other.IsValid());
  return string_ordinal_ == other.string_ordinal_;
}

bool StringOrdinal::EqualOrBothInvalid(const StringOrdinal& other) const {
  if (!IsValid() && !other.IsValid())
    return true;

  if (!IsValid() || !other.IsValid())
    return false;

  return Equal(other);
}

StringOrdinal StringOrdinal::CreateBetween(const StringOrdinal& other) const {
  CHECK(IsValid());
  CHECK(other.IsValid());
  CHECK(!Equal(other));

  if (LessThan(other)) {
    return CreateStringOrdinalBetween(*this, other);
  } else {
    return CreateStringOrdinalBetween(other, *this);
  }
}

StringOrdinal StringOrdinal::CreateBefore() const {
  CHECK(IsValid());
  // Create the smallest valid StringOrdinal of the appropriate length
  // to be the minimum boundary.
  const size_t length = string_ordinal_.length();
  std::string start(length, kZeroDigit);
  start[length - 1] = kMinDigit;
  if (start == string_ordinal_) {
    start[length - 1] = kZeroDigit;
    start += kMinDigit;
  }

  // Even though |start| is already a valid StringOrdinal that is less
  // than |*this|, we don't return it because we wouldn't have much space in
  // front of it to insert potential future values.
  return CreateBetween(StringOrdinal(start));
}

StringOrdinal StringOrdinal::CreateAfter() const {
  CHECK(IsValid());
  // Create the largest valid StringOrdinal of the appropriate length to be
  // the maximum boundary.
  std::string end(string_ordinal_.length(), kMaxDigit);
  if (end == string_ordinal_)
    end += kMaxDigit;

  // Even though |end| is already a valid StringOrdinal that is greater than
  // |*this|, we don't return it because we wouldn't have much space after
  // it to insert potential future values.
  return CreateBetween(StringOrdinal(end));
}

std::string StringOrdinal::ToString() const {
  CHECK(IsValid());
  return string_ordinal_;
}

bool StringOrdinalLessThan::operator() (const StringOrdinal& lhs,
                                        const StringOrdinal& rhs) const {
  return lhs.LessThan(rhs);
}

bool StringOrdinal::operator==(const StringOrdinal& rhs) const {
  return Equal(rhs);
}
