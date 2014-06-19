// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/prefetch_field_trial.h"

#include <string>

#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "chrome/browser/prerender/prerender_field_trial.h"

namespace prefetch {

bool IsPrefetchFieldTrialEnabled() {
  std::string experiment = base::FieldTrialList::FindFullName("Prefetch");
  if (StartsWithASCII(experiment, "ExperimentYes", false))
    return true;
  // If this client needs to prefetch for the Prerender Local Predictor,
  // enable prefetching.
  if (prerender::IsLocalPredictorPrerenderPrefetchEnabled())
    return true;
  return false;
}

}  // namespace prefetch
