// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_FEATURES_CHROME_CHANNEL_FEATURE_FILTER_H_
#define CHROME_COMMON_EXTENSIONS_FEATURES_CHROME_CHANNEL_FEATURE_FILTER_H_

#include "chrome/common/chrome_version_info.h"
#include "extensions/common/features/simple_feature_filter.h"

namespace extensions {

// This filter parses a "channel" key from feature value data and makes features
// unavailable if they aren't stable enough for the current channel.
class ChromeChannelFeatureFilter : public SimpleFeatureFilter {
 public:
  explicit ChromeChannelFeatureFilter(SimpleFeature* feature);
  virtual ~ChromeChannelFeatureFilter();

  // SimpleFeatureFilter implementation.
  virtual std::string Parse(const base::DictionaryValue* value) OVERRIDE;
  virtual Feature::Availability IsAvailableToManifest(
      const std::string& extension_id,
      Manifest::Type type,
      Manifest::Location location,
      int manifest_version,
      Feature::Platform platform) const OVERRIDE;

 private:
  bool channel_has_been_set_;
  chrome::VersionInfo::Channel channel_;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_FEATURES_CHROME_CHANNEL_FEATURE_FILTER_H_
