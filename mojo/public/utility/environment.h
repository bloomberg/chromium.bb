// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_UTILITY_ENVIRONMENT_H_
#define MOJO_PUBLIC_UTILITY_ENVIRONMENT_H_

#include "mojo/public/system/macros.h"

namespace mojo {

namespace internal {
class BindingsSupportImpl;
}  // namespace internal

class RunLoop;

// Use Environment to cofigure state.
class Environment {
 public:
  Environment();
  ~Environment();

 private:
  internal::BindingsSupportImpl* bindings_support_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(Environment);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_UTILITY_ENVIRONMENT_H_
