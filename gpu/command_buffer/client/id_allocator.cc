// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the implementation of IdAllocator.

#include "gpu/command_buffer/client/id_allocator.h"

namespace gpu {

IdAllocator::IdAllocator() : bitmap_(1) { bitmap_[0] = 0; }

static const unsigned int kBitsPerUint32 = sizeof(Uint32) * 8;  // NOLINT

// Looks for the first non-full entry, and return the first free bit in that
// entry. If all the entries are full, it will return the first bit of an entry
// that would be appended, but doesn't actually append that entry to the vector.
unsigned int IdAllocator::FindFirstFree() const {
  size_t size = bitmap_.size();
  for (unsigned int i = 0; i < size; ++i) {
    Uint32 value = bitmap_[i];
    if (value != 0xffffffffU) {
      for (unsigned int j = 0; j < kBitsPerUint32; ++j) {
        if (!(value & (1 << j))) return i * kBitsPerUint32 + j;
      }
      DLOG(FATAL) << "Code should not reach here.";
    }
  }
  return size*kBitsPerUint32;
}

// Sets the correct bit in the proper entry, resizing the vector if needed.
void IdAllocator::SetBit(unsigned int bit, bool value) {
  size_t size = bitmap_.size();
  if (bit >= size * kBitsPerUint32) {
    size_t newsize = bit / kBitsPerUint32 + 1;
    bitmap_.resize(newsize);
    for (size_t i = size; i < newsize; ++i) bitmap_[i] = 0;
  }
  Uint32 mask = 1U << (bit % kBitsPerUint32);
  if (value) {
    bitmap_[bit / kBitsPerUint32] |= mask;
  } else {
    bitmap_[bit / kBitsPerUint32] &= ~mask;
  }
}

// Gets the bit from the proper entry. This doesn't resize the vector, just
// returns false if the bit is beyond the last entry.
bool IdAllocator::GetBit(unsigned int bit) const {
  size_t size = bitmap_.size();
  if (bit / kBitsPerUint32 >= size) return false;
  Uint32 mask = 1U << (bit % kBitsPerUint32);
  return (bitmap_[bit / kBitsPerUint32] & mask) != 0;
}

}  // namespace gpu
