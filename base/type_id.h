// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TYPE_ID_H_
#define BASE_TYPE_ID_H_

#include <stdint.h>
#include <string>

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"

namespace base {
namespace internal {

// Make sure dummy_var has default visibility since we need to make sure that
// there is only one definition across all libraries (shared or static).
template <typename Type>
struct __attribute__((visibility("default"))) TypeTag {
  constexpr static char dummy_var = 0;
};

template <typename Type>
constexpr char TypeTag<Type>::dummy_var;

// Returns a compile-time unique pointer for the passed in type.
template <typename Type>
constexpr inline const void* UniqueIdFromType() {
  return &TypeTag<Type>::dummy_var;
}

}  // namespace internal

// A substitute for RTTI that uses the linker to uniquely reserve an address in
// the binary for each type.
class BASE_EXPORT TypeId {
 public:
  template <typename T>
  constexpr static TypeId From() {
    return TypeId(PRETTY_FUNCTION, internal::UniqueIdFromType<T>());
  }

  constexpr TypeId() : TypeId(nullptr, "") {}

  constexpr TypeId(const TypeId& other) = default;
  TypeId& operator=(const TypeId& other) = default;

  constexpr bool operator==(TypeId other) const {
    return unique_type_id_ == other.unique_type_id_;
  }

  constexpr bool operator!=(TypeId other) const { return !(*this == other); }

  std::string ToString() const;

 private:
  constexpr TypeId(const char* function_name, const void* unique_type_id)
      :
#if DCHECK_IS_ON()
        function_name_(function_name),
#endif
        unique_type_id_(unique_type_id) {
  }

  uintptr_t internal_unique_id() const {
    return reinterpret_cast<uintptr_t>(unique_type_id_);
  }

#if DCHECK_IS_ON()
  const char* function_name_;
#endif
  const void* unique_type_id_;
};

BASE_EXPORT std::ostream& operator<<(std::ostream& out, const TypeId& type_id);

}  // namespace base

#endif  // BASE_TYPE_ID_H_
