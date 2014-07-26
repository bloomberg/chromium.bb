// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_COMPLEX_FEATURE_H_
#define EXTENSIONS_COMMON_FEATURES_COMPLEX_FEATURE_H_

#include <set>
#include <string>

#include "base/memory/scoped_vector.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/manifest.h"

namespace extensions {

// A ComplexFeature is composed of one or many Features. A ComplexFeature
// is available if any Feature (i.e. permission rule) that composes it is
// available, but not if only some combination of Features is available.
class ComplexFeature : public Feature {
 public:
  typedef ScopedVector<Feature> FeatureList;

  explicit ComplexFeature(scoped_ptr<FeatureList> features);
  virtual ~ComplexFeature();

  // extensions::Feature:
  virtual Availability IsAvailableToManifest(const std::string& extension_id,
                                             Manifest::Type type,
                                             Manifest::Location location,
                                             int manifest_version,
                                             Platform platform) const OVERRIDE;

  virtual Availability IsAvailableToContext(const Extension* extension,
                                            Context context,
                                            const GURL& url,
                                            Platform platform) const OVERRIDE;

  virtual bool IsIdInBlacklist(const std::string& extension_id) const OVERRIDE;
  virtual bool IsIdInWhitelist(const std::string& extension_id) const OVERRIDE;

 protected:
  // extensions::Feature:
  virtual std::string GetAvailabilityMessage(
      AvailabilityResult result,
      Manifest::Type type,
      const GURL& url,
      Context context) const OVERRIDE;

  virtual bool IsInternal() const OVERRIDE;

 private:
  FeatureList features_;

  DISALLOW_COPY_AND_ASSIGN(ComplexFeature);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_COMPLEX_FEATURE_H_
