// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/permissions_data.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/switches.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/common/user_script.h"
#include "url/gurl.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

namespace {

PermissionsData::PolicyDelegate* g_policy_delegate = NULL;

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
      pattern.MatchesScheme(content::kChromeUIScheme)) {
    URLPatternSet chrome_scheme_hosts = ExtensionsClient::Get()->
        GetPermittedChromeSchemeHosts(extension, permissions);
    if (chrome_scheme_hosts.ContainsPattern(pattern))
      return true;

    // Component extensions can have access to all of chrome://*.
    if (PermissionsData::CanExecuteScriptEverywhere(extension))
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
          permissions, APIPermissionSet::kDisallowInternalPermissions,
          api_permissions, error, &host_data)) {
    return false;
  }

  // Verify feature availability of permissions.
  std::vector<APIPermission::ID> to_remove;
  FeatureProvider* permission_features =
      FeatureProvider::GetPermissionFeatures();
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
      extension->AddInstallWarning(InstallWarning(availability.message(),
                                                  feature->name()));
      to_remove.push_back(iter->id());
      continue;
    }

    if (iter->id() == APIPermission::kExperimental) {
      if (!CanSpecifyExperimentalPermission(extension)) {
        *error = base::ASCIIToUTF16(errors::kExperimentalFlagRequired);
        return false;
      }
    }
  }

  api_permissions->AddImpliedPermissions();

  // Remove permissions that are not available to this extension.
  for (std::vector<APIPermission::ID>::const_iterator iter = to_remove.begin();
       iter != to_remove.end(); ++iter) {
      api_permissions->erase(*iter);
  }

  // Parse host pattern permissions.
  const int kAllowedSchemes =
      PermissionsData::CanExecuteScriptEverywhere(extension) ?
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
      if (pattern.MatchesScheme(content::kFileScheme) &&
          !PermissionsData::CanExecuteScriptEverywhere(extension)) {
        extension->set_wants_file_access(true);
        if (!(extension->creation_flags() & Extension::ALLOW_FILE_ACCESS))
          valid_schemes &= ~URLPattern::SCHEME_FILE;
      }

      if (pattern.scheme() != content::kChromeUIScheme &&
          !PermissionsData::CanExecuteScriptEverywhere(extension)) {
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
        extension->AddInstallWarning(InstallWarning(
            ErrorUtils::FormatErrorMessage(
                errors::kInvalidPermissionScheme, permission_str),
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
            manifest_errors::kPermissionUnknownOrMalformed,
            permission_str),
        key,
        permission_str));
  }

  return true;
}

// Returns true if this extension id is from a trusted provider.
bool IsTrustedId(const std::string& extension_id) {
  // See http://b/4946060 for more details.
  return extension_id == std::string("nckgahadagoaajjgafhacjanaoiihapd");
}

}  // namespace

struct PermissionsData::InitialPermissions {
  APIPermissionSet api_permissions;
  ManifestPermissionSet manifest_permissions;
  URLPatternSet host_permissions;
  URLPatternSet scriptable_hosts;
};

PermissionsData::PermissionsData() {
}

PermissionsData::~PermissionsData() {
}

