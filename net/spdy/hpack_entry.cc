// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/hpack_entry.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "net/spdy/hpack_string_util.h"

namespace net {

namespace {

const uint32 kReferencedMask = 0x80000000;
const uint32 kTouchCountMask = 0x7fffffff;

}  // namespace

const uint32 HpackEntry::kSizeOverhead = 32;

const uint32 HpackEntry::kUntouched = 0x7fffffff;

HpackEntry::HpackEntry() : referenced_and_touch_count_(kUntouched) {}

HpackEntry::HpackEntry(base::StringPiece name, base::StringPiece value)
    : name_(name.as_string()),
      value_(value.as_string()),
      referenced_and_touch_count_(kUntouched) {}

bool HpackEntry::IsReferenced() const {
  return ((referenced_and_touch_count_ & kReferencedMask) != 0);
}

uint32 HpackEntry::TouchCount() const {
  return referenced_and_touch_count_ & kTouchCountMask;
}

size_t HpackEntry::Size() const {
  return name_.size() + value_.size() + kSizeOverhead;
}

std::string HpackEntry::GetDebugString() const {
  const char* is_referenced_str = (IsReferenced() ? "true" : "false");
  std::string touch_count_str = "(untouched)";
  if (TouchCount() != kUntouched)
    touch_count_str = base::IntToString(TouchCount());
  return "{ name: \"" + name_ + "\", value: \"" + value_ +
      "\", referenced: " + is_referenced_str + ", touch_count: " +
      touch_count_str + " }";
}

bool HpackEntry::Equals(const HpackEntry& other) const {
  return
      StringPiecesEqualConstantTime(name_, other.name_) &&
      StringPiecesEqualConstantTime(value_, other.value_) &&
      (referenced_and_touch_count_ == other.referenced_and_touch_count_);
}

void HpackEntry::SetReferenced(bool referenced) {
  referenced_and_touch_count_ &= kTouchCountMask;
  if (referenced)
    referenced_and_touch_count_ |= kReferencedMask;
}

void HpackEntry::AddTouches(uint32 additional_touch_count) {
  uint32 new_touch_count = TouchCount();
  if (new_touch_count == kUntouched)
    new_touch_count = 0;
  new_touch_count += additional_touch_count;
  DCHECK_LT(new_touch_count, kUntouched);
  referenced_and_touch_count_ &= kReferencedMask;
  referenced_and_touch_count_ |= new_touch_count;
}

void HpackEntry::ClearTouches() {
  referenced_and_touch_count_ &= kReferencedMask;
  referenced_and_touch_count_ |= kUntouched;
}

}  // namespace net
