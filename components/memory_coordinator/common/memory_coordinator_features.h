// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_COORDINATOR_COMMON_MEMORY_COORDINATOR_FEATURES_H_
#define COMPONENTS_MEMORY_COORDINATOR_COMMON_MEMORY_COORDINATOR_FEATURES_H_

#include "base/feature_list.h"
#include "components/memory_coordinator/common/memory_coordinator_export.h"

// WARNING:
// The memory coordinator is not ready for use and enabling this may cause
// unexpected memory regression at this point. Please do not enable this.

namespace memory_coordinator {

// Returns true when the memory coordinator is enabled.
MEMORY_COORDINATOR_EXPORT bool IsEnabled();

// Enables the memory coordinator for testing.
MEMORY_COORDINATOR_EXPORT void EnableForTesting();

}  // memory_coordinator

#endif  // COMPONENTS_MEMORY_COORDINATOR_COMMON_MEMORY_COORDINATOR_FEATURES_H_
