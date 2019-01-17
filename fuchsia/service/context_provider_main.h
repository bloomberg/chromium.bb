// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_SERVICE_CONTEXT_PROVIDER_MAIN_H_
#define FUCHSIA_SERVICE_CONTEXT_PROVIDER_MAIN_H_

#include "fuchsia/common/fuchsia_export.h"

namespace webrunner {

// Main function for the process that implements web::ContextProvider interface.
// Called by WebRunnerMainDelegate when the process is started without --type
// argument.
FUCHSIA_EXPORT int ContextProviderMain();

}  // namespace webrunner

#endif  // FUCHSIA_SERVICE_CONTEXT_PROVIDER_MAIN_H_