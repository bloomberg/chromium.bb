// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_ENVIRONMENT_ENVIRONMENT_H_
#define MOJO_PUBLIC_ENVIRONMENT_ENVIRONMENT_H_

#include "mojo/public/c/system/macros.h"

namespace mojo {

// This class must be instantiated before using any Mojo APIs.
class Environment {
 public:
  Environment();
  ~Environment();

 private:
  MOJO_DISALLOW_COPY_AND_ASSIGN(Environment);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_ENVIRONMENT_ENVIRONMENT_H_
