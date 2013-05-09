// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/permissions_data.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/features/base_feature_provider.h"
#include "chrome/common/extensions/features/feature.h"
#include "chrome/common/extensions/manifest_handlers/content_scripts_handler.h"
#include "chrome/common/extensions/permissions/api_permission_set.h"
#include "chrome/common/extensions/permissions/permission_set.h"
#include "chrome/common/extensions/permissions/permissions_info.h"
#include "chrome/common/url_constants.h"
#include "extensions/common/error_utils.h"

namespace keys = extension_manifest_keys;
namespace errors = extension_manifest_errors;

namespace extensions {

namespace {

const char kThumbsWhiteListedExtension[] = "khopmbdjffemhegeeobelklnbglcdgfh";

bool ContainsManifestForbiddenPermission(const APIPermissionSet& apis,
                                         string16* error) {
  CHECK(error);
  for (APIPermissionSet::const_iterator iter = apis.begin();
       iter != apis.end(); ++iter) {
    if ((*iter)->ManifestEntryForbidden()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kPermissionNotAllowedInManifest,
          (*iter)->info()->name());
      return true;
    }
  }
  return false;
}

// Custom checks for the experimental permission that can't be expressed in
// _permission_features.json.
bool CanSpecifyExperimentalPermission(const Extension* extension) {
  if (extension->location() == Manifest::COMPONENT)
    return true;

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExperimentalExtensionApis)) {
    return true;
  }

  // We rely on the webstore to check access to experimental. This way we can
  // whitelist extensions to have access to experimental in just the store, and
  // not have to push a new version of the client.
  if (extension->from_webstore())
    return true;

  return false;
}

// Checks whether the host |pattern| is allowed for the given |extension|,
// given API permissions |permissions|.
bool CanSpecifyHostPermission(const Extension* extension,
                              const URLPattern& pattern,
                              const APIPermissionSet& permissions) {
  if (!pattern.match_all_urls() &&
      pattern.MatchesScheme(chrome::kChromeUIScheme)) {
    // Regular extensions are only allowed access to chrome://favicon.
    if (pattern.host() == chrome::kChromeUIFaviconHost)
      return true;

    // Experimental extensions are also allowed chrome://thumb.
    //
    // TODO: A public API should be created for retrieving thumbnails.
    // See http://crbug.com/222856. A temporary hack is implemented here to
    // make chrome://thumbs available to NTP Russia extension as
    // non-experimental.
    if (pattern.host() == chrome::kChromeUIThumbnailHost) {
      return
          permissions.find(APIPermission::kExperimental) != permissions.end() ||
          (extension->id() == kThumbsWhiteListedExtension &&
              extension->from_webstore());
    }

    // Component extensions can have access to all of chrome://*.
    if (extension->CanExecuteScriptEverywhere())
      return true;

    if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kExtensionsOnChromeURLs)) {
      return true;
    }

    // TODO(aboxhall): return from_webstore() when webstore handles blocking
    // extensions which request chrome:// urls
    return false;
  }

  // Otherwise, the valid schemes were handled by URLPattern.
  return true;
}

