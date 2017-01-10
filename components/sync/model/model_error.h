// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_MODEL_ERROR_H_
#define COMPONENTS_SYNC_MODEL_MODEL_ERROR_H_

#include <string>

#include "base/callback.h"
#include "base/location.h"

namespace syncer {

// A minimal error object for use by USS model type code.
class ModelError {
 public:
  // Creates a set error object with the given location and message.
  ModelError(const tracked_objects::Location& location,
             const std::string& message);

  ~ModelError();

  // The location of the error this object represents. Can only be called if the
  // error is set.
  const tracked_objects::Location& location() const;

  // The message explaining the error this object represents. Can only be called
  // if the error is set.
  const std::string& message() const;

 private:
  tracked_objects::Location location_;
  std::string message_;
};

// Typedef for a simple error handler callback.
using ModelErrorHandler = base::RepeatingCallback<void(const ModelError&)>;

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_MODEL_ERROR_H_
