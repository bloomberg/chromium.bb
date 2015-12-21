// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_IMAGE_UTILS_H_
#define COURGETTE_IMAGE_UTILS_H_

#include <stddef.h>
#include <stdint.h>

// COURGETTE_HISTOGRAM_TARGETS prints out a histogram of how frequently
// different target addresses are referenced. Purely for debugging.
#define COURGETTE_HISTOGRAM_TARGETS 0

namespace courgette {

typedef uint32_t RVA;

// A Label is a symbolic reference to an address.  Unlike a conventional
// assembly language, we always know the address.  The address will later be
// stored in a table and the Label will be replaced with the index into the
// table.
// TODO(huangs): Make this a struct, and remove "_" from member names.
class Label {
 public:
  enum : int { kNoIndex = -1 };
  explicit Label(RVA rva) : rva_(rva) {}

  bool operator==(const Label& other) const {
    return rva_ == other.rva_ && index_ == other.index_ &&
           count_ == other.count_;
  }

  RVA rva_ = 0;           // Address referred to by the label.
  int index_ = kNoIndex;  // Index of address in address table.
  int32_t count_ = 0;
};

// These helper functions avoid the need for casts in the main code.
inline uint16_t ReadU16(const uint8_t* address, size_t offset) {
  return *reinterpret_cast<const uint16_t*>(address + offset);
}

inline uint32_t ReadU32(const uint8_t* address, size_t offset) {
  return *reinterpret_cast<const uint32_t*>(address + offset);
}

inline uint64_t ReadU64(const uint8_t* address, size_t offset) {
  return *reinterpret_cast<const uint64_t*>(address + offset);
}

inline uint16_t Read16LittleEndian(const void* address) {
  return *reinterpret_cast<const uint16_t*>(address);
}

inline uint32_t Read32LittleEndian(const void* address) {
  return *reinterpret_cast<const uint32_t*>(address);
}

inline uint64_t Read64LittleEndian(const void* address) {
  return *reinterpret_cast<const uint64_t*>(address);
}

}  // namespace courgette

#endif  // COURGETTE_IMAGE_UTILS_H_
