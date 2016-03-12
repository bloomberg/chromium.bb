// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/disassembler.h"

#include "base/logging.h"

namespace courgette {

Disassembler::Disassembler(const void* start, size_t length)
    : failure_reason_("uninitialized") {
  start_ = reinterpret_cast<const uint8_t*>(start);
  length_ = length;
  end_ = start_ + length_;
};

Disassembler::~Disassembler() {};

const uint8_t* Disassembler::FileOffsetToPointer(FileOffset file_offset) const {
  CHECK_LE(file_offset, static_cast<FileOffset>(end_ - start_));
  return start_ + file_offset;
}

const uint8_t* Disassembler::RVAToPointer(RVA rva) const {
  FileOffset file_offset = RVAToFileOffset(rva);
  if (file_offset == kNoFileOffset)
    return nullptr;

  return FileOffsetToPointer(file_offset);
}

bool Disassembler::Good() {
  failure_reason_ = nullptr;
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
