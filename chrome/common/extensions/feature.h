// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_FEATURE_H_
#define CHROME_COMMON_EXTENSIONS_FEATURE_H_
#pragma once

#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/extensions/extension.h"

namespace extensions {

// Represents a single feature accessible to an extension developer, such as a
// top-level manifest key, a permission, or a programmatic API. A feature can
// express requirements for where it can be accessed, and supports testing
// support for those requirements.
class Feature {
 public:
  // The JavaScript contexts the feature is supported in.
  enum Context {
    UNSPECIFIED_CONTEXT,

    // A context in a privileged extension process.
    BLESSED_EXTENSION_CONTEXT,

    // A context in an unprivileged extension process.
    UNBLESSED_EXTENSION_CONTEXT,

    // A context from a content script.
    CONTENT_SCRIPT_CONTEXT,

    // A normal web page. This should have an associated URL matching pattern.
    WEB_PAGE_CONTEXT,
  };

  // The location required of extensions the feature is supported in.
  enum Location {
    UNSPECIFIED_LOCATION,
    COMPONENT_LOCATION
  };

  // The platforms the feature is supported in.
  enum Platform {
    UNSPECIFIED_PLATFORM,
    CHROMEOS_PLATFORM
  };

  // Whether a feature is available in a given situation or not, and if not,
  // why not.
  enum Availability {
    IS_AVAILABLE,
    NOT_FOUND_IN_WHITELIST,
    INVALID_TYPE,
    INVALID_CONTEXT,
    INVALID_LOCATION,
    INVALID_PLATFORM,
    INVALID_MIN_MANIFEST_VERSION,
    INVALID_MAX_MANIFEST_VERSION
  };

  Feature();
  ~Feature();

  // Parses a feature from its JSON representation.
  static scoped_ptr<Feature> Parse(const DictionaryValue* value);

  // Gets the platform the code is currently running on.
  static Platform GetCurrentPlatform();

  // Gets the Feature::Location value for the specified Extension::Location.
  static Location ConvertLocation(Extension::Location extension_location);

  std::set<std::string>* whitelist() { return &whitelist_; }
  std::set<Extension::Type>* extension_types() { return &extension_types_; }
  std::set<Context>* contexts() { return &contexts_; }

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

  // Returns true if the feature is available to the specified extension. Use
  // this overload for features that are not associated with a specific context.
  Availability IsAvailable(const Extension* extension) {
    return IsAvailable(extension, UNSPECIFIED_CONTEXT);
  }

  // Returns true if the feature is available to the specified extension, in the
  // specified context type.
  Availability IsAvailable(const Extension* extension, Context context) {
    return IsAvailable(extension->id(), extension->GetType(),
                       ConvertLocation(extension->location()), context,
                       GetCurrentPlatform(),
                       extension->manifest_version());
  }

  // Returns true if the feature is available to extensions with the specified
  // properties. Use this overload for features that are not associated with a
  // specific context, and when a full Extension object is not available.
  Availability IsAvailable(const std::string& extension_id,
                           Extension::Type type, Location location,
                           int manifest_version) {
    return IsAvailable(extension_id, type, location, UNSPECIFIED_CONTEXT,
                       GetCurrentPlatform(), manifest_version);
  }

  // Returns true if the feature is available to extensions with the specified
  // properties, in the specified context type, and on the specified platform.
  // This overload is mainly used for testing.
  Availability IsAvailable(const std::string& extension_id,
                           Extension::Type type, Location location,
                           Context context, Platform platform,
                           int manifest_version);

  // Returns an error message for an Availability code.
  std::string GetErrorMessage(Availability result);

 private:
  // For clarify and consistency, we handle the default value of each of these
  // members the same way: it matches everything. It is up to the higher level
  // code that reads Features out of static data to validate that data and set
  // sensible defaults.
  std::set<std::string> whitelist_;
  std::set<Extension::Type> extension_types_;
  std::set<Context> contexts_;
  Location location_;  // we only care about component/not-component now
  Platform platform_;  // we only care about chromeos/not-chromeos now
  int min_manifest_version_;
  int max_manifest_version_;

  DISALLOW_COPY_AND_ASSIGN(Feature);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_FEATURE_H_
