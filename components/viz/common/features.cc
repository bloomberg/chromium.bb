// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/features.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "components/viz/common/switches.h"

namespace features {

#if defined(USE_AURA)
const base::Feature kEnableSurfaceSynchronization{
    "SurfaceSynchronization", base::FEATURE_ENABLED_BY_DEFAULT};
#else
const base::Feature kEnableSurfaceSynchronization{
    "SurfaceSynchronization", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

bool IsSurfaceSynchronizationEnabled() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  return base::FeatureList::IsEnabled(kEnableSurfaceSynchronization) ||
         command_line->HasSwitch(switches::kEnableSurfaceSynchronization) ||
         command_line->HasSwitch(switches::kEnableViz);
}

}  // namespace features
