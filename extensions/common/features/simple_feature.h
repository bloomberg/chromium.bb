// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_SIMPLE_FEATURE_H_
#define EXTENSIONS_COMMON_FEATURES_SIMPLE_FEATURE_H_

#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/values.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/simple_feature_filter.h"
#include "extensions/common/manifest.h"

namespace extensions {

class SimpleFeature : public Feature {
 public:
  SimpleFeature();
  ~SimpleFeature() override;

  // Similar to Manifest::Location, these are the classes of locations
  // supported in feature files.
  //
  // This is only public for testing. Production code should never access it,
  // nor should it really have any reason to access the SimpleFeature class
  // directly, it should be dealing with the Feature interface.
  enum Location {
    UNSPECIFIED_LOCATION,
    COMPONENT_LOCATION,
    EXTERNAL_COMPONENT_LOCATION,
    POLICY_LOCATION,
  };

  // Accessors defined for testing. See comment above about not directly using
  // SimpleFeature in production code.
  std::set<std::string>* blacklist() { return &blacklist_; }
  const std::set<std::string>* blacklist() const { return &blacklist_; }
  std::set<std::string>* whitelist() { return &whitelist_; }
  const std::set<std::string>* whitelist() const { return &whitelist_; }
  std::set<Manifest::Type>* extension_types() { return &extension_types_; }
  const std::set<Manifest::Type>* extension_types() const {
    return &extension_types_;
  }
  std::set<Context>* contexts() { return &contexts_; }
  const std::set<Context>* contexts() const { return &contexts_; }
  Location location() const { return location_; }
  void set_location(Location location) { location_ = location; }
  int min_manifest_version() const { return min_manifest_version_; }
  void set_min_manifest_version(int min_manifest_version) {
    min_manifest_version_ = min_manifest_version;
  }
  int max_manifest_version() const { return max_manifest_version_; }
  void set_max_manifest_version(int max_manifest_version) {
    max_manifest_version_ = max_manifest_version;
  }
  const std::string& command_line_switch() const {
    return command_line_switch_;
  }
  void set_command_line_switch(const std::string& command_line_switch) {
    command_line_switch_ = command_line_switch;
  }

  // Dependency resolution is a property of Features that is preferrably
  // handled internally to avoid temptation, but FeatureFilters may need
  // to know if there are any at all.
  bool HasDependencies() const;

  // Adds a filter to this feature. The feature takes ownership of the filter.
  void AddFilter(scoped_ptr<SimpleFeatureFilter> filter);

  // Parses the JSON representation of a feature into the fields of this object.
  // Unspecified values in the JSON are not modified in the object. This allows
  // us to implement inheritance by parsing one value after another. Returns
  // the error found, or an empty string on success.
  virtual std::string Parse(const base::DictionaryValue* value);

  std::set<Platform>* platforms() { return &platforms_; }

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
  Availability IsAvailableToManifest(const std::string& extension_id,
                                     Manifest::Type type,
                                     Manifest::Location location,
                                     int manifest_version,
                                     Platform platform) const override;

  Availability IsAvailableToContext(const Extension* extension,
                                    Context context,
                                    const GURL& url,
                                    Platform platform) const override;

  std::string GetAvailabilityMessage(AvailabilityResult result,
                                     Manifest::Type type,
                                     const GURL& url,
                                     Context context) const override;

  bool IsInternal() const override;

  bool IsIdInBlacklist(const std::string& extension_id) const override;
  bool IsIdInWhitelist(const std::string& extension_id) const override;
  static bool IsIdInList(const std::string& extension_id,
                         const std::set<std::string>& list);

 protected:
  // Handy utilities which construct the correct availability message.
  Availability CreateAvailability(AvailabilityResult result) const;
  Availability CreateAvailability(AvailabilityResult result,
                                  Manifest::Type type) const;
  Availability CreateAvailability(AvailabilityResult result,
                                  const GURL& url) const;
  Availability CreateAvailability(AvailabilityResult result,
                                  Context context) const;

 private:
  bool MatchesManifestLocation(Manifest::Location manifest_location) const;

  Availability CheckDependencies(
      const base::Callback<Availability(const Feature*)>& checker) const;

  // For clarity and consistency, we handle the default value of each of these
  // members the same way: it matches everything. It is up to the higher level
  // code that reads Features out of static data to validate that data and set
  // sensible defaults.
  std::set<std::string> blacklist_;
  std::set<std::string> whitelist_;
  std::set<std::string> dependencies_;
  std::set<Manifest::Type> extension_types_;
  std::set<Context> contexts_;
  URLPatternSet matches_;
  Location location_;
  std::set<Platform> platforms_;
  int min_manifest_version_;
  int max_manifest_version_;
  bool component_extensions_auto_granted_;
  std::string command_line_switch_;

  ScopedVector<SimpleFeatureFilter> filters_;;

  DISALLOW_COPY_AND_ASSIGN(SimpleFeature);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_SIMPLE_FEATURE_H_
