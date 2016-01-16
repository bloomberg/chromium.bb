// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the content
// module.

#ifndef CONTENT_PUBLIC_COMMON_CONTENT_FEATURES_H_
#define CONTENT_PUBLIC_COMMON_CONTENT_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.
CONTENT_EXPORT extern const base::Feature kBrotliEncoding;
CONTENT_EXPORT extern const base::Feature kDownloadResumption;
CONTENT_EXPORT extern const base::Feature kExperimentalFramework;
CONTENT_EXPORT extern const base::Feature kUpdateRendererPriorityOnStartup;
CONTENT_EXPORT extern const base::Feature kWebFontsIntervention;

#if defined(OS_ANDROID)
CONTENT_EXPORT extern const base::Feature kSeccompSandboxAndroid;
#endif  // defined(OS_ANDROID)

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

}  // namespace features

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_FEATURES_H_
