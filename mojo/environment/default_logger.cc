// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/environment/default_logger.h"

#include "mojo/environment/default_logger_impl.h"

namespace mojo {

const MojoLogger* GetDefaultLogger() {
  return internal::GetDefaultLoggerImpl();
}

}  // namespace mojo
