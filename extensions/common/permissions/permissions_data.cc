// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/permissions_data.h"

#include "base/command_line.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/switches.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/common/user_script.h"
#include "url/gurl.h"

namespace extensions {

namespace {

PermissionsData::PolicyDelegate* g_policy_delegate = NULL;

// Returns true if this extension id is from a trusted provider.
bool ShouldSkipPermissionWarnings(const std::string& extension_id) {
  // See http://b/4946060 for more details.
  return extension_id == std::string("nckgahadagoaajjgafhacjanaoiihapd");
}

}  // namespace

PermissionsData::PermissionsData(const Extension* extension)
    : extension_id_(extension->id()), manifest_type_(extension->GetType()) {
  base::AutoLock auto_lock(runtime_lock_);
  scoped_refptr<const PermissionSet> required_permissions =
      PermissionsParser::GetRequiredPermissions(extension);
  active_permissions_unsafe_ =
      new PermissionSet(required_permissions->apis(),
                        required_permissions->manifest_permissions(),
                        required_permissions->explicit_hosts(),
                        required_permissions->scriptable_hosts());
}

PermissionsData::~PermissionsData() {
}

// static
void PermissionsData::SetPolicyDelegate(PolicyDelegate* delegate) {
  g_policy_delegate = delegate;
}

// static
const PermissionsData* PermissionsData::ForExtension(
    const Extension* extension) {
  // TODO(rdevlin.cronin): Figure out what we're doing with this (i.e. whether
  // or not we want to expose PermissionsData on the extension).
  return extension->permissions_data();
}

// static
bool PermissionsData::CanSilentlyIncreasePermissions(
    const Extension* extension) {
  return extension->location() != Manifest::INTERNAL;
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

void PermissionsData::SetActivePermissions(
    const PermissionSet* permissions) const {
  base::AutoLock auto_lock(runtime_lock_);
  active_permissions_unsafe_ = permissions;
}

void PermissionsData::UpdateTabSpecificPermissions(
    int tab_id,
    scoped_refptr<const PermissionSet> permissions) const {
  base::AutoLock auto_lock(runtime_lock_);
  CHECK_GE(tab_id, 0);
  TabPermissionsMap::iterator iter = tab_specific_permissions_.find(tab_id);
  if (iter == tab_specific_permissions_.end())
    tab_specific_permissions_[tab_id] = permissions;
  else
    iter->second = PermissionSet::CreateUnion(iter->second, permissions);
}

void PermissionsData::ClearTabSpecificPermissions(int tab_id) const {
  base::AutoLock auto_lock(runtime_lock_);
  CHECK_GE(tab_id, 0);
  tab_specific_permissions_.erase(tab_id);
}

bool PermissionsData::HasAPIPermission(APIPermission::ID permission) const {
  return active_permissions()->HasAPIPermission(permission);
}

bool PermissionsData::HasAPIPermission(
    const std::string& permission_name) const {
  return active_permissions()->HasAPIPermission(permission_name);
}

bool PermissionsData::HasAPIPermissionForTab(
    int tab_id,
    APIPermission::ID permission) const {
  if (HasAPIPermission(permission))
    return true;

  scoped_refptr<const PermissionSet> tab_permissions =
      GetTabSpecificPermissions(tab_id);

  // Place autolock below the HasAPIPermission() and
  // GetTabSpecificPermissions(), since each already acquires the lock.
  base::AutoLock auto_lock(runtime_lock_);
  return tab_permissions.get() && tab_permissions->HasAPIPermission(permission);
}

bool PermissionsData::CheckAPIPermissionWithParam(
    APIPermission::ID permission,
    const APIPermission::CheckParam* param) const {
  return active_permissions()->CheckAPIPermissionWithParam(permission, param);
}

const URLPatternSet& PermissionsData::GetEffectiveHostPermissions() const {
  return active_permissions()->effective_hosts();
}

bool PermissionsData::HasHostPermission(const GURL& url) const {
  return active_permissions()->HasExplicitAccessToOrigin(url);
}

bool PermissionsData::HasEffectiveAccessToAllHosts() const {
  return active_permissions()->HasEffectiveAccessToAllHosts();
}

PermissionMessages PermissionsData::GetPermissionMessages() const {
  if (ShouldSkipPermissionWarnings(extension_id_)) {
    return PermissionMessages();
  } else {
    return PermissionMessageProvider::Get()->GetPermissionMessages(
        active_permissions(), manifest_type_);
  }
}

std::vector<base::string16> PermissionsData::GetPermissionMessageStrings()
    const {
  if (ShouldSkipPermissionWarnings(extension_id_))
    return std::vector<base::string16>();
  return PermissionMessageProvider::Get()->GetWarningMessages(
      active_permissions(), manifest_type_);
}

std::vector<base::string16>
PermissionsData::GetPermissionMessageDetailsStrings() const {
  if (ShouldSkipPermissionWarnings(extension_id_))
    return std::vector<base::string16>();
  return PermissionMessageProvider::Get()->GetWarningMessagesDetails(
      active_permissions(), manifest_type_);
}

bool PermissionsData::CanExecuteScriptOnPage(const Extension* extension,
                                             const GURL& document_url,
                                             const GURL& top_frame_url,
                                             int tab_id,
                                             const UserScript* script,
                                             int process_id,
                                             std::string* error) const {
  if (g_policy_delegate &&
      !g_policy_delegate->CanExecuteScriptOnPage(extension,
                                                 document_url,
                                                 top_frame_url,
                                                 tab_id,
                                                 script,
                                                 process_id,
                                                 error)) {
    return false;
  }

  bool can_execute_everywhere = CanExecuteScriptEverywhere(extension);
  if (!can_execute_everywhere &&
      !ExtensionsClient::Get()->IsScriptableURL(document_url, error)) {
    return false;
  }

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kExtensionsOnChromeURLs)) {
    if (document_url.SchemeIs(content::kChromeUIScheme) &&
        !can_execute_everywhere) {
      if (error)
        *error = manifest_errors::kCannotAccessChromeUrl;
      return false;
    }
  }

