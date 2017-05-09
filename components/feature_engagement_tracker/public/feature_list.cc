/// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/public/feature_list.h"

#include "base/feature_list.h"
#include "components/feature_engagement_tracker/public/feature_constants.h"

namespace feature_engagement_tracker {

namespace {

const base::Feature* kAllFeatures[] = {
    &kIPHDataSaverPreview, &kIPHDownloadPageFeature, &kIPHDownloadHomeFeature};

}  // namespace

std::vector<const base::Feature*> GetAllFeatures() {
  return std::vector<const base::Feature*>(
      kAllFeatures, kAllFeatures + arraysize(kAllFeatures));
}

}  // namespace feature_engagement_tracker
