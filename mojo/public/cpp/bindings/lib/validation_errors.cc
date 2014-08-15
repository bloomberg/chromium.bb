// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/validation_errors.h"

#include "mojo/public/cpp/environment/logging.h"

namespace mojo {
namespace internal {
namespace {

ValidationErrorObserverForTesting* g_validation_error_observer = NULL;

}  // namespace

const char* ValidationErrorToString(ValidationError error) {
  switch (error) {
    case VALIDATION_ERROR_NONE:
      return "VALIDATION_ERROR_NONE";
    case VALIDATION_ERROR_MISALIGNED_OBJECT:
      return "VALIDATION_ERROR_MISALIGNED_OBJECT";
    case VALIDATION_ERROR_ILLEGAL_MEMORY_RANGE:
      return "VALIDATION_ERROR_ILLEGAL_MEMORY_RANGE";
    case VALIDATION_ERROR_UNEXPECTED_STRUCT_HEADER:
      return "VALIDATION_ERROR_UNEXPECTED_STRUCT_HEADER";
    case VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER:
      return "VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER";
    case VALIDATION_ERROR_ILLEGAL_HANDLE:
      return "VALIDATION_ERROR_ILLEGAL_HANDLE";
    case VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE:
      return "VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE";
    case VALIDATION_ERROR_ILLEGAL_POINTER:
      return "VALIDATION_ERROR_ILLEGAL_POINTER";
    case VALIDATION_ERROR_UNEXPECTED_NULL_POINTER:
      return "VALIDATION_ERROR_UNEXPECTED_NULL_POINTER";
    case VALIDATION_ERROR_MESSAGE_HEADER_INVALID_FLAG_COMBINATION:
      return "VALIDATION_ERROR_MESSAGE_HEADER_INVALID_FLAG_COMBINATION";
    case VALIDATION_ERROR_MESSAGE_HEADER_MISSING_REQUEST_ID:
      return "VALIDATION_ERROR_MESSAGE_HEADER_MISSING_REQUEST_ID";
  }

  return "Unknown error";
}

void ReportValidationError(ValidationError error) {
  if (g_validation_error_observer)
    g_validation_error_observer->set_last_error(error);
  else
    MOJO_LOG(ERROR) << "Invalid message: " << ValidationErrorToString(error);
}

ValidationErrorObserverForTesting::ValidationErrorObserverForTesting()
    : last_error_(VALIDATION_ERROR_NONE) {
  MOJO_DCHECK(!g_validation_error_observer);
  g_validation_error_observer = this;
  MOJO_LOG(WARNING) << "Non-nullable validation is turned on for testing but "
                    << "not for production code yet!";
}

ValidationErrorObserverForTesting::~ValidationErrorObserverForTesting() {
  MOJO_DCHECK(g_validation_error_observer == this);
  g_validation_error_observer = NULL;
}

bool IsNonNullableValidationEnabled() {
  return !!g_validation_error_observer;
}

}  // namespace internal
}  // namespace mojo
