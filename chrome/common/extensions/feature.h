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
#include "chrome/common/chrome_version_info.h"
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
    INVALID_MAX_MANIFEST_VERSION,
    NOT_PRESENT,
    UNSUPPORTED_CHANNEL,
  };

  Feature();
  Feature(const Feature& other);
  virtual ~Feature();

  // (Re)Sets whether checking against "channel" should be done. This must only
  // be called on the browser process, since the check involves accessing the
  // filesystem.
  // See http://crbug.com/126535.
  static void SetChannelCheckingEnabled(bool enabled);
  static void ResetChannelCheckingEnabled();

  // (Re)Sets the Channel to for all Features to compare against. This is
  // usually chrome::VersionInfo::GetChannel(), but for tests allow this to be
  // overridden.
  static void SetChannelForTesting(chrome::VersionInfo::Channel channel);
  static void ResetChannelForTesting();

  const std::string& name() const { return name_; }
  void set_name(const std::string& name) { name_ = name; }

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

  // Parses the JSON representation of a feature into the fields of this object.
  // Unspecified values in the JSON are not modified in the object. This allows
  // us to implement inheritance by parsing one value after another.
  void Parse(const DictionaryValue* value);

  // Returns true if the feature contains the same values as another.
  bool Equals(const Feature& other) const;

  // Returns true if the feature is available to be parsed into a new extension
  // manifest.
  Availability IsAvailableToManifest(const std::string& extension_id,
                                     Extension::Type type,
                                     Location location,
                                     int manifest_version) const {
    return IsAvailableToManifest(extension_id, type, location, manifest_version,
                                 GetCurrentPlatform());
  }
  Availability IsAvailableToManifest(const std::string& extension_id,
                                     Extension::Type type,
                                     Location location,
                                     int manifest_version,
                                     Platform platform) const;

  // Returns true if the feature is available to be used in the specified
  // extension and context.
  Availability IsAvailableToContext(const Extension* extension,
                                    Context context) const {
    return IsAvailableToContext(extension, context, GetCurrentPlatform());
  }
  virtual Availability IsAvailableToContext(const Extension* extension,
                                            Context context,
                                            Platform platform) const;

  // Returns an error message for an Availability code.
  std::string GetErrorMessage(Availability result);

 private:
  std::string name_;

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
  chrome::VersionInfo::Channel channel_;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_FEATURE_H_
