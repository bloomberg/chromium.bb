// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_PROVIDER_LOGOS_FEATURES_H_
#define COMPONENTS_SEARCH_PROVIDER_LOGOS_FEATURES_H_

#include "base/feature_list.h"

namespace search_provider_logos {
namespace features {

// If enabled, Google Doodles are fetched from the newer /ddljson API instead of
// the /newtab_mobile API.
extern const base::Feature kUseDdljsonApi;

// This parameter can be used to override the URL of the /ddljson API. Useful
// for testing.
extern const char kDdljsonOverrideUrlParam[];

// If enabled, Doodles are fetched for third-party search engines that specify
// a doodle_url in prepopulated_engines.json.
extern const base::Feature kThirdPartyDoodles;

}  // namespace features
}  // namespace search_provider_logos

#endif  // COMPONENTS_SEARCH_PROVIDER_LOGOS_FEATURES_H_
