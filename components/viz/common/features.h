// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_FEATURES_H_
#define COMPONENTS_VIZ_COMMON_FEATURES_H_

#include "components/viz/common/viz_common_export.h"

#include "base/feature_list.h"

namespace features {

VIZ_COMMON_EXPORT extern const base::Feature kEnableSurfaceSynchronization;
VIZ_COMMON_EXPORT extern const base::Feature kVizDisplayCompositor;

VIZ_COMMON_EXPORT bool IsSurfaceSynchronizationEnabled();

}  // namespace features

#endif  // COMPONENTS_VIZ_COMMON_FEATURES_H_
