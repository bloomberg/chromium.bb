// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/hpack_entry.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "net/spdy/hpack_string_util.h"

namespace net {

using base::StringPiece;

const size_t HpackEntry::kSizeOverhead = 32;

bool HpackEntry::Comparator::operator() (
    const HpackEntry* lhs, const HpackEntry* rhs) const {
  int result = lhs->name().compare(rhs->name());
  if (result != 0)
    return result < 0;
  result = lhs->value().compare(rhs->value());
  if (result != 0)
    return result < 0;
  DCHECK(lhs == rhs || lhs->Index() != rhs->Index());
  return lhs->Index() < rhs->Index();
}

HpackEntry::HpackEntry(StringPiece name,
                       StringPiece value,
                       bool is_static,
                       size_t insertion_index,
                       const size_t* total_table_insertions_or_current_size)
    : name_(name.data(), name.size()),
      value_(value.data(), value.size()),
      is_static_(is_static),
      state_(0),
      insertion_index_(insertion_index),
      total_insertions_or_size_(total_table_insertions_or_current_size) {
  CHECK_NE(total_table_insertions_or_current_size,
           static_cast<const size_t*>(NULL));
}

HpackEntry::HpackEntry(StringPiece name, StringPiece value)
    : name_(name.data(), name.size()),
      value_(value.data(), value.size()),
      is_static_(false),
      state_(0),
      insertion_index_(0),
      total_insertions_or_size_(NULL) {
}

HpackEntry::HpackEntry()
    : is_static_(false),
      state_(0),
      insertion_index_(0),
      total_insertions_or_size_(NULL) {
}

HpackEntry::~HpackEntry() {}

size_t HpackEntry::Index() const {
  if (total_insertions_or_size_ == NULL) {
    // This is a lookup instance.
    return 0;
  } else if (IsStatic()) {
    return 1 + insertion_index_ + *total_insertions_or_size_;
  } else {
    return *total_insertions_or_size_ - insertion_index_;
  }
}

// static
size_t HpackEntry::Size(StringPiece name, StringPiece value) {
  return name.size() + value.size() + kSizeOverhead;
}
size_t HpackEntry::Size() const {
  return Size(name(), value());
}

std::string HpackEntry::GetDebugString() const {
  return "{ name: \"" + name_ +
      "\", value: \"" + value_ +
      "\", " + (IsStatic() ? "static" : "dynamic") +
      ", state: " + base::IntToString(state_) + " }";
}

}  // namespace net