  if (top_frame_url.SchemeIs(kExtensionScheme) &&
      top_frame_url.GetOrigin() !=
          Extension::GetBaseURLFromExtensionId(extension->id()).GetOrigin() &&
      !can_execute_everywhere) {
    if (error)
      *error = manifest_errors::kCannotAccessExtensionUrl;
    return false;
  }

  if (HasTabSpecificPermissionToExecuteScript(tab_id, top_frame_url))
    return true;

  bool can_access = false;

  if (script) {
    // If a script is specified, use its matches.
    can_access = script->MatchesURL(document_url);
  } else {
    // Otherwise, see if this extension has permission to execute script
    // programmatically on pages.
    can_access = active_permissions()->HasExplicitAccessToOrigin(document_url);
  }

  if (!can_access && error) {
    *error = ErrorUtils::FormatErrorMessage(manifest_errors::kCannotAccessPage,
                                            document_url.spec());
  }

  return can_access;
}

bool PermissionsData::CanCaptureVisiblePage(int tab_id,
                                            std::string* error) const {
  const URLPattern all_urls(URLPattern::SCHEME_ALL,
                            URLPattern::kAllUrlsPattern);

  if (active_permissions()->explicit_hosts().ContainsPattern(all_urls))
    return true;

  if (tab_id >= 0) {
    scoped_refptr<const PermissionSet> tab_permissions =
        GetTabSpecificPermissions(tab_id);
    if (tab_permissions &&
        tab_permissions->HasAPIPermission(APIPermission::kTab)) {
      return true;
    }
    if (error)
      *error = manifest_errors::kActiveTabPermissionNotGranted;
    return false;
  }

  if (error)
    *error = manifest_errors::kAllURLOrActiveTabNeeded;
  return false;
}

// static
bool PermissionsData::RequiresActionForScriptExecution(
    const Extension* extension) const {
  return RequiresActionForScriptExecution(extension, -1, GURL());
}

// static
bool PermissionsData::RequiresActionForScriptExecution(
    const Extension* extension,
    int tab_id,
    const GURL& url) const {
  // For now, the user should be notified when an extension with all hosts
  // permission tries to execute a script on a page. Exceptions for policy-
  // enabled and component extensions, and extensions which are whitelisted to
  // execute scripts everywhere.
  if (!extension->ShouldDisplayInExtensionSettings() ||
      Manifest::IsPolicyLocation(extension->location()) ||
      Manifest::IsComponentLocation(extension->location()) ||
      CanExecuteScriptEverywhere(extension) ||
      !active_permissions()->ShouldWarnAllHosts()) {
    return false;
  }

  // If the extension has explicit permission to run on the given tab, then
  // we don't need to alert the user.
  if (HasTabSpecificPermissionToExecuteScript(tab_id, url))
    return false;

  return true;
}

scoped_refptr<const PermissionSet> PermissionsData::GetTabSpecificPermissions(
    int tab_id) const {
  base::AutoLock auto_lock(runtime_lock_);
  CHECK_GE(tab_id, 0);
  TabPermissionsMap::const_iterator iter =
      tab_specific_permissions_.find(tab_id);
  return (iter != tab_specific_permissions_.end()) ? iter->second : NULL;
}

bool PermissionsData::HasTabSpecificPermissionToExecuteScript(
    int tab_id,
    const GURL& url) const {
  if (tab_id >= 0) {
    scoped_refptr<const PermissionSet> tab_permissions =
        GetTabSpecificPermissions(tab_id);
    if (tab_permissions.get() &&
        tab_permissions->explicit_hosts().MatchesSecurityOrigin(url)) {
      return true;
    }
  }
  return false;
}

}  // namespace extensions
