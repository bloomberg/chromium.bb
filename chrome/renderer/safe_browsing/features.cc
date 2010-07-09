// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/features.h"

#include "base/histogram.h"
#include "base/logging.h"

namespace safe_browsing {

const size_t FeatureMap::kMaxFeatureMapSize = 10000;

FeatureMap::FeatureMap() {}
FeatureMap::~FeatureMap() {}

bool FeatureMap::AddBooleanFeature(const std::string& name) {
  if (features_.size() >= kMaxFeatureMapSize) {
    // If we hit this case, it indicates that either kMaxFeatureMapSize is
    // too small, or there is a bug causing too many features to be added.
    // In this case, we'll log to a histogram so we can see that this is
    // happening, and make phishing classification fail silently.
    LOG(ERROR) << "Not adding feature: " << name << " because the "
               << "feature map is too large.";
    UMA_HISTOGRAM_COUNTS("SBClientPhishing.TooManyFeatures", 1);
    return false;
  }
  features_[name] = 1.0;
  return true;
}

void FeatureMap::Clear() {
  features_.clear();
}

namespace features {
// URL host features
const char kUrlHostIsIpAddress[] = "UrlHostIsIpAddress";
const char kUrlTldToken[] = "UrlTld=";
const char kUrlDomainToken[] = "UrlDomain=";
const char kUrlOtherHostToken[] = "UrlOtherHostToken=";

// URL host aggregate features
const char kUrlNumOtherHostTokensGTOne[] = "UrlNumOtherHostTokens>1";
const char kUrlNumOtherHostTokensGTThree[] = "UrlNumOtherHostTokens>3";

// URL path features
const char kUrlPathToken[] = "UrlPathToken=";

}  // namespace features
}  // namespace safe_browsing
