// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_FEATURE_H_
#define EXTENSIONS_COMMON_FEATURES_FEATURE_H_

#include <set>
#include <string>

#include "base/values.h"
#include "extensions/common/manifest.h"

class GURL;

namespace extensions {

class Extension;

// Represents a single feature accessible to an extension developer, such as a
// top-level manifest key, a permission, or a programmatic API. A feature can
// express requirements for where it can be accessed, and supports testing
// support for those requirements. If platforms are not specified, then feature
// is available on all platforms.
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

    // A web page context which has been blessed by the user. Typically this
    // will be via the installation of a hosted app, so this may host an
    // extension. This is not affected by the URL matching pattern.
    BLESSED_WEB_PAGE_CONTEXT,
  };

  // The platforms the feature is supported in.
  enum Platform {
    UNSPECIFIED_PLATFORM,
    CHROMEOS_PLATFORM,
    LINUX_PLATFORM,
    MACOSX_PLATFORM,
    WIN_PLATFORM
  };

  // Whether a feature is available in a given situation or not, and if not,
  // why not.
  enum AvailabilityResult {
    IS_AVAILABLE,
    NOT_FOUND_IN_WHITELIST,
    INVALID_URL,
    INVALID_TYPE,
    INVALID_CONTEXT,
    INVALID_LOCATION,
    INVALID_PLATFORM,
    INVALID_MIN_MANIFEST_VERSION,
    INVALID_MAX_MANIFEST_VERSION,
    NOT_PRESENT,
    UNSUPPORTED_CHANNEL,
    FOUND_IN_BLACKLIST,
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
    friend class Feature;

    // Instances should be created via Feature::CreateAvailability.
    Availability(AvailabilityResult result, const std::string& message)
        : result_(result), message_(message) { }

    const AvailabilityResult result_;
    const std::string message_;
  };

  Feature();
  virtual ~Feature();

  // Used by ChromeV8Context until the feature system is fully functional.
  // TODO(kalman): This is no longer used by ChromeV8Context, so what is the
  // comment trying to say?
  static Availability CreateAvailability(AvailabilityResult result,
                                         const std::string& message);

  const std::string& name() const { return name_; }
  void set_name(const std::string& name) { name_ = name; }
  bool no_parent() const { return no_parent_; }

  // Gets the platform the code is currently running on.
  static Platform GetCurrentPlatform();

  // Tests whether this is an internal API or not.
  virtual bool IsInternal() const = 0;

  // Returns True for features excluded from service worker backed contexts.
  virtual bool IsBlockedInServiceWorker() const = 0;

  // Returns true if the feature is available to be parsed into a new extension
  // manifest.
  Availability IsAvailableToManifest(const std::string& extension_id,
                                     Manifest::Type type,
                                     Manifest::Location location,
                                     int manifest_version) const {
    return IsAvailableToManifest(extension_id, type, location, manifest_version,
                                 GetCurrentPlatform());
  }
  virtual Availability IsAvailableToManifest(const std::string& extension_id,
                                             Manifest::Type type,
                                             Manifest::Location location,
                                             int manifest_version,
                                             Platform platform) const = 0;

  // Returns true if the feature is available to |extension|.
  Availability IsAvailableToExtension(const Extension* extension);

  // Returns true if the feature is available to be used in the specified
  // extension and context.
  Availability IsAvailableToContext(const Extension* extension,
                                    Context context,
                                    const GURL& url) const {
    return IsAvailableToContext(extension, context, url, GetCurrentPlatform());
  }
  virtual Availability IsAvailableToContext(const Extension* extension,
                                            Context context,
                                            const GURL& url,
                                            Platform platform) const = 0;

  virtual std::string GetAvailabilityMessage(AvailabilityResult result,
                                             Manifest::Type type,
                                             const GURL& url,
                                             Context context) const = 0;

  virtual bool IsIdInBlacklist(const std::string& extension_id) const = 0;
  virtual bool IsIdInWhitelist(const std::string& extension_id) const = 0;

 protected:
  std::string name_;
  bool no_parent_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_FEATURE_H_