// Parses the host and api permissions from the specified permission |key|
// from |extension|'s manifest.
bool ParseHelper(Extension* extension,
                 const char* key,
                 APIPermissionSet* api_permissions,
                 URLPatternSet* host_permissions,
                 string16* error) {
  if (!extension->manifest()->HasKey(key))
    return true;

  const ListValue* permissions = NULL;
  if (!extension->manifest()->GetList(key, &permissions)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(errors::kInvalidPermissions,
                                                 std::string());
    return false;
  }

  // NOTE: We need to get the APIPermission before we check if features
  // associated with them are available because the feature system does not
  // know about aliases.

  std::vector<std::string> host_data;
  if (!APIPermissionSet::ParseFromJSON(
          permissions, api_permissions, error, &host_data)) {
    return false;
  }

  // Verify feature availability of permissions.
  std::vector<APIPermission::ID> to_remove;
  FeatureProvider* permission_features =
      BaseFeatureProvider::GetByName("permission");
  for (APIPermissionSet::const_iterator iter = api_permissions->begin();
       iter != api_permissions->end(); ++iter) {
    Feature* feature = permission_features->GetFeature(iter->name());

    // The feature should exist since we just got an APIPermission for it. The
    // two systems should be updated together whenever a permission is added.
    DCHECK(feature);
    // http://crbug.com/176381
    if (!feature) {
      to_remove.push_back(iter->id());
      continue;
    }

    Feature::Availability availability = feature->IsAvailableToManifest(
        extension->id(),
        extension->GetType(),
        Feature::ConvertLocation(extension->location()),
        extension->manifest_version());

    if (!availability.is_available()) {
      // Don't fail, but warn the developer that the manifest contains
      // unrecognized permissions. This may happen legitimately if the
      // extensions requests platform- or channel-specific permissions.
      extension->AddInstallWarning(InstallWarning(InstallWarning::FORMAT_TEXT,
                                                  availability.message()));
      to_remove.push_back(iter->id());
      continue;
    }

    if (iter->id() == APIPermission::kExperimental) {
      if (!CanSpecifyExperimentalPermission(extension)) {
        *error = ASCIIToUTF16(errors::kExperimentalFlagRequired);
        return false;
      }
    }
  }

  // Remove permissions that are not available to this extension.
  for (std::vector<APIPermission::ID>::const_iterator iter = to_remove.begin();
       iter != to_remove.end(); ++iter) {
      api_permissions->erase(*iter);
  }

  // Parse host pattern permissions.
  const int kAllowedSchemes = extension->CanExecuteScriptEverywhere() ?
      URLPattern::SCHEME_ALL : Extension::kValidHostPermissionSchemes;

  for (std::vector<std::string>::const_iterator iter = host_data.begin();
       iter != host_data.end(); ++iter) {
    const std::string& permission_str = *iter;

    // Check if it's a host pattern permission.
    URLPattern pattern = URLPattern(kAllowedSchemes);
    URLPattern::ParseResult parse_result = pattern.Parse(permission_str);
    if (parse_result == URLPattern::PARSE_SUCCESS) {
      // The path component is not used for host permissions, so we force it
      // to match all paths.
      pattern.SetPath("/*");
      int valid_schemes = pattern.valid_schemes();
      if (pattern.MatchesScheme(chrome::kFileScheme) &&
          !extension->CanExecuteScriptEverywhere()) {
        extension->set_wants_file_access(true);
        if (!(extension->creation_flags() & Extension::ALLOW_FILE_ACCESS))
          valid_schemes &= ~URLPattern::SCHEME_FILE;
      }

      if (pattern.scheme() != chrome::kChromeUIScheme &&
          !extension->CanExecuteScriptEverywhere()) {
        // Keep chrome:// in allowed schemes only if it's explicitly requested
        // or CanExecuteScriptEverywhere is true. If the
        // extensions_on_chrome_urls flag is not set, CanSpecifyHostPermission
        // will fail, so don't check the flag here.
        valid_schemes &= ~URLPattern::SCHEME_CHROMEUI;
      }
      pattern.SetValidSchemes(valid_schemes);

      if (!CanSpecifyHostPermission(extension, pattern, *api_permissions)) {
        // TODO(aboxhall): make a warning (see pattern.match_all_urls() block
        // below).
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidPermissionScheme, permission_str);
        return false;
      }

      host_permissions->AddPattern(pattern);

      // We need to make sure all_urls matches chrome://favicon and (maybe)
      // chrome://thumbnail, so add them back in to host_permissions separately.
      if (pattern.match_all_urls()) {
        host_permissions->AddPattern(
            URLPattern(URLPattern::SCHEME_CHROMEUI,
                       chrome::kChromeUIFaviconURL));
        if (api_permissions->find(APIPermission::kExperimental) !=
                api_permissions->end()) {
          host_permissions->AddPattern(
              URLPattern(URLPattern::SCHEME_CHROMEUI,
                         chrome::kChromeUIThumbnailURL));
        }
      }
      continue;
    }

    // It's probably an unknown API permission. Do not throw an error so
    // extensions can retain backwards compatability (http://crbug.com/42742).
    extension->AddInstallWarning(InstallWarning(
        InstallWarning::FORMAT_TEXT,
        base::StringPrintf(
            "Permission '%s' is unknown or URL pattern is malformed.",
            permission_str.c_str())));
  }

  return true;
}

}  // namespace

