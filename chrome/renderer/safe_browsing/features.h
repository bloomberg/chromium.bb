// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Common types and constants for extracting and evaluating features in the
// client-side phishing detection model.  A feature is simply a string and an
// associated floating-point value between 0 and 1.  The phishing
// classification model contains rules which give an appropriate weight to each
// feature or combination of features.  These values can then be summed to
// compute a final phishiness score.
//
// Some features are boolean features.  If these features are set, they always
// have a value of 0.0 or 1.0.  In practice, the features are only set if the
// value is true (1.0).
//
// We also use token features.  These features have a unique name that is
// constructed from the URL or page contents that we are classifying, for
// example, "UrlDomain=chromium".  These features are also always set to 1.0
// if they are present.
//
// The intermediate storage of the features for a URL is a FeatureMap, which is
// just a thin wrapper around a map of feature name to value.  The entire set
// of features for a URL is extracted before we do any scoring.

#ifndef CHROME_RENDERER_SAFE_BROWSING_FEATURES_H_
#define CHROME_RENDERER_SAFE_BROWSING_FEATURES_H_

#include <string>
#include "base/basictypes.h"
#include "base/hash_tables.h"

namespace safe_browsing {

// Container for a map of features to values, which enforces behavior
// such as a maximum number of features in the map.
class FeatureMap {
 public:
  FeatureMap();
  ~FeatureMap();

  // Adds a boolean feature to a FeatureMap with a value of 1.0.
  // Returns true on success, or false if the feature map exceeds
  // kMaxFeatureMapSize.
  bool AddBooleanFeature(const std::string& name);

  // Provides read-only access to the current set of features.
  const base::hash_map<std::string, double>& features() const {
    return features_;
  }

  // Clears the set of features in the map.
  void Clear();

  // This is an upper bound on the number of features that will be extracted.
  // We should never hit this cap; it is intended as a sanity check to prevent
  // the FeatureMap from growing too large.
  static const size_t kMaxFeatureMapSize;

 private:
  base::hash_map<std::string, double> features_;

  DISALLOW_COPY_AND_ASSIGN(FeatureMap);
};

namespace features {
// Constants for the various feature names that we use.

////////////////////////////////////////////////////
// URL host features
////////////////////////////////////////////////////

// Set if the URL's hostname is an IP address.
extern const char kUrlHostIsIpAddress[];
// Token feature containing the portion of the hostname controlled by a
// registrar, for example "com" or "co.uk".
extern const char kUrlTldToken[];
// Token feature containing the first host component below the registrar.
// For example, in "www.google.com", the domain would be "google".
extern const char kUrlDomainToken[];
// Token feature containing each host component below the domain.
// For example, in "www.host.example.com", both "www" and "host" would be
// "other host tokens".
extern const char kUrlOtherHostToken[];

////////////////////////////////////////////////////
// Aggregate features for URL host tokens
////////////////////////////////////////////////////

// Set if the number of "other" host tokens for a URL is greater than one.
// Longer hostnames, regardless of the specific tokens, can be a signal that
// the URL is phishy.
extern const char kUrlNumOtherHostTokensGTOne[];
// Set if the number of "other" host tokens for a URL is greater than three.
extern const char kUrlNumOtherHostTokensGTThree[];

////////////////////////////////////////////////////
// URL path token features
////////////////////////////////////////////////////

// Token feature containing each alphanumeric string in the path that is at
// least 3 characters long.  For example, "/abc/d/efg" would have 2 path
// token features, "abc" and "efg".  Query parameters are not included.
extern const char kUrlPathToken[];

}  // namespace features
}  // namepsace safe_browsing

#endif  // CHROME_RENDERER_SAFE_BROWSING_FEATURES_H_
