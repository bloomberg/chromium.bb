// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/features.h"

#include "ios/web/public/features.h"

namespace reading_list {

const base::Feature kOfflineVersionWithoutNativeContent{
    "OfflineVersionWithoutNativeContent", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsOfflinePageWithoutNativeContentEnabled() {
  return base::FeatureList::IsEnabled(kOfflineVersionWithoutNativeContent);
}

}  // namespace reading_list
