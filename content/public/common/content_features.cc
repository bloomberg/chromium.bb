// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "content/public/common/content_features.h"

namespace features {

// All features in alphabetical order.

// Enables brotli "Accept-Encoding" advertising and "Content-Encoding" support.
// Brotli format specification: http://www.ietf.org/id/draft-alakuijala-brotli
const base::Feature kBrotliEncoding{"brotli-encoding",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Downloads resumption will be controllable via a flag until it's enabled
// permanently. See https://crbug.com/7648
const base::Feature kDownloadResumption{"DownloadResumption",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// The Experimental Framework for controlling access to API experiments.
const base::Feature kExperimentalFramework{"ExperimentalFramework",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Control whether the priority of a renderer is set when the process is
// launched. This is in response to a bug seen on Windows. See
// https://crbug.com/560446
const base::Feature kUpdateRendererPriorityOnStartup{
    "UpdateRendererPriorityOnStartup", base::FEATURE_ENABLED_BY_DEFAULT};

// An experimental User Agent Intervention on WebFonts loading.
const base::Feature kWebFontsIntervention{"WebFontsIntervention",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
// FeatureList definition for the Seccomp field trial.
const base::Feature kSeccompSandboxAndroid{"SeccompSandboxAndroid",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
#endif

}  // namespace features