struct PermissionsData::InitialPermissions {
  APIPermissionSet api_permissions;
  URLPatternSet host_permissions;
};

PermissionsData::PermissionsData() {
}

PermissionsData::~PermissionsData() {
}

// static
const PermissionSet* PermissionsData::GetOptionalPermissions(
    const Extension* extension) {
  return extension->permissions_data()->optional_permission_set_.get();
}

// static
const PermissionSet* PermissionsData::GetRequiredPermissions(
    const Extension* extension) {
  return extension->permissions_data()->required_permission_set_.get();
}

// static
const APIPermissionSet* PermissionsData::GetInitialAPIPermissions(
    const Extension* extension) {
  return &extension->permissions_data()->
      initial_required_permissions_->api_permissions;
}

// static
APIPermissionSet* PermissionsData::GetInitialAPIPermissions(
    Extension* extension) {
  return &extension->permissions_data()->
      initial_required_permissions_->api_permissions;
}

bool PermissionsData::ParsePermissions(Extension* extension, string16* error) {
  initial_required_permissions_.reset(new InitialPermissions);
  if (!ParseHelper(extension,
                   keys::kPermissions,
                   &initial_required_permissions_->api_permissions,
                   &initial_required_permissions_->host_permissions,
                   error)) {
    return false;
  }

  // Check for any permissions that are optional only.
  for (APIPermissionSet::const_iterator iter =
           initial_required_permissions_->api_permissions.begin();
       iter != initial_required_permissions_->api_permissions.end(); ++iter) {
    if ((*iter)->info()->must_be_optional()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kPermissionMustBeOptional, (*iter)->info()->name());
      return false;
    }
  }

  // TODO(jeremya/kalman) do this via the features system by exposing the
  // app.window API to platform apps, with no dependency on any permissions.
  // See http://crbug.com/120069.
  if (extension->is_platform_app()) {
    initial_required_permissions_->api_permissions.insert(
        APIPermission::kAppCurrentWindowInternal);
    initial_required_permissions_->api_permissions.insert(
        APIPermission::kAppRuntime);
    initial_required_permissions_->api_permissions.insert(
        APIPermission::kAppWindow);
  }

  initial_optional_permissions_.reset(new InitialPermissions);
  if (!ParseHelper(extension,
                   keys::kOptionalPermissions,
                   &initial_optional_permissions_->api_permissions,
                   &initial_optional_permissions_->host_permissions,
                   error)) {
    return false;
  }

  if (ContainsManifestForbiddenPermission(
          initial_required_permissions_->api_permissions, error) ||
      ContainsManifestForbiddenPermission(
          initial_optional_permissions_->api_permissions, error)) {
    return false;
  }

  return true;
}

void PermissionsData::FinalizePermissions(Extension* extension) {
  URLPatternSet scriptable_hosts =
      ContentScriptsInfo::GetScriptableHosts(extension);

  extension->SetActivePermissions(new PermissionSet(
      initial_required_permissions_->api_permissions,
      initial_required_permissions_->host_permissions,
      scriptable_hosts));

  required_permission_set_ = new PermissionSet(
      initial_required_permissions_->api_permissions,
      initial_required_permissions_->host_permissions,
      scriptable_hosts);

  optional_permission_set_ = new PermissionSet(
      initial_optional_permissions_->api_permissions,
      initial_optional_permissions_->host_permissions,
      URLPatternSet());

  initial_required_permissions_.reset();
  initial_optional_permissions_.reset();
}

}  // namespace extensions
