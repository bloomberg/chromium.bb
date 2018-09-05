// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/permissions_parser.h"

#include <utility>

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "extensions/common/url_pattern_set.h"
#include "url/url_constants.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

namespace {

struct ManifestPermissions : public Extension::ManifestData {
  ManifestPermissions(std::unique_ptr<const PermissionSet> permissions);
  ~ManifestPermissions() override;

  std::unique_ptr<const PermissionSet> permissions;
};

ManifestPermissions::ManifestPermissions(
    std::unique_ptr<const PermissionSet> permissions)
    : permissions(std::move(permissions)) {}

ManifestPermissions::~ManifestPermissions() {
}

// Checks whether the host |pattern| is allowed for the given |extension|,
// given API permissions |permissions|.
bool CanSpecifyHostPermission(const Extension* extension,
                              const URLPattern& pattern,
                              const APIPermissionSet& permissions) {
  if (!pattern.match_all_urls() &&
      pattern.MatchesScheme(content::kChromeUIScheme)) {
    URLPatternSet chrome_scheme_hosts =
        ExtensionsClient::Get()->GetPermittedChromeSchemeHosts(extension,
                                                               permissions);
    if (chrome_scheme_hosts.ContainsPattern(pattern))
      return true;

    // Component extensions can have access to all of chrome://*.
    if (PermissionsData::CanExecuteScriptEverywhere(extension->id(),
                                                    extension->location())) {
      return true;
    }

    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
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
                 base::string16* error) {
  if (!extension->manifest()->HasKey(key))
    return true;

  const base::ListValue* permissions = NULL;
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
          permissions,
          APIPermissionSet::kDisallowInternalPermissions,
          api_permissions,
          error,
          &host_data)) {
    return false;
  }

  // Verify feature availability of permissions.
  std::vector<APIPermission::ID> to_remove;
  const FeatureProvider* permission_features =
      FeatureProvider::GetPermissionFeatures();
  for (APIPermissionSet::const_iterator iter = api_permissions->begin();
       iter != api_permissions->end();
       ++iter) {
    const Feature* feature = permission_features->GetFeature(iter->name());

    // The feature should exist since we just got an APIPermission for it. The
    // two systems should be updated together whenever a permission is added.
    DCHECK(feature) << "Could not find feature for " << iter->name();
    // http://crbug.com/176381
    if (!feature) {
      to_remove.push_back(iter->id());
      continue;
    }

    // Sneaky check for "experimental", which we always allow for extensions
    // installed from the Webstore. This way we can whitelist extensions to
    // have access to experimental in just the store, and not have to push a
    // new version of the client. Otherwise, experimental goes through the
    // usual features check.
    if (iter->id() == APIPermission::kExperimental &&
        extension->from_webstore()) {
      continue;
    }

    Feature::Availability availability =
        feature->IsAvailableToExtension(extension);
    if (!availability.is_available()) {
      // Don't fail, but warn the developer that the manifest contains
      // unrecognized permissions. This may happen legitimately if the
      // extensions requests platform- or channel-specific permissions.
      extension->AddInstallWarning(
          InstallWarning(availability.message(), feature->name()));
      to_remove.push_back(iter->id());
      continue;
    }
  }

  // Remove permissions that are not available to this extension.
  for (std::vector<APIPermission::ID>::const_iterator iter = to_remove.begin();
       iter != to_remove.end();
       ++iter) {
    api_permissions->erase(*iter);
  }

  bool can_execute_script_everywhere =
      PermissionsData::CanExecuteScriptEverywhere(extension->id(),
                                                  extension->location());

  // Users should be able to enable file access for extensions with activeTab.
  if (!can_execute_script_everywhere &&
      base::ContainsKey(*api_permissions, APIPermission::kActiveTab)) {
    extension->set_wants_file_access(true);
  }

  // Parse host pattern permissions.
  const int kAllowedSchemes = can_execute_script_everywhere
                                  ? URLPattern::SCHEME_ALL
                                  : Extension::kValidHostPermissionSchemes;

  const bool all_urls_includes_chrome_urls =
      PermissionsData::AllUrlsIncludesChromeUrls(extension->id());

