// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/tests/struct_with_traits_impl.h"

namespace mojo {
namespace test {

NestedStructWithTraitsImpl::NestedStructWithTraitsImpl() {}
NestedStructWithTraitsImpl::NestedStructWithTraitsImpl(int32_t in_value)
    : value(in_value) {}

StructWithTraitsImpl::StructWithTraitsImpl() {}

StructWithTraitsImpl::~StructWithTraitsImpl() {}

StructWithTraitsImpl::StructWithTraitsImpl(const StructWithTraitsImpl& other) =
    default;

MoveOnlyStructWithTraitsImpl::MoveOnlyStructWithTraitsImpl() {}

MoveOnlyStructWithTraitsImpl::MoveOnlyStructWithTraitsImpl(
    MoveOnlyStructWithTraitsImpl&& other) = default;

MoveOnlyStructWithTraitsImpl::~MoveOnlyStructWithTraitsImpl() {}

MoveOnlyStructWithTraitsImpl& MoveOnlyStructWithTraitsImpl::operator=(
    MoveOnlyStructWithTraitsImpl&& other) = default;

UnionWithTraitsInt32::~UnionWithTraitsInt32() {}

UnionWithTraitsStruct::~UnionWithTraitsStruct() {}

}  // namespace test
}  // namespace mojo
