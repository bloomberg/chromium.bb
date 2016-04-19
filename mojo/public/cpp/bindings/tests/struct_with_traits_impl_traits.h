// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_STRUCT_WITH_TRAITS_IMPL_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_STRUCT_WITH_TRAITS_IMPL_TRAITS_H_

#include <stdint.h>

#include <string>

#include "base/strings/string_piece.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/tests/struct_with_traits_impl.h"

namespace mojo {
namespace test {
class StructWithTraits;
class StructWithTraits_Reader;
}

template <>
struct StructTraits<test::StructWithTraits, test::StructWithTraitsImpl> {
  // Deserialization to test::StructTraitsImpl.
  static bool Read(test::StructWithTraits_Reader r,
                   test::StructWithTraitsImpl* out);

  // Fields in test::StructWithTraits.
  // See src/mojo/public/interfaces/bindings/tests/test_native_types.mojom.
  static bool f_bool(const test::StructWithTraitsImpl& value) {
    return value.get_bool();
  }

  static uint32_t f_uint32(const test::StructWithTraitsImpl& value) {
    return value.get_uint32();
  }

  static uint64_t f_uint64(const test::StructWithTraitsImpl& value) {
    return value.get_uint64();
  }

  static base::StringPiece f_string(const test::StructWithTraitsImpl& value) {
    return value.get_string();
  }

  static base::StringPiece f_string2(const test::StructWithTraitsImpl& value) {
    return value.get_string();
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_STRUCT_WITH_TRAITS_IMPL_TRAITS_H_
