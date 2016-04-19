// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_STRUCT_WITH_TRAITS_IMPL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_STRUCT_WITH_TRAITS_IMPL_H_

#include <stdint.h>

#include <string>

#include "base/strings/string_piece.h"

namespace mojo {
namespace test {

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
  base::StringPiece get_string() const { return string_; }

 private:
  bool bool_ = false;
  uint32_t uint32_ = 0;
  uint64_t uint64_ = 0;
  std::string string_;
};

}  // namespace test
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_STRUCT_WITH_TRAITS_IMPL_H_
