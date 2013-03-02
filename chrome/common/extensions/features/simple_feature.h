// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_FEATURES_SIMPLE_FEATURE_H_
#define CHROME_COMMON_EXTENSIONS_FEATURES_SIMPLE_FEATURE_H_

#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/values.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/features/feature.h"
#include "chrome/common/extensions/manifest.h"

namespace extensions {

class ComplexFeature;

class SimpleFeature : public Feature {
 public:
  SimpleFeature();
  SimpleFeature(const SimpleFeature& other);
  virtual ~SimpleFeature();

  std::set<std::string>* whitelist() { return &whitelist_; }
  std::set<Manifest::Type>* extension_types() { return &extension_types_; }

  // Parses the JSON representation of a feature into the fields of this object.
  // Unspecified values in the JSON are not modified in the object. This allows
  // us to implement inheritance by parsing one value after another.
  void Parse(const DictionaryValue* value);

  // Returns true if the feature contains the same values as another.
  bool Equals(const SimpleFeature& other) const;

  Location location() const { return location_; }
  void set_location(Location location) { location_ = location; }

  Platform platform() const { return platform_; }
  void set_platform(Platform platform) { platform_ = platform; }

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
    return IsAvailableToContext(extension, context, GetCurrentPlatform());
  }

  // extension::Feature:
  virtual Availability IsAvailableToManifest(const std::string& extension_id,
                                             Manifest::Type type,
                                             Location location,
                                             int manifest_version,
                                             Platform platform) const OVERRIDE;

  virtual Availability IsAvailableToContext(const Extension* extension,
                                            Context context,
                                            Platform platform) const OVERRIDE;

  virtual std::string GetAvailabilityMessage(
      AvailabilityResult result, Manifest::Type type) const OVERRIDE;

  virtual std::set<Context>* GetContexts() OVERRIDE;

 protected:
  Availability CreateAvailability(AvailabilityResult result) const;
  Availability CreateAvailability(AvailabilityResult result,
                                  Manifest::Type type) const;
  bool IsIdInWhitelist(const std::string& extension_id) const;

 private:
  // For clarity and consistency, we handle the default value of each of these
  // members the same way: it matches everything. It is up to the higher level
  // code that reads Features out of static data to validate that data and set
  // sensible defaults.
  std::set<std::string> whitelist_;
  std::set<Manifest::Type> extension_types_;
  std::set<Context> contexts_;
  Location location_;  // we only care about component/not-component now
  Platform platform_;  // we only care about chromeos/not-chromeos now
  int min_manifest_version_;
  int max_manifest_version_;
  chrome::VersionInfo::Channel channel_;

  FRIEND_TEST_ALL_PREFIXES(ExtensionSimpleFeatureTest, Context);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_FEATURES_SIMPLE_FEATURE_H_
