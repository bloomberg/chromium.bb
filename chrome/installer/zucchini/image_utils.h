// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_ZUCCHINI_IMAGE_UTILS_H_
#define CHROME_INSTALLER_ZUCCHINI_IMAGE_UTILS_H_

#include <cstdint>

#include "chrome/installer/zucchini/typed_value.h"

namespace zucchini {

// offset_t is used to describe an offset in an image.
// Files bigger than 4GB are not supported.
using offset_t = uint32_t;

// Used to uniquely identify a reference group.
// Strongly typed objects are used to avoid ambiguitees with PoolTag.
struct TypeTag : public TypedValue<TypeTag, uint8_t> {
  // inheriting constructor:
  using TypedValue<TypeTag, uint8_t>::TypedValue;
};

// Used to uniquely identify a pool.
struct PoolTag : public TypedValue<PoolTag, uint8_t> {
  // inheriting constructor:
  using TypedValue<PoolTag, uint8_t>::TypedValue;
};

constexpr TypeTag kNoTypeTag(0xFF);
constexpr PoolTag kNoPoolTag(0xFF);

// Specification of references in an image file.
struct ReferenceTypeTraits {
  constexpr ReferenceTypeTraits(offset_t width,
                                TypeTag type_tag,
                                PoolTag pool_tag)
      : width(width), type_tag(type_tag), pool_tag(pool_tag) {}

  // |width| specifies number of bytes covered by the reference's binary
  // |encoding.
  offset_t width;
  // |type_tag| identifies the reference type being described.
  TypeTag type_tag = kNoTypeTag;
  // |pool_tag| identifies the pool this type belongs to.
  PoolTag pool_tag = kNoPoolTag;
};

// There is no need to store |type| because references of the same type are
// always aggregated into the same container, and so during iteration we'd have
// |type| already.
struct Reference {
  offset_t location;
  offset_t target;
};

inline bool operator==(const Reference& a, const Reference& b) {
  return a.location == b.location && a.target == b.target;
}

// An Equivalence is a block of length |length| that approximately match in
// |old_image| at an offset of |src_offset| and in |new_image| at an offset of
// |dst_offset|.
struct Equivalence {
  offset_t src_offset;
  offset_t dst_offset;
  offset_t length;
};

// Same as Equivalence, but with a similarity score.
// This is only used when generating the patch.
struct EquivalenceCandidate {
  offset_t src_offset;
  offset_t dst_offset;
  offset_t length;
  double similarity;
};

// Enumerations for supported executables.
enum ExecutableType : uint32_t {
  kExeTypeUnknown = UINT32_MAX,
  kExeTypeNoOp = 0,
  kExeTypeWin32X86 = 1,
  kExeTypeWin32X64 = 2,
  kExeTypeElfX86 = 3,
  kExeTypeElfArm32 = 4,
  kExeTypeElfAArch64 = 5,
  kExeTypeDex = 6,
};

// Descibes where to find an executable embedded in an image.
struct Element {
  ExecutableType exe_type;
  offset_t offset;
  offset_t length;
};

// A matched pair of Elements.
struct ElementMatch {
  Element old_element;
  Element new_element;
};

}  // namespace zucchini

#endif  // CHROME_INSTALLER_ZUCCHINI_IMAGE_UTILS_H_
