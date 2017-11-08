// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_PROVIDER_LOGOS_FEATURES_H_
#define COMPONENTS_SEARCH_PROVIDER_LOGOS_FEATURES_H_

#include "base/feature_list.h"

namespace search_provider_logos {
namespace features {

// If enabled, Doodles are fetched for third-party search engines that specify
// a doodle_url in prepopulated_engines.json.
extern const base::Feature kThirdPartyDoodles;

// This parameter can be used to override the URL of the doodle API for
// third-party search engines. Useful for testing.
extern const char kThirdPartyDoodlesOverrideUrlParam[];

}  // namespace features
}  // namespace search_provider_logos

#endif  // COMPONENTS_SEARCH_PROVIDER_LOGOS_FEATURES_H_
