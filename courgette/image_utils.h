// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_IMAGE_UTILS_H_
#define COURGETTE_IMAGE_UTILS_H_

#include "base/basictypes.h"

// COURGETTE_HISTOGRAM_TARGETS prints out a histogram of how frequently
// different target addresses are referenced. Purely for debugging.
#define COURGETTE_HISTOGRAM_TARGETS 0

namespace courgette {

typedef uint32 RVA;

// These helper functions avoid the need for casts in the main code.
inline uint16 ReadU16(const uint8* address, size_t offset) {
  return *reinterpret_cast<const uint16*>(address + offset);
}

inline uint32 ReadU32(const uint8* address, size_t offset) {
  return *reinterpret_cast<const uint32*>(address + offset);
}

inline uint64 ReadU64(const uint8* address, size_t offset) {
  return *reinterpret_cast<const uint64*>(address + offset);
}

inline uint16 Read16LittleEndian(const void* address) {
  return *reinterpret_cast<const uint16*>(address);
}

inline uint32 Read32LittleEndian(const void* address) {
  return *reinterpret_cast<const uint32*>(address);
}

inline uint64 Read64LittleEndian(const void* address) {
  return *reinterpret_cast<const uint64*>(address);
}

}  // namespace courgette

#endif  // COURGETTE_IMAGE_UTILS_H_
