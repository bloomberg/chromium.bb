// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/test/fake_scattered_buffer.h"

#include <string.h>

#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {
namespace v2 {

FakeScatteredBuffer::FakeScatteredBuffer(size_t chunk_size)
    : chunk_size_(chunk_size) {}

FakeScatteredBuffer::~FakeScatteredBuffer() {}

ContiguousMemoryRange FakeScatteredBuffer::GetNewBuffer() {
  std::unique_ptr<uint8_t[]> chunk(new uint8_t[chunk_size_]);
  uint8_t* begin = chunk.get();
  memset(begin, 0, chunk_size_);
  chunks_.push_back(std::move(chunk));
  return {begin, begin + chunk_size_};
}

std::string FakeScatteredBuffer::GetChunkAsString(int chunk_index) {
  return base::HexEncode(chunks_[chunk_index].get(), chunk_size_);
}

void FakeScatteredBuffer::GetBytes(size_t start, size_t length, uint8_t* buf) {
  ASSERT_LE(start + length, chunks_.size() * chunk_size_);
  for (size_t pos = 0; pos < length; ++pos) {
    size_t chunk_index = (start + pos) / chunk_size_;
    size_t chunk_offset = (start + pos) % chunk_size_;
    buf[pos] = chunks_[chunk_index].get()[chunk_offset];
  }
}

std::string FakeScatteredBuffer::GetBytesAsString(size_t start, size_t length) {
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[length]);
  GetBytes(start, length, buffer.get());
  return base::HexEncode(buffer.get(), length);
}

}  // namespace v2
}  // namespace tracing
