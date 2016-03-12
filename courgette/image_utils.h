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

// There are several ways to reason about addresses in an image:
// - File Offset: Position relative to start of image.
// - VA (Virtual Address): Virtual memory address of a loaded image. This is
//   subject to relocation by the OS.
// - RVA (Relative Virtual Address): VA relative to some base address. This is
//   the preferred way to specify pointers in an image. Two ways to encode RVA
//   are:
//   - abs32: RVA value is encoded directly.
//   - rel32: RVA is encoded as offset from an instruction address. This is
//     commonly used for relative branch/call opcodes.
// Courgette operates on File Offsets and RVAs only.

using RVA = uint32_t;
const RVA kUnassignedRVA = 0xFFFFFFFFU;
const RVA kNoRVA = 0xFFFFFFFFU;

using FileOffset = size_t;
const FileOffset kNoFileOffset = UINTPTR_MAX;

// An interface for {File Offset, RVA, pointer to image data} translation.
class AddressTranslator {
 public:
  // Returns the RVA corresponding to |file_offset|, or kNoRVA if nonexistent.
  virtual RVA FileOffsetToRVA(FileOffset file_offset) const = 0;

  // Returns the file offset corresponding to |rva|, or kNoFileOffset if
  // nonexistent.
  virtual FileOffset RVAToFileOffset(RVA rva) const = 0;

  // Returns the pointer to the image data for |file_offset|. Assumes that
  // 0 <= |file_offset| <= image size. If |file_offset| == image, the resulting
  // pointer is an end bound for iteration that should never be dereferenced.
  virtual const uint8_t* FileOffsetToPointer(FileOffset file_offset) const = 0;

  // Returns the pointer to the image data for |rva|, or null if |rva| is
  // invalid.
  virtual const uint8_t* RVAToPointer(RVA rva) const = 0;
};

// A Label is a symbolic reference to an address.  Unlike a conventional
// assembly language, we always know the address.  The address will later be
// stored in a table and the Label will be replaced with the index into the
// table.
// TODO(huangs): Make this a struct, and remove "_" from member names.
class Label {
 public:
  enum : int { kNoIndex = -1 };
  explicit Label(RVA rva) : rva_(rva) {}
  Label(RVA rva, int index) : rva_(rva), index_(index) {}
  Label(RVA rva, int index, int32_t count)
      : rva_(rva), index_(index), count_(count) {}

  bool operator==(const Label& other) const {
    return rva_ == other.rva_ && index_ == other.index_ &&
           count_ == other.count_;
  }

  RVA rva_ = kUnassignedRVA;  // Address referred to by the label.
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