  for (std::vector<std::string>::const_iterator iter = host_data.begin();
       iter != host_data.end();
       ++iter) {
    const std::string& permission_str = *iter;

    // Check if it's a host pattern permission.
    URLPattern pattern = URLPattern(kAllowedSchemes);
    URLPattern::ParseResult parse_result = pattern.Parse(permission_str);
    if (parse_result == URLPattern::ParseResult::kSuccess) {
      // The path component is not used for host permissions, so we force it
      // to match all paths.
      pattern.SetPath("/*");
      int valid_schemes = pattern.valid_schemes();
      if (pattern.MatchesScheme(url::kFileScheme) &&
          !can_execute_script_everywhere) {
        extension->set_wants_file_access(true);
        if (!(extension->creation_flags() & Extension::ALLOW_FILE_ACCESS))
          valid_schemes &= ~URLPattern::SCHEME_FILE;
      }

      if (pattern.scheme() != content::kChromeUIScheme &&
          !all_urls_includes_chrome_urls) {
        // Keep chrome:// in allowed schemes only if it's explicitly requested
        // or been granted by extension ID. If the extensions_on_chrome_urls
        // flag is not set, CanSpecifyHostPermission will fail, so don't check
        // the flag here.
        valid_schemes &= ~URLPattern::SCHEME_CHROMEUI;
      }
      pattern.SetValidSchemes(valid_schemes);

      if (!CanSpecifyHostPermission(extension, pattern, *api_permissions)) {
        // TODO(aboxhall): make a warning (see pattern.match_all_urls() block
        // below).
        extension->AddInstallWarning(InstallWarning(
            ErrorUtils::FormatErrorMessage(errors::kInvalidPermissionScheme,
                                           permission_str),
            key,
            permission_str));
        continue;
      }

      host_permissions->AddPattern(pattern);
      // We need to make sure all_urls matches chrome://favicon and (maybe)
      // chrome://thumbnail, so add them back in to host_permissions separately.
      if (pattern.match_all_urls()) {
        host_permissions->AddPatterns(
            ExtensionsClient::Get()->GetPermittedChromeSchemeHosts(
                extension, *api_permissions));
      }
      continue;
    }

    // It's probably an unknown API permission. Do not throw an error so
    // extensions can retain backwards compatability (http://crbug.com/42742).
    extension->AddInstallWarning(InstallWarning(
        ErrorUtils::FormatErrorMessage(
            manifest_errors::kPermissionUnknownOrMalformed, permission_str),
        key,
        permission_str));
  }

  return true;
}

}  // namespace

struct PermissionsParser::InitialPermissions {
  APIPermissionSet api_permissions;
  ManifestPermissionSet manifest_permissions;
  URLPatternSet host_permissions;
  URLPatternSet scriptable_hosts;
};

PermissionsParser::PermissionsParser() {
}

PermissionsParser::~PermissionsParser() {
}

bool PermissionsParser::Parse(Extension* extension, base::string16* error) {
  initial_required_permissions_.reset(new InitialPermissions);
  if (!ParseHelper(extension,
                   keys::kPermissions,
                   &initial_required_permissions_->api_permissions,
                   &initial_required_permissions_->host_permissions,
                   error)) {
    return false;
  }

  initial_optional_permissions_.reset(new InitialPermissions);
  if (!ParseHelper(extension,
                   keys::kOptionalPermissions,
                   &initial_optional_permissions_->api_permissions,
                   &initial_optional_permissions_->host_permissions,
                   error)) {
    return false;
  }

  return true;
}

void PermissionsParser::Finalize(Extension* extension) {
  ManifestHandler::AddExtensionInitialRequiredPermissions(
      extension, &initial_required_permissions_->manifest_permissions);

  std::unique_ptr<const PermissionSet> required_permissions(
      new PermissionSet(initial_required_permissions_->api_permissions,
                        initial_required_permissions_->manifest_permissions,
                        initial_required_permissions_->host_permissions,
                        initial_required_permissions_->scriptable_hosts));
  extension->SetManifestData(
      keys::kPermissions,
      std::make_unique<ManifestPermissions>(std::move(required_permissions)));

  std::unique_ptr<const PermissionSet> optional_permissions(new PermissionSet(
      initial_optional_permissions_->api_permissions,
      initial_optional_permissions_->manifest_permissions,
      initial_optional_permissions_->host_permissions, URLPatternSet()));
  extension->SetManifestData(
      keys::kOptionalPermissions,
      std::make_unique<ManifestPermissions>(std::move(optional_permissions)));
}

// static
void PermissionsParser::AddAPIPermission(Extension* extension,
                                         APIPermission::ID permission) {
  DCHECK(extension->permissions_parser());
  extension->permissions_parser()
      ->initial_required_permissions_->api_permissions.insert(permission);
}

// static
void PermissionsParser::AddAPIPermission(Extension* extension,
                                         APIPermission* permission) {
  DCHECK(extension->permissions_parser());
  extension->permissions_parser()
      ->initial_required_permissions_->api_permissions.insert(permission);
}

// static
bool PermissionsParser::HasAPIPermission(const Extension* extension,
                                         APIPermission::ID permission) {
  DCHECK(extension->permissions_parser());
  return extension->permissions_parser()
             ->initial_required_permissions_->api_permissions.count(
                 permission) > 0;
}

// static
void PermissionsParser::SetScriptableHosts(
    Extension* extension,
    const URLPatternSet& scriptable_hosts) {
  DCHECK(extension->permissions_parser());
  extension->permissions_parser()
      ->initial_required_permissions_->scriptable_hosts = scriptable_hosts;
}

// static
const PermissionSet& PermissionsParser::GetRequiredPermissions(
    const Extension* extension) {
  DCHECK(extension->GetManifestData(keys::kPermissions));
  return *static_cast<const ManifestPermissions*>(
              extension->GetManifestData(keys::kPermissions))
              ->permissions;
}

// static
const PermissionSet& PermissionsParser::GetOptionalPermissions(
    const Extension* extension) {
  DCHECK(extension->GetManifestData(keys::kOptionalPermissions));
  return *static_cast<const ManifestPermissions*>(
              extension->GetManifestData(keys::kOptionalPermissions))
              ->permissions;
}

}  // namespace extensions
