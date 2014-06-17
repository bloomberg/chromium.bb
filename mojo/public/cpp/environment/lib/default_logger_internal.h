// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_ENVIRONMENT_LIB_DEFAULT_LOGGER_INTERNAL_H_
#define MOJO_PUBLIC_CPP_ENVIRONMENT_LIB_DEFAULT_LOGGER_INTERNAL_H_

#include "mojo/public/c/environment/logger.h"

namespace mojo {
namespace internal {

// Sets the minimum log level (which will always be at most
// |MOJO_LOG_LEVEL_FATAL|).
void SetMinimumLogLevel(MojoLogLevel minimum_log_level);

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_ENVIRONMENT_LIB_DEFAULT_LOGGER_INTERNAL_H_
