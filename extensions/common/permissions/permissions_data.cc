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
#include "url/url_constants.h"

namespace extensions {

namespace {

PermissionsData::PolicyDelegate* g_policy_delegate = NULL;

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
  withheld_permissions_unsafe_ = new PermissionSet();
}

PermissionsData::~PermissionsData() {
}

// static
void PermissionsData::SetPolicyDelegate(PolicyDelegate* delegate) {
  g_policy_delegate = delegate;
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

bool PermissionsData::ShouldSkipPermissionWarnings(
    const std::string& extension_id) {
  // See http://b/4946060 for more details.
  return extension_id == std::string("nckgahadagoaajjgafhacjanaoiihapd");
}

// static
bool PermissionsData::IsRestrictedUrl(const GURL& document_url,
                                      const GURL& top_frame_url,
                                      const Extension* extension,
                                      std::string* error) {
  if (extension && CanExecuteScriptEverywhere(extension))
    return false;

  // Check if the scheme is valid for extensions. If not, return.
  if (!URLPattern::IsValidSchemeForExtensions(document_url.scheme()) &&
      document_url.spec() != url::kAboutBlankURL) {
    if (error) {
      *error = ErrorUtils::FormatErrorMessage(
                   manifest_errors::kCannotAccessPage,
                   document_url.spec());
    }
    return true;
  }

  if (!ExtensionsClient::Get()->IsScriptableURL(document_url, error))
    return true;

  bool allow_on_chrome_urls = base::CommandLine::ForCurrentProcess()->HasSwitch(
                                  switches::kExtensionsOnChromeURLs);
  if (document_url.SchemeIs(content::kChromeUIScheme) &&
      !allow_on_chrome_urls) {
    if (error)
      *error = manifest_errors::kCannotAccessChromeUrl;
    return true;
  }

  if (extension && top_frame_url.SchemeIs(kExtensionScheme) &&
      top_frame_url.host() != extension->id() && !allow_on_chrome_urls) {
    if (error)
      *error = manifest_errors::kCannotAccessExtensionUrl;
    return true;
  }

  return false;
}

void PermissionsData::SetPermissions(
    const scoped_refptr<const PermissionSet>& active,
    const scoped_refptr<const PermissionSet>& withheld) const {
  base::AutoLock auto_lock(runtime_lock_);
  active_permissions_unsafe_ = active;
  withheld_permissions_unsafe_ = withheld;
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

bool PermissionsData::HasWithheldImpliedAllHosts() const {
  // Since we currently only withhold all_hosts, it's sufficient to check
  // that either set is not empty.
  return !withheld_permissions()->explicit_hosts().is_empty() ||
         !withheld_permissions()->scriptable_hosts().is_empty();
}

bool PermissionsData::CanAccessPage(const Extension* extension,
                                    const GURL& document_url,
                                    const GURL& top_frame_url,
                                    int tab_id,
                                    int process_id,
                                    std::string* error) const {
  AccessType result = CanRunOnPage(extension,
                                   document_url,
                                   top_frame_url,
                                   tab_id,
                                   process_id,
                                   active_permissions()->explicit_hosts(),
                                   withheld_permissions()->explicit_hosts(),
                                   error);
  // TODO(rdevlin.cronin) Update callers so that they only need ACCESS_ALLOWED.
  return result == ACCESS_ALLOWED || result == ACCESS_WITHHELD;
}

PermissionsData::AccessType PermissionsData::GetPageAccess(
    const Extension* extension,
    const GURL& document_url,
    const GURL& top_frame_url,
    int tab_id,
    int process_id,
    std::string* error) const {
  return CanRunOnPage(extension,
                      document_url,
                      top_frame_url,
                      tab_id,
                      process_id,
                      active_permissions()->explicit_hosts(),
                      withheld_permissions()->explicit_hosts(),
                      error);
}

bool PermissionsData::CanRunContentScriptOnPage(const Extension* extension,
                                                const GURL& document_url,
                                                const GURL& top_frame_url,
                                                int tab_id,
                                                int process_id,
                                                std::string* error) const {
  AccessType result = CanRunOnPage(extension,
                                   document_url,
                                   top_frame_url,
                                   tab_id,
                                   process_id,
                                   active_permissions()->scriptable_hosts(),
                                   withheld_permissions()->scriptable_hosts(),
                                   error);
  // TODO(rdevlin.cronin) Update callers so that they only need ACCESS_ALLOWED.
  return result == ACCESS_ALLOWED || result == ACCESS_WITHHELD;
}

PermissionsData::AccessType PermissionsData::GetContentScriptAccess(
    const Extension* extension,
    const GURL& document_url,
    const GURL& top_frame_url,
    int tab_id,
    int process_id,
    std::string* error) const {
  return CanRunOnPage(extension,
                      document_url,
                      top_frame_url,
                      tab_id,
                      process_id,
                      active_permissions()->scriptable_hosts(),
                      withheld_permissions()->scriptable_hosts(),
                      error);
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

PermissionsData::AccessType PermissionsData::CanRunOnPage(
    const Extension* extension,
    const GURL& document_url,
    const GURL& top_frame_url,
    int tab_id,
    int process_id,
    const URLPatternSet& permitted_url_patterns,
    const URLPatternSet& withheld_url_patterns,
    std::string* error) const {
  if (g_policy_delegate &&
      !g_policy_delegate->CanExecuteScriptOnPage(
          extension, document_url, top_frame_url, tab_id, process_id, error)) {
    return ACCESS_DENIED;
  }

  if (IsRestrictedUrl(document_url, top_frame_url, extension, error))
    return ACCESS_DENIED;

  if (HasTabSpecificPermissionToExecuteScript(tab_id, top_frame_url))
    return ACCESS_ALLOWED;

  if (permitted_url_patterns.MatchesURL(document_url))
    return ACCESS_ALLOWED;

  if (withheld_url_patterns.MatchesURL(document_url))
    return ACCESS_WITHHELD;

  if (error) {
    *error = ErrorUtils::FormatErrorMessage(manifest_errors::kCannotAccessPage,
                                            document_url.spec());
  }
  return ACCESS_DENIED;
}

}  // namespace extensions