// static
void PermissionsData::SetPolicyDelegate(PolicyDelegate* delegate) {
  g_policy_delegate = delegate;
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

// static
void PermissionsData::SetInitialScriptableHosts(
    Extension* extension, const URLPatternSet& scriptable_hosts) {
  extension->permissions_data()->
      initial_required_permissions_->scriptable_hosts = scriptable_hosts;
}

// static
void PermissionsData::SetActivePermissions(const Extension* extension,
                                           const PermissionSet* permissions) {
  base::AutoLock auto_lock(extension->permissions_data()->runtime_lock_);
  extension->permissions_data()->active_permissions_ = permissions;
}

// static
scoped_refptr<const PermissionSet> PermissionsData::GetActivePermissions(
    const Extension* extension) {
  return extension->permissions_data()->active_permissions_;
}

// static
scoped_refptr<const PermissionSet> PermissionsData::GetTabSpecificPermissions(
    const Extension* extension,
    int tab_id) {
  CHECK_GE(tab_id, 0);
  TabPermissionsMap::const_iterator iter =
      extension->permissions_data()->tab_specific_permissions_.find(tab_id);
  return
      (iter != extension->permissions_data()->tab_specific_permissions_.end())
          ? iter->second
          : NULL;
}

// static
void PermissionsData::UpdateTabSpecificPermissions(
    const Extension* extension,
    int tab_id,
    scoped_refptr<const PermissionSet> permissions) {
  CHECK_GE(tab_id, 0);
  TabPermissionsMap* tab_permissions =
      &extension->permissions_data()->tab_specific_permissions_;
  if (tab_permissions->count(tab_id)) {
    (*tab_permissions)[tab_id] = PermissionSet::CreateUnion(
        (*tab_permissions)[tab_id].get(), permissions.get());
  } else {
    (*tab_permissions)[tab_id] = permissions;
  }
}

// static
void PermissionsData::ClearTabSpecificPermissions(
    const Extension* extension,
    int tab_id) {
  CHECK_GE(tab_id, 0);
  extension->permissions_data()->tab_specific_permissions_.erase(tab_id);
}

// static
bool PermissionsData::HasAPIPermission(const Extension* extension,
                                       APIPermission::ID permission) {
  base::AutoLock auto_lock(extension->permissions_data()->runtime_lock_);
  return GetActivePermissions(extension)->HasAPIPermission(permission);
}

// static
bool PermissionsData::HasAPIPermission(
    const Extension* extension,
    const std::string& permission_name) {
  base::AutoLock auto_lock(extension->permissions_data()->runtime_lock_);
  return GetActivePermissions(extension)->HasAPIPermission(permission_name);
}

// static
bool PermissionsData::HasAPIPermissionForTab(
    const Extension* extension,
    int tab_id,
    APIPermission::ID permission) {
  if (HasAPIPermission(extension, permission))
    return true;

  // Place autolock below the HasAPIPermission() check, since HasAPIPermission
  // also acquires the lock.
  base::AutoLock auto_lock(extension->permissions_data()->runtime_lock_);
  scoped_refptr<const PermissionSet> tab_permissions =
      GetTabSpecificPermissions(extension, tab_id);
  return tab_permissions.get() && tab_permissions->HasAPIPermission(permission);
}

// static
bool PermissionsData::CheckAPIPermissionWithParam(
    const Extension* extension,
    APIPermission::ID permission,
    const APIPermission::CheckParam* param) {
  base::AutoLock auto_lock(extension->permissions_data()->runtime_lock_);
  return GetActivePermissions(extension)->CheckAPIPermissionWithParam(
      permission, param);
}

// static
const URLPatternSet& PermissionsData::GetEffectiveHostPermissions(
    const Extension* extension) {
  base::AutoLock auto_lock(extension->permissions_data()->runtime_lock_);
  return GetActivePermissions(extension)->effective_hosts();
}

// static
bool PermissionsData::CanSilentlyIncreasePermissions(
    const Extension* extension) {
  return extension->location() != Manifest::INTERNAL;
}

// static
bool PermissionsData::ShouldSkipPermissionWarnings(const Extension* extension) {
  return IsTrustedId(extension->id());
}

// static
bool PermissionsData::HasHostPermission(const Extension* extension,
                                        const GURL& url) {
  base::AutoLock auto_lock(extension->permissions_data()->runtime_lock_);
  return GetActivePermissions(extension)->HasExplicitAccessToOrigin(url);
}

// static
bool PermissionsData::HasEffectiveAccessToAllHosts(const Extension* extension) {
  base::AutoLock auto_lock(extension->permissions_data()->runtime_lock_);
  return GetActivePermissions(extension)->HasEffectiveAccessToAllHosts();
}

// static
PermissionMessages PermissionsData::GetPermissionMessages(
    const Extension* extension) {
  base::AutoLock auto_lock(extension->permissions_data()->runtime_lock_);
  if (ShouldSkipPermissionWarnings(extension)) {
    return PermissionMessages();
  } else {
    return PermissionMessageProvider::Get()->GetPermissionMessages(
        GetActivePermissions(extension), extension->GetType());
  }
}

// static
std::vector<base::string16> PermissionsData::GetPermissionMessageStrings(
    const Extension* extension) {
  base::AutoLock auto_lock(extension->permissions_data()->runtime_lock_);
  if (ShouldSkipPermissionWarnings(extension)) {
    return std::vector<base::string16>();
  } else {
    return PermissionMessageProvider::Get()->GetWarningMessages(
        GetActivePermissions(extension), extension->GetType());
  }
}

// static
std::vector<base::string16> PermissionsData::GetPermissionMessageDetailsStrings(
    const Extension* extension) {
  base::AutoLock auto_lock(extension->permissions_data()->runtime_lock_);
  if (ShouldSkipPermissionWarnings(extension)) {
    return std::vector<base::string16>();
  } else {
    return PermissionMessageProvider::Get()->GetWarningMessagesDetails(
        GetActivePermissions(extension), extension->GetType());
  }
}

// static
bool PermissionsData::CanExecuteScriptOnPage(const Extension* extension,
                                             const GURL& document_url,
                                             const GURL& top_frame_url,
                                             int tab_id,
                                             const UserScript* script,
                                             int process_id,
                                             std::string* error) {
  base::AutoLock auto_lock(extension->permissions_data()->runtime_lock_);
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  bool can_execute_everywhere = CanExecuteScriptEverywhere(extension);

  if (g_policy_delegate &&
      !g_policy_delegate->CanExecuteScriptOnPage(
           extension, document_url, top_frame_url, tab_id,
           script, process_id, error))
    return false;

  if (!can_execute_everywhere &&
      !ExtensionsClient::Get()->IsScriptableURL(document_url, error)) {
    return false;
  }

  if (!command_line->HasSwitch(switches::kExtensionsOnChromeURLs)) {
    if (document_url.SchemeIs(content::kChromeUIScheme) &&
        !can_execute_everywhere) {
      if (error)
        *error = errors::kCannotAccessChromeUrl;
      return false;
    }
  }

  if (top_frame_url.SchemeIs(extensions::kExtensionScheme) &&
      top_frame_url.GetOrigin() !=
          Extension::GetBaseURLFromExtensionId(extension->id()).GetOrigin() &&
      !can_execute_everywhere) {
    if (error)
      *error = errors::kCannotAccessExtensionUrl;
    return false;
  }

  // If a tab ID is specified, try the tab-specific permissions.
  if (tab_id >= 0) {
    scoped_refptr<const PermissionSet> tab_permissions =
        GetTabSpecificPermissions(extension, tab_id);
    if (tab_permissions.get() &&
        tab_permissions->explicit_hosts().MatchesSecurityOrigin(document_url)) {
      return true;
    }
  }

  bool can_access = false;

  if (script) {
    // If a script is specified, use its matches.
    can_access = script->MatchesURL(document_url);
  } else {
    // Otherwise, see if this extension has permission to execute script
    // programmatically on pages.
    can_access = GetActivePermissions(extension)->
        HasExplicitAccessToOrigin(document_url);
  }

  if (!can_access && error) {
    *error = ErrorUtils::FormatErrorMessage(errors::kCannotAccessPage,
                                            document_url.spec());
  }

  return can_access;
}

// static
bool PermissionsData::CanExecuteScriptEverywhere(const Extension* extension) {
  if (extension->location() == Manifest::COMPONENT)
    return true;

  const ExtensionsClient::ScriptingWhitelist& whitelist =
      ExtensionsClient::Get()->GetScriptingWhitelist();

  return std::find(whitelist.begin(), whitelist.end(), extension->id()) !=
      whitelist.end();
}

// static
bool PermissionsData::CanCaptureVisiblePage(const Extension* extension,
                                            int tab_id,
                                            std::string* error) {
  scoped_refptr<const PermissionSet> active_permissions =
      GetActivePermissions(extension);
  const URLPattern all_urls(URLPattern::SCHEME_ALL,
                            URLPattern::kAllUrlsPattern);
  if (active_permissions->explicit_hosts().ContainsPattern(all_urls))
    return true;

  if (tab_id >= 0) {
    scoped_refptr<const PermissionSet> tab_permissions =
        GetTabSpecificPermissions(extension, tab_id);
    if (tab_permissions &&
        tab_permissions->HasAPIPermission(APIPermission::kTab)) {
      return true;
    }
    if (error)
      *error = errors::kActiveTabPermissionNotGranted;
    return false;
  }

  if (error)
    *error = errors::kAllURLOrActiveTabNeeded;
  return false;
}

bool PermissionsData::ParsePermissions(Extension* extension,
                                       base::string16* error) {
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

void PermissionsData::InitializeManifestPermissions(Extension* extension) {
  ManifestHandler::AddExtensionInitialRequiredPermissions(
    extension, &initial_required_permissions_->manifest_permissions);
}

void PermissionsData::FinalizePermissions(Extension* extension) {
  active_permissions_ = new PermissionSet(
      initial_required_permissions_->api_permissions,
      initial_required_permissions_->manifest_permissions,
      initial_required_permissions_->host_permissions,
      initial_required_permissions_->scriptable_hosts);

  required_permission_set_ = new PermissionSet(
      initial_required_permissions_->api_permissions,
      initial_required_permissions_->manifest_permissions,
      initial_required_permissions_->host_permissions,
      initial_required_permissions_->scriptable_hosts);

  optional_permission_set_ = new PermissionSet(
      initial_optional_permissions_->api_permissions,
      initial_optional_permissions_->manifest_permissions,
      initial_optional_permissions_->host_permissions,
      URLPatternSet());

  initial_required_permissions_.reset();
  initial_optional_permissions_.reset();
}

}  // namespace extensions
