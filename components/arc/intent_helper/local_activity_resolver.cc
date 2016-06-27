// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/local_activity_resolver.h"

#include "url/gurl.h"

namespace {

constexpr char kIntentActionView[] = "android.intent.action.VIEW";
constexpr char kIntentCategoryBrowsable[] = "android.intent.category.BROWSABLE";
constexpr char kSchemeHttp[] = "http";
constexpr char kSchemeHttps[] = "https";

}  // namespace

namespace arc {

LocalActivityResolver::LocalActivityResolver() = default;

LocalActivityResolver::~LocalActivityResolver() = default;

bool LocalActivityResolver::ShouldChromeHandleUrl(const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    // Chrome will handle everything that is not http and https.
    return true;
  }

  for (const mojom::IntentFilterPtr& filter : intent_filters_) {
    if (IsRelevantIntentFilter(filter)) {
      // For now err on the side of caution and let Android
      // handle cases where there are possible matching intent
      // filters.
      return false;
    }
  }

  // Didn't find any matches for Android so let Chrome handle it.
  return true;
}

void LocalActivityResolver::UpdateIntentFilters(
    mojo::Array<mojom::IntentFilterPtr> intent_filters) {
  intent_filters_ = std::move(intent_filters);
}

bool LocalActivityResolver::IsRelevantIntentFilter(
    const mojom::IntentFilterPtr& intent_filter) {
  return FilterHasViewAction(intent_filter) &&
         FilterCategoryIsBrowsable(intent_filter) &&
         FilterHandlesWebSchemes(intent_filter);
}

bool LocalActivityResolver::FilterHasViewAction(
    const mojom::IntentFilterPtr& intent_filter) {
  for (const mojo::String& action : intent_filter->actions) {
    if (action == kIntentActionView) {
      return true;
    }
  }

  return false;
}

bool LocalActivityResolver::FilterCategoryIsBrowsable(
    const mojom::IntentFilterPtr& intent_filter) {
  for (const mojo::String& category : intent_filter->categories) {
    if (category == kIntentCategoryBrowsable) {
      return true;
    }
  }

  return false;
}

bool LocalActivityResolver::FilterHandlesWebSchemes(
    const mojom::IntentFilterPtr& intent_filter) {
  for (const mojo::String& scheme : intent_filter->data_schemes) {
    if (scheme == kSchemeHttp || scheme == kSchemeHttps) {
      return true;
    }
  }

  return false;
}

}  // namespace arc
