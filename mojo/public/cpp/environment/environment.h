// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_ENVIRONMENT_ENVIRONMENT_H_
#define MOJO_PUBLIC_CPP_ENVIRONMENT_ENVIRONMENT_H_

#include "mojo/public/c/environment/logger.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {

// This class must be instantiated before using any Mojo C++ APIs.
class Environment {
 public:
  Environment();
  ~Environment();

  // Note: Not thread-safe; the default logger must not be used concurrently.
  static void SetMinimumLogLevel(MojoLogLevel minimum_log_level);

 private:
  MOJO_DISALLOW_COPY_AND_ASSIGN(Environment);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_ENVIRONMENT_ENVIRONMENT_H_
