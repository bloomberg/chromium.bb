// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"

namespace memory_coordinator {

const base::Feature kMemoryCoordinator {
  "MemoryCoordinator", base::FEATURE_DISABLED_BY_DEFAULT
};

bool IsEnabled() {
  return base::FeatureList::IsEnabled(kMemoryCoordinator);
}

void EnableForTesting() {
  base::FeatureList::ClearInstanceForTesting();
  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  feature_list->InitializeFromCommandLine(kMemoryCoordinator.name, "");
  base::FeatureList::SetInstance(std::move(feature_list));
}

}  // namespace memory_coordinator
