// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/tests/struct_with_traits_impl_traits.h"

namespace mojo {
namespace {

struct Context {
  int32_t value;
};

}  // namespace

// static
void* StructTraits<test::NestedStructWithTraits,
                   test::NestedStructWithTraitsImpl>::
    SetUpContext(const test::NestedStructWithTraitsImpl& input) {
  Context* context = new Context;
  context->value = input.value;
  return context;
}

// static
void StructTraits<test::NestedStructWithTraits,
                  test::NestedStructWithTraitsImpl>::
    TearDownContext(const test::NestedStructWithTraitsImpl& input,
                    void* context) {
  Context* context_obj = static_cast<Context*>(context);
  CHECK_EQ(context_obj->value, input.value);
  delete context_obj;
}

// static
int32_t StructTraits<test::NestedStructWithTraits,
                     test::NestedStructWithTraitsImpl>::
    value(const test::NestedStructWithTraitsImpl& input, void* context) {
  Context* context_obj = static_cast<Context*>(context);
  CHECK_EQ(context_obj->value, input.value);
  return input.value;
}

// static
bool StructTraits<test::NestedStructWithTraits,
                  test::NestedStructWithTraitsImpl>::
    Read(test::NestedStructWithTraits::DataView data,
         test::NestedStructWithTraitsImpl* output) {
  output->value = data.value();
  return true;
}

test::EnumWithTraits
EnumTraits<test::EnumWithTraits, test::EnumWithTraitsImpl>::ToMojom(
    test::EnumWithTraitsImpl input) {
  switch (input) {
    case test::EnumWithTraitsImpl::CUSTOM_VALUE_0:
      return test::EnumWithTraits::VALUE_0;
    case test::EnumWithTraitsImpl::CUSTOM_VALUE_1:
      return test::EnumWithTraits::VALUE_1;
  };

  NOTREACHED();
  return test::EnumWithTraits::VALUE_0;
}

bool EnumTraits<test::EnumWithTraits, test::EnumWithTraitsImpl>::FromMojom(
    test::EnumWithTraits input,
    test::EnumWithTraitsImpl* output) {
  switch (input) {
    case test::EnumWithTraits::VALUE_0:
      *output = test::EnumWithTraitsImpl::CUSTOM_VALUE_0;
      return true;
    case test::EnumWithTraits::VALUE_1:
      *output = test::EnumWithTraitsImpl::CUSTOM_VALUE_1;
      return true;
  };

  return false;
}

// static
bool StructTraits<test::StructWithTraits, test::StructWithTraitsImpl>::Read(
    test::StructWithTraits::DataView data,
    test::StructWithTraitsImpl* out) {
  test::EnumWithTraitsImpl f_enum;
  if (!data.ReadFEnum(&f_enum))
    return false;
  out->set_enum(f_enum);

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

  if (!data.ReadFStructMap(&out->get_mutable_struct_map()))
    return false;

  return true;
}

// static
bool StructTraits<test::PassByValueStructWithTraits,
                  test::PassByValueStructWithTraitsImpl>::
    Read(test::PassByValueStructWithTraits::DataView data,
         test::PassByValueStructWithTraitsImpl* out) {
  out->get_mutable_handle() = data.TakeFHandle();
  return true;
}

}  // namespace mojo
