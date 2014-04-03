// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_SIMPLE_FEATURE_H_
#define EXTENSIONS_COMMON_FEATURES_SIMPLE_FEATURE_H_

#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/simple_feature_filter.h"
#include "extensions/common/manifest.h"

namespace extensions {

class ComplexFeature;

class SimpleFeature : public Feature {
 public:
  SimpleFeature();
  virtual ~SimpleFeature();

  std::set<std::string>* whitelist() { return &whitelist_; }
  std::set<Manifest::Type>* extension_types() { return &extension_types_; }

  // Adds a filter to this feature. The feature takes ownership of the filter.
  void AddFilter(scoped_ptr<SimpleFeatureFilter> filter);

  // Parses the JSON representation of a feature into the fields of this object.
  // Unspecified values in the JSON are not modified in the object. This allows
  // us to implement inheritance by parsing one value after another. Returns
  // the error found, or an empty string on success.
  virtual std::string Parse(const base::DictionaryValue* value);

  Location location() const { return location_; }
  void set_location(Location location) { location_ = location; }

  std::set<Platform>* platforms() { return &platforms_; }

  int min_manifest_version() const { return min_manifest_version_; }
  void set_min_manifest_version(int min_manifest_version) {
    min_manifest_version_ = min_manifest_version;
  }

  int max_manifest_version() const { return max_manifest_version_; }
  void set_max_manifest_version(int max_manifest_version) {
    max_manifest_version_ = max_manifest_version;
  }

  Availability IsAvailableToContext(const Extension* extension,
                                    Context context) const {
    return IsAvailableToContext(extension, context, GURL());
  }
  Availability IsAvailableToContext(const Extension* extension,
                                    Context context,
                                    Platform platform) const {
    return IsAvailableToContext(extension, context, GURL(), platform);
  }
  Availability IsAvailableToContext(const Extension* extension,
                                    Context context,
                                    const GURL& url) const {
    return IsAvailableToContext(extension, context, url, GetCurrentPlatform());
  }

  // extension::Feature:
  virtual Availability IsAvailableToManifest(const std::string& extension_id,
                                             Manifest::Type type,
                                             Location location,
                                             int manifest_version,
                                             Platform platform) const OVERRIDE;

  virtual Availability IsAvailableToContext(const Extension* extension,
                                            Context context,
                                            const GURL& url,
                                            Platform platform) const OVERRIDE;

  virtual std::string GetAvailabilityMessage(AvailabilityResult result,
                                             Manifest::Type type,
                                             const GURL& url,
                                             Context context) const OVERRIDE;

  virtual std::set<Context>* GetContexts() OVERRIDE;

  virtual bool IsInternal() const OVERRIDE;
  virtual bool IsBlockedInServiceWorker() const OVERRIDE;

  virtual bool IsIdInWhitelist(const std::string& extension_id) const OVERRIDE;
  static bool IsIdInWhitelist(const std::string& extension_id,
                              const std::set<std::string>& whitelist);

 protected:
  Availability CreateAvailability(AvailabilityResult result) const;
  Availability CreateAvailability(AvailabilityResult result,
                                  Manifest::Type type) const;
  Availability CreateAvailability(AvailabilityResult result,
                                  const GURL& url) const;
  Availability CreateAvailability(AvailabilityResult result,
                                  Context context) const;

 private:
  // For clarity and consistency, we handle the default value of each of these
  // members the same way: it matches everything. It is up to the higher level
  // code that reads Features out of static data to validate that data and set
  // sensible defaults.
  std::set<std::string> whitelist_;
  std::set<Manifest::Type> extension_types_;
  std::set<Context> contexts_;
  URLPatternSet matches_;
  Location location_;  // we only care about component/not-component now
  std::set<Platform> platforms_;
  int min_manifest_version_;
  int max_manifest_version_;
  bool has_parent_;

  typedef std::vector<linked_ptr<SimpleFeatureFilter> > FilterList;
  FilterList filters_;

  FRIEND_TEST_ALL_PREFIXES(ExtensionSimpleFeatureTest, Context);
  DISALLOW_COPY_AND_ASSIGN(SimpleFeature);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_SIMPLE_FEATURE_H_
