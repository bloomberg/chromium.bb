// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/tests/struct_with_traits_impl_traits.h"
#include "mojo/public/interfaces/bindings/tests/struct_with_traits.mojom.h"

namespace mojo {

// static
bool StructTraits<test::StructWithTraits, test::StructWithTraitsImpl>::Read(
    test::StructWithTraits_Reader r,
    test::StructWithTraitsImpl* out) {
  out->set_bool(r.f_bool());
  out->set_uint32(r.f_uint32());
  out->set_uint64(r.f_uint64());
  out->set_string(r.f_string().as_string());
  return true;
}

}  // namespace mojo
