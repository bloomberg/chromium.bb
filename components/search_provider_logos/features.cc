// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_provider_logos/features.h"

namespace search_provider_logos {
namespace features {

const base::Feature kThirdPartyDoodles{"ThirdPartyDoodles",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

const char kThirdPartyDoodlesOverrideUrlParam[] =
    "third-party-doodles-override-url";

}  // namespace features
}  // namespace search_provider_logos
