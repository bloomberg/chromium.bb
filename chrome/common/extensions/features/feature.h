// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_FEATURES_FEATURE_H_
#define CHROME_COMMON_EXTENSIONS_FEATURES_FEATURE_H_

#include <set>
#include <string>

#include "base/values.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest.h"

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
  enum AvailabilityResult {
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

  // Container for AvailabiltyResult that also exposes a user-visible error
  // message in cases where the feature is not available.
  class Availability {
   public:
    AvailabilityResult result() const { return result_; }
    bool is_available() const { return result_ == IS_AVAILABLE; }
    const std::string& message() const { return message_; }

   private:
    friend class SimpleFeature;

    // Instances should be created via Feature::CreateAvailability.
    Availability(AvailabilityResult result, const std::string& message)
        : result_(result), message_(message) { }

    const AvailabilityResult result_;
    const std::string message_;
  };

  virtual ~Feature();

  // Gets the current channel as seen by the Feature system.
  static chrome::VersionInfo::Channel GetCurrentChannel();

  // Sets the current channel as seen by the Feature system. In the browser
  // process this should be chrome::VersionInfo::GetChannel(), and in the
  // renderer this will need to come from an IPC.
  static void SetCurrentChannel(chrome::VersionInfo::Channel channel);

  // Gets the default channel as seen by the Feature system.
  static chrome::VersionInfo::Channel GetDefaultChannel();

  // Scoped channel setter. Use for tests.
  class ScopedCurrentChannel {
   public:
    explicit ScopedCurrentChannel(chrome::VersionInfo::Channel channel)
        : original_channel_(chrome::VersionInfo::CHANNEL_UNKNOWN) {
      original_channel_ = GetCurrentChannel();
      SetCurrentChannel(channel);
    }

    ~ScopedCurrentChannel() {
      SetCurrentChannel(original_channel_);
    }

   private:
    chrome::VersionInfo::Channel original_channel_;
  };

  const std::string& name() const { return name_; }
  void set_name(const std::string& name) { name_ = name; }

  // Gets the platform the code is currently running on.
  static Platform GetCurrentPlatform();

  // Gets the Feature::Location value for the specified Manifest::Location.
  static Location ConvertLocation(Manifest::Location extension_location);

  // TODO(justinlin): Remove and move to APIFeature when it exists.
  virtual std::set<Context>* GetContexts() = 0;

  // Returns true if the feature is available to be parsed into a new extension
  // manifest.
  Availability IsAvailableToManifest(const std::string& extension_id,
                                     Manifest::Type type,
                                     Location location,
                                     int manifest_version) const {
    return IsAvailableToManifest(extension_id, type, location, manifest_version,
                                 GetCurrentPlatform());
  }
  virtual Availability IsAvailableToManifest(const std::string& extension_id,
                                             Manifest::Type type,
                                             Location location,
                                             int manifest_version,
                                             Platform platform) const = 0;

  // Returns true if the feature is available to be used in the specified
  // extension and context.
  Availability IsAvailableToContext(const Extension* extension,
                                    Context context) const {
    return IsAvailableToContext(extension, context, GetCurrentPlatform());
  }
  virtual Availability IsAvailableToContext(const Extension* extension,
                                            Context context,
                                            Platform platform) const = 0;

  virtual std::string GetAvailabilityMessage(
      AvailabilityResult result, Manifest::Type type) const = 0;

 protected:
  std::string name_;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_FEATURES_FEATURE_H_
