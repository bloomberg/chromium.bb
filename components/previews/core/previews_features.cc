// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_features.h"

namespace previews {
namespace features {

// Enables the Offline previews on Android.
const base::Feature kOfflinePreviews{"OfflinePreviews",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the Client Lo-Fi previews on Android.
const base::Feature kClientLoFi{"ClientLoFi",
                                base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the Stale Previews timestamp on Previews infobars.
const base::Feature kStalePreviewsTimestamp{"StalePreviewsTimestamp",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace previews
