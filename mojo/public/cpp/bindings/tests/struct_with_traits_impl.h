// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_STRUCT_WITH_TRAITS_IMPL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_STRUCT_WITH_TRAITS_IMPL_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/strings/string_piece.h"

namespace mojo {
namespace test {

struct NestedStructWithTraitsImpl {
 public:
  bool operator==(const NestedStructWithTraitsImpl& other) const {
    return value == other.value;
  }

  int32_t value = 0;
};

// A type which knows how to look like a mojo::test::StructWithTraits mojom type
// by way of mojo::StructTraits.
class StructWithTraitsImpl {
 public:
  StructWithTraitsImpl();
  ~StructWithTraitsImpl();

  void set_bool(bool value) { bool_ = value; }
  bool get_bool() const { return bool_; }

  void set_uint32(uint32_t value) { uint32_ = value; }
  uint32_t get_uint32() const { return uint32_; }

  void set_uint64(uint64_t value) { uint64_ = value; }
  uint64_t get_uint64() const { return uint64_; }

  void set_string(std::string value) { string_ = value; }
  base::StringPiece get_string_as_string_piece() const { return string_; }
  const std::string& get_string() const { return string_; }

  const std::vector<std::string>& get_string_array() const {
    return string_array_;
  }
  std::vector<std::string>& get_mutable_string_array() { return string_array_; }

  const NestedStructWithTraitsImpl& get_struct() const { return struct_; }
  NestedStructWithTraitsImpl& get_mutable_struct() { return struct_; }

  const std::vector<NestedStructWithTraitsImpl>& get_struct_array() const {
    return struct_array_;
  }
  std::vector<NestedStructWithTraitsImpl>& get_mutable_struct_array() {
    return struct_array_;
  }

 private:
  bool bool_ = false;
  uint32_t uint32_ = 0;
  uint64_t uint64_ = 0;
  std::string string_;
  std::vector<std::string> string_array_;
  NestedStructWithTraitsImpl struct_;
  std::vector<NestedStructWithTraitsImpl> struct_array_;
};

}  // namespace test
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_STRUCT_WITH_TRAITS_IMPL_H_
