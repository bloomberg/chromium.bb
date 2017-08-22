// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_provider_logos/features.h"

namespace search_provider_logos {
namespace features {

const base::Feature kUseDdljsonApi{"UseDdljsonApi",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

const char kDdljsonOverrideUrlParam[] = "ddljson-override-url";

const base::Feature kThirdPartyDoodles{"ThirdPartyDoodles",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace search_provider_logos
