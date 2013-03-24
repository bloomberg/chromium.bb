// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_FEATURES_COMPLEX_FEATURE_H_
#define CHROME_COMMON_EXTENSIONS_FEATURES_COMPLEX_FEATURE_H_

#include <string>

#include "base/memory/scoped_vector.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/features/feature.h"
#include "chrome/common/extensions/manifest.h"

namespace extensions {

class SimpleFeature;

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
                                             Location location,
                                             int manifest_version,
                                             Platform platform) const OVERRIDE;

  virtual Availability IsAvailableToContext(const Extension* extension,
                                            Context context,
                                            const GURL& url,
                                            Platform platform) const OVERRIDE;

 protected:
  // extensions::Feature:
  virtual std::string GetAvailabilityMessage(
      AvailabilityResult result,
      Manifest::Type type,
      const GURL& url) const OVERRIDE;

  virtual std::set<Context>* GetContexts() OVERRIDE;

 private:
  FeatureList features_;

  DISALLOW_COPY_AND_ASSIGN(ComplexFeature);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_FEATURES_COMPLEX_FEATURE_H_
