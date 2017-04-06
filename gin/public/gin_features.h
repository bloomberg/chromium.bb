// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the gin
// module.

#ifndef GIN_PUBLIC_GIN_FEATURES_H_
#define GIN_PUBLIC_GIN_FEATURES_H_

#include "base/feature_list.h"
#include "gin/gin_features_export.h"

namespace features {

GIN_FEATURES_EXPORT extern const base::Feature kV8NoTurbo;
GIN_FEATURES_EXPORT extern const base::Feature kV8IgnitionLowEnd;

}  // namespace features

#endif  // GIN_PUBLIC_GIN_FEATURES_H_
