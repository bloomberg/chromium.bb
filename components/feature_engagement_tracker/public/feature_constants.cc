/// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/public/feature_constants.h"

#include "base/feature_list.h"

namespace feature_engagement_tracker {

const base::Feature kIPHDemoMode{"IPH_DemoMode",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kIPHDataSaverPreview{"IPH_DataSaverPreview",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHDownloadPageFeature{"IPH_DownloadPage",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHDownloadHomeFeature{"IPH_DownloadHome",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace feature_engagement_tracker
