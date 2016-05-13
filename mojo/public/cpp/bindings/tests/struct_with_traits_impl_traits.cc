// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/tests/struct_with_traits_impl_traits.h"

namespace mojo {

// static
bool StructTraits<test::NestedStructWithTraits,
                  test::NestedStructWithTraitsImpl>::
    Read(test::NestedStructWithTraitsDataView data,
         test::NestedStructWithTraitsImpl* output) {
  output->value = data.value();
  return true;
}

// static
bool StructTraits<test::StructWithTraits, test::StructWithTraitsImpl>::Read(
    test::StructWithTraitsDataView data,
    test::StructWithTraitsImpl* out) {
  out->set_bool(data.f_bool());
  out->set_uint32(data.f_uint32());
  out->set_uint64(data.f_uint64());

  base::StringPiece f_string;
  std::string f_string2;
  if (!data.ReadFString(&f_string) || !data.ReadFString2(&f_string2) ||
      f_string != f_string2) {
    return false;
  }
  out->set_string(f_string2);

  if (!data.ReadFStringArray(&out->get_mutable_string_array()))
    return false;

  if (!data.ReadFStruct(&out->get_mutable_struct()))
    return false;

  if (!data.ReadFStructArray(&out->get_mutable_struct_array()))
    return false;

  return true;
}

}  // namespace mojo
