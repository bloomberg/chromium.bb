// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/tests/struct_with_traits_impl_traits.h"

namespace mojo {

// static
bool StructTraits<test::StructWithTraits, test::StructWithTraitsImpl>::Read(
    test::StructWithTraitsDataView data,
    test::StructWithTraitsImpl* out) {
  out->set_bool(data.f_bool());
  out->set_uint32(data.f_uint32());
  out->set_uint64(data.f_uint64());
  base::StringPiece f_string, f_string2;
  if (!data.ReadFString(&f_string) || !data.ReadFString2(&f_string2))
    return false;
  out->set_string(f_string.as_string());
  return f_string == f_string2;
}

}  // namespace mojo
