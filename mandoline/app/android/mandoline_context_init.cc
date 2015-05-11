// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "mandoline/app/core_services_initialization.h"

namespace mojo {
namespace runner {

void InitContext(Context* context) {
  mandoline::InitCoreServicesForContext(context);
}

}  // namespace runner
}  // namespace mojo
