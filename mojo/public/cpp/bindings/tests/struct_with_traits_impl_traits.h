// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_STRUCT_WITH_TRAITS_IMPL_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_STRUCT_WITH_TRAITS_IMPL_TRAITS_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/strings/string_piece.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/tests/struct_with_traits_impl.h"
#include "mojo/public/interfaces/bindings/tests/struct_with_traits.mojom.h"

namespace mojo {

template <>
struct StructTraits<test::NestedStructWithTraits,
                    test::NestedStructWithTraitsImpl> {
  static bool Read(test::NestedStructWithTraitsDataView data,
                   test::NestedStructWithTraitsImpl* output);
  static int32_t value(const test::NestedStructWithTraitsImpl& input) {
    return input.value;
  }
};

template <>
struct StructTraits<test::StructWithTraits, test::StructWithTraitsImpl> {
  // Deserialization to test::StructTraitsImpl.
  static bool Read(test::StructWithTraitsDataView data,
                   test::StructWithTraitsImpl* out);

  // Fields in test::StructWithTraits.
  // See src/mojo/public/interfaces/bindings/tests/struct_with_traits.mojom.
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
    return value.get_string_as_string_piece();
  }

  static const std::string& f_string2(const test::StructWithTraitsImpl& value) {
    return value.get_string();
  }

  static const std::vector<std::string>& f_string_array(
      const test::StructWithTraitsImpl& value) {
    return value.get_string_array();
  }

  static const test::NestedStructWithTraitsImpl& f_struct(
      const test::StructWithTraitsImpl& value) {
    return value.get_struct();
  }

  static const std::vector<test::NestedStructWithTraitsImpl>& f_struct_array(
      const test::StructWithTraitsImpl& value) {
    return value.get_struct_array();
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_STRUCT_WITH_TRAITS_IMPL_TRAITS_H_
