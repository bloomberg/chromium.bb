// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/ntp_features.h"

#include "build/build_config.h"
#include "ui/base/ui_base_features.h"

namespace features {

// If enabled, the user will see Doodles on the New Tab Page.
const base::Feature kDoodlesOnLocalNtp{"DoodlesOnLocalNtp",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the user will sometimes see promos on the NTP.
const base::Feature kPromosOnLocalNtp{"PromosOnLocalNtp",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the fakebox will not be shown on the NTP.
const base::Feature kRemoveNtpFakebox{"RemoveNtpFakebox",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the user will sometimes see search suggestions on the NTP.
const base::Feature kSearchSuggestionsOnLocalNtp{
    "SearchSuggestionsOnLocalNtp", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables using the local NTP if Google is the default search engine.
const base::Feature kUseGoogleLocalNtp{"UseGoogleLocalNtp",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
