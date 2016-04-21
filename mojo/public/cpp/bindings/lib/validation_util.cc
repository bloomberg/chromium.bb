// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/validation_util.h"

#include <stdint.h>

#include <limits>

#include "mojo/public/cpp/bindings/lib/bindings_serialization.h"
#include "mojo/public/cpp/bindings/lib/message_internal.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"
#include "mojo/public/interfaces/bindings/interface_control_messages.mojom.h"

namespace mojo {
namespace internal {

bool ValidateEncodedPointer(const uint64_t* offset) {
  // - Make sure |*offset| is no more than 32-bits.
  // - Cast |offset| to uintptr_t so overflow behavior is well defined across
  //   32-bit and 64-bit systems.
  return *offset <= std::numeric_limits<uint32_t>::max() &&
         (reinterpret_cast<uintptr_t>(offset) +
              static_cast<uint32_t>(*offset) >=
          reinterpret_cast<uintptr_t>(offset));
}

bool ValidateStructHeaderAndClaimMemory(const void* data,
                                        BoundsChecker* bounds_checker) {
  if (!IsAligned(data)) {
    ReportValidationError(VALIDATION_ERROR_MISALIGNED_OBJECT);
    return false;
  }
  if (!bounds_checker->IsValidRange(data, sizeof(StructHeader))) {
    ReportValidationError(VALIDATION_ERROR_ILLEGAL_MEMORY_RANGE);
    return false;
  }

  const StructHeader* header = static_cast<const StructHeader*>(data);

  if (header->num_bytes < sizeof(StructHeader)) {
    ReportValidationError(VALIDATION_ERROR_UNEXPECTED_STRUCT_HEADER);
    return false;
  }

  if (!bounds_checker->ClaimMemory(data, header->num_bytes)) {
    ReportValidationError(VALIDATION_ERROR_ILLEGAL_MEMORY_RANGE);
    return false;
  }

  return true;
}

bool ValidateMessageIsRequestWithoutResponse(const Message* message) {
  if (message->has_flag(kMessageIsResponse) ||
      message->has_flag(kMessageExpectsResponse)) {
    ReportValidationError(VALIDATION_ERROR_MESSAGE_HEADER_INVALID_FLAGS);
    return false;
  }
  return true;
}

bool ValidateMessageIsRequestExpectingResponse(const Message* message) {
  if (message->has_flag(kMessageIsResponse) ||
      !message->has_flag(kMessageExpectsResponse)) {
    ReportValidationError(VALIDATION_ERROR_MESSAGE_HEADER_INVALID_FLAGS);
    return false;
  }
  return true;
}

bool ValidateMessageIsResponse(const Message* message) {
  if (message->has_flag(kMessageExpectsResponse) ||
      !message->has_flag(kMessageIsResponse)) {
    ReportValidationError(VALIDATION_ERROR_MESSAGE_HEADER_INVALID_FLAGS);
    return false;
  }
  return true;
}

bool ValidateControlRequest(const Message* message) {
  switch (message->header()->name) {
    case kRunMessageId:
      return ValidateMessageIsRequestExpectingResponse(message) &&
             ValidateMessagePayload<RunMessageParams_Data>(message);
    case kRunOrClosePipeMessageId:
      return ValidateMessageIsRequestWithoutResponse(message) &&
             ValidateMessagePayload<RunOrClosePipeMessageParams_Data>(message);
  }
  return false;
}

bool ValidateControlResponse(const Message* message) {
  if (!ValidateMessageIsResponse(message))
    return false;
  switch (message->header()->name) {
    case kRunMessageId:
      return ValidateMessagePayload<RunResponseMessageParams_Data>(message);
  }
  return false;
}

bool ValidateHandleNonNullable(const Handle& input, const char* error_message) {
  if (input.value() != kEncodedInvalidHandleValue)
    return true;

  ReportValidationError(VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE,
                        error_message);
  return false;
}

bool ValidateInterfaceIdNonNullable(InterfaceId input,
                                    const char* error_message) {
  if (IsValidInterfaceId(input))
    return true;

  ReportValidationError(VALIDATION_ERROR_UNEXPECTED_INVALID_INTERFACE_ID,
                        error_message);
  return false;
}

bool ValidateHandle(const Handle& input, BoundsChecker* bounds_checker) {
  if (bounds_checker->ClaimHandle(input))
    return true;

  ReportValidationError(VALIDATION_ERROR_ILLEGAL_HANDLE);
  return false;
}

bool ValidateAssociatedInterfaceId(InterfaceId input) {
  if (!IsMasterInterfaceId(input))
    return true;

  ReportValidationError(VALIDATION_ERROR_ILLEGAL_INTERFACE_ID);
  return false;
}

}  // namespace internal
}  // namespace mojo
