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
//   the preferred way to specify pointers in an image.
//
// In Courgette we consider two types of addresses:
// - abs32: In an image these are directly stored as VA whose locations are
//   stored in the relocation table.
// - rel32: In an image these appear in branch/call opcodes, and are represented
//   as offsets from an instruction address.

using RVA = uint32_t;
const RVA kUnassignedRVA = 0xFFFFFFFFU;
const RVA kNoRVA = 0xFFFFFFFFU;

using FileOffset = size_t;
const FileOffset kNoFileOffset = UINTPTR_MAX;

// An interface translate and read addresses. The main conversion path is:
//  (1) Location RVA.
//  (2) Location FileOffset.
//  (3) Pointer in image.
//  (4) Target VA (32-bit or 64-bit).
//  (5) Target RVA (32-bit).
// For abs32, we get (1) from relocation table, and convert to (5).
// For rel32, we get (2) from scanning opcode, and convert to (1).
class AddressTranslator {
 public:
  // (2) -> (1): Returns the RVA corresponding to |file_offset|, or kNoRVA if
  // nonexistent.
  virtual RVA FileOffsetToRVA(FileOffset file_offset) const = 0;

  // (1) -> (2): Returns the file offset corresponding to |rva|, or
  // kNoFileOffset if nonexistent.
  virtual FileOffset RVAToFileOffset(RVA rva) const = 0;

  // (2) -> (3): Returns image data pointer correspnoding to |file_offset|.
  // Assumes 0 <= |file_offset| <= image size.
  // If |file_offset| == image size, then the resulting pointer is an end bound
  // for iteration, and should not be dereferenced.
  virtual const uint8_t* FileOffsetToPointer(FileOffset file_offset) const = 0;

  // (1) -> (3): Returns the pointer to the image data for |rva|, or null if
  // |rva| is invalid.
  virtual const uint8_t* RVAToPointer(RVA rva) const = 0;

  // (3) -> (5): Returns the target RVA located at |p|, where |p| is a pointer
  // to image data.
  virtual RVA PointerToTargetRVA(const uint8_t* p) const = 0;
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
