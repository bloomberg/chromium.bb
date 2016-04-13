// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATE_PARAMS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATE_PARAMS_H_

#include <stdint.h>

#include "base/macros.h"

namespace mojo {
namespace internal {

using ValidateEnumFunc = bool (*)(int32_t);

class ArrayValidateParams {
 public:
  // ArrayValidateParams takes ownership of |in_element_validate params|.
  ArrayValidateParams(uint32_t in_expected_num_elements,
                      bool in_element_is_nullable,
                      ArrayValidateParams* in_element_validate_params)
      : expected_num_elements(in_expected_num_elements),
        element_is_nullable(in_element_is_nullable),
        element_validate_params(in_element_validate_params),
        validate_enum_func(nullptr) {}

  // Validates an array of enums.
  ArrayValidateParams(uint32_t in_expected_num_elements,
                      ValidateEnumFunc in_validate_enum_func)
      : expected_num_elements(in_expected_num_elements),
        element_is_nullable(false),
        element_validate_params(nullptr),
        validate_enum_func(in_validate_enum_func) {}

  ~ArrayValidateParams() {
    if (element_validate_params)
      delete element_validate_params;
  }

  // If |expected_num_elements| is not 0, the array is expected to have exactly
  // that number of elements.
  uint32_t expected_num_elements;

  // Whether the elements are nullable.
  bool element_is_nullable;

  // Validation information for elements. It is either a pointer to another
  // instance of ArrayValidateParams (if elements are arrays or maps), or
  // nullptr. In the case of maps, this is used to validate the value array.
  ArrayValidateParams* element_validate_params;

  // Validation function for enum elements.
  ValidateEnumFunc validate_enum_func;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArrayValidateParams);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATE_PARAMS_H_
