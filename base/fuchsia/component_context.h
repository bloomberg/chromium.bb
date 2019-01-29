// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_COMPONENT_CONTEXT_H_
#define BASE_FUCHSIA_COMPONENT_CONTEXT_H_

#include <lib/zx/channel.h>
#include <utility>

#include "base/fuchsia/service_directory_client.h"

namespace base {
namespace fuchsia {

// TODO(https://crbug.com/920920): Remove these once all call-sites are updated.
using ComponentContext = ServiceDirectoryClient;
using ScopedDefaultComponentContext =
    ScopedServiceDirectoryClientForCurrentProcessForTest;

}  // namespace fuchsia
}  // namespace base

#endif  // BASE_FUCHSIA_COMPONENT_CONTEXT_H_
