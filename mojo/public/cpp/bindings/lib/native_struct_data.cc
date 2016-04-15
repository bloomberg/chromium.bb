// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/native_struct_data.h"

#include "mojo/public/cpp/bindings/lib/bounds_checker.h"
#include "mojo/public/cpp/bindings/lib/buffer.h"

namespace mojo {
namespace internal {

// static
bool NativeStruct_Data::Validate(const void* data,
                                 BoundsChecker* bounds_checker) {
  const ArrayValidateParams data_validate_params(0, false, nullptr);
  return Array_Data<uint8_t>::Validate(data, bounds_checker,
                                       &data_validate_params);
}

}  // namespace internal
}  // namespace mojo
