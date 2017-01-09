// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/model_error.h"

namespace syncer {

ModelError::ModelError() : is_set_(false) {}

ModelError::ModelError(const tracked_objects::Location& location,
                       const std::string& message)
    : is_set_(true), location_(location), message_(message) {}

ModelError::~ModelError() = default;

bool ModelError::IsSet() const {
  return is_set_;
}

const tracked_objects::Location& ModelError::location() const {
  DCHECK(IsSet());
  return location_;
}

const std::string& ModelError::message() const {
  DCHECK(IsSet());
  return message_;
}

}  // namespace syncer
