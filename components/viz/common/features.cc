// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/features.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "components/viz/common/switches.h"

namespace features {

const base::Feature kEnableSurfaceSynchronization{
    "SurfaceSynchronization", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsSurfaceSynchronizationEnabled() {
  return base::FeatureList::IsEnabled(kEnableSurfaceSynchronization) ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kEnableSurfaceSynchronization);
}

}  // namespace features
