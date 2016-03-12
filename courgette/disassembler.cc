// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/disassembler.h"

namespace courgette {

Disassembler::Disassembler(const void* start, size_t length)
  : failure_reason_("uninitialized") {
  start_ = reinterpret_cast<const uint8_t*>(start);
  length_ = length;
  end_ = start_ + length_;
};

Disassembler::~Disassembler() {};

const uint8_t* Disassembler::OffsetToPointer(size_t offset) const {
  assert(start_ + offset <= end_);
  return start_ + offset;
}

bool Disassembler::Good() {
  failure_reason_ = NULL;
  return true;
}

bool Disassembler::Bad(const char* reason) {
  failure_reason_ = reason;
  return false;
}

void Disassembler::ReduceLength(size_t reduced_length) {
  CHECK_LE(reduced_length, length_);
  length_ = reduced_length;
  end_ = start_ + length_;
}

}  // namespace courgette
