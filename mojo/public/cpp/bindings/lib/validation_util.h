// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATION_UTIL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATION_UTIL_H_

#include <stdint.h>

#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/bounds_checker.h"
#include "mojo/public/cpp/bindings/lib/serialization_util.h"
#include "mojo/public/cpp/bindings/lib/validate_params.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {
namespace internal {

// Checks whether decoding the pointer will overflow and produce a pointer
// smaller than |offset|.
bool ValidateEncodedPointer(const uint64_t* offset);

// Validates that |data| contains a valid struct header, in terms of alignment
// and size (i.e., the |num_bytes| field of the header is sufficient for storing
// the header itself). Besides, it checks that the memory range
// [data, data + num_bytes) is not marked as occupied by other objects in
// |bounds_checker|. On success, the memory range is marked as occupied.
// Note: Does not verify |version| or that |num_bytes| is correct for the
// claimed version.
bool ValidateStructHeaderAndClaimMemory(const void* data,
                                        BoundsChecker* bounds_checker);

// Validates that |data| contains a valid union header, in terms of alignment
// and size. If not inlined, it checks that the memory range
// [data, data + num_bytes) is not marked as occupied by other objects in
// |bounds_checker|. On success, the memory range is marked as occupied.
bool ValidateUnionHeaderAndClaimMemory(const void* data,
                                       bool inlined,
                                       BoundsChecker* bounds_checker);

// Validates that the message is a request which doesn't expect a response.
bool ValidateMessageIsRequestWithoutResponse(const Message* message);
// Validates that the message is a request expecting a response.
bool ValidateMessageIsRequestExpectingResponse(const Message* message);
// Validates that the message is a response.
bool ValidateMessageIsResponse(const Message* message);

// Validates that the message payload is a valid struct of type ParamsType.
template <typename ParamsType>
bool ValidateMessagePayload(const Message* message) {
  BoundsChecker bounds_checker(message->payload(), message->payload_num_bytes(),
                               message->handles()->size());
  return ParamsType::Validate(message->payload(), &bounds_checker);
}

// The following methods validate control messages defined in
// interface_control_messages.mojom.
bool ValidateControlRequest(const Message* message);
bool ValidateControlResponse(const Message* message);

// The following Validate.*NonNullable() functions validate that the given
// |input| is not null/invalid.
template <typename T>
bool ValidatePointerNonNullable(const T& input, const char* error_message) {
  if (input.offset)
    return true;

  ReportValidationError(VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
                        error_message);
  return false;
}

template <typename T>
bool ValidateInlinedUnionNonNullable(const T& input,
                                     const char* error_message) {
  if (!input.is_null())
    return true;

  ReportValidationError(VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
                        error_message);
  return false;
}

bool ValidateHandleNonNullable(const Handle_Data& input,
                               const char* error_message);

bool ValidateInterfaceIdNonNullable(InterfaceId input,
                                    const char* error_message);

template <typename T>
bool ValidateArray(const Pointer<Array_Data<T>>& input,
                   BoundsChecker* bounds_checker,
                   const ArrayValidateParams* validate_params) {
  if (!ValidateEncodedPointer(&input.offset)) {
    ReportValidationError(VALIDATION_ERROR_ILLEGAL_POINTER);
    return false;
  }

  return Array_Data<T>::Validate(DecodePointerRaw(&input.offset),
                                 bounds_checker, validate_params);
}

template <typename T>
bool ValidateMap(const Pointer<T>& input,
                 BoundsChecker* bounds_checker,
                 const ArrayValidateParams* value_validate_params) {
  if (!ValidateEncodedPointer(&input.offset)) {
    ReportValidationError(VALIDATION_ERROR_ILLEGAL_POINTER);
    return false;
  }

  return T::Validate(DecodePointerRaw(&input.offset), bounds_checker,
                     value_validate_params);
}

template <typename T>
bool ValidateStruct(const Pointer<T>& input, BoundsChecker* bounds_checker) {
  if (!ValidateEncodedPointer(&input.offset)) {
    ReportValidationError(VALIDATION_ERROR_ILLEGAL_POINTER);
    return false;
  }

  return T::Validate(DecodePointerRaw(&input.offset), bounds_checker);
}

template <typename T>
bool ValidateInlinedUnion(const T& input, BoundsChecker* bounds_checker) {
  return T::Validate(&input, bounds_checker, true);
}

template <typename T>
bool ValidateNonInlinedUnion(const Pointer<T>& input,
                             BoundsChecker* bounds_checker) {
  if (!ValidateEncodedPointer(&input.offset)) {
    ReportValidationError(VALIDATION_ERROR_ILLEGAL_POINTER);
    return false;
  }

  return T::Validate(DecodePointerRaw(&input.offset), bounds_checker, false);
}

bool ValidateHandle(const Handle_Data& input, BoundsChecker* bounds_checker);

bool ValidateAssociatedInterfaceId(InterfaceId input);

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATION_UTIL_H_
