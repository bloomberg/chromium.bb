// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/permissions_data.h"

#include <utility>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/switches.h"
#include "extensions/common/url_pattern_set.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace extensions {

namespace {

PermissionsData::PolicyDelegate* g_policy_delegate = nullptr;

struct DefaultRuntimePolicy {
  URLPatternSet blocked_hosts;
  URLPatternSet allowed_hosts;
};

// URLs an extension can't interact with. An extension can override these
// settings by declaring its own list of blocked and allowed hosts using
// policy_blocked_hosts and policy_allowed_hosts.
base::LazyInstance<DefaultRuntimePolicy>::Leaky default_runtime_policy =
    LAZY_INSTANCE_INITIALIZER;

class AutoLockOnValidThread {
 public:
  AutoLockOnValidThread(base::Lock& lock, base::ThreadChecker* thread_checker)
      : auto_lock_(lock) {
    DCHECK(!thread_checker || thread_checker->CalledOnValidThread());
  }

 private:
  base::AutoLock auto_lock_;

  DISALLOW_COPY_AND_ASSIGN(AutoLockOnValidThread);
};

}  // namespace

PermissionsData::PermissionsData(const Extension* extension)
    : extension_id_(extension->id()), manifest_type_(extension->GetType()) {
  const PermissionSet& required_permissions =
      PermissionsParser::GetRequiredPermissions(extension);
  active_permissions_unsafe_.reset(new PermissionSet(
      required_permissions.apis(), required_permissions.manifest_permissions(),
      required_permissions.explicit_hosts(),
      required_permissions.scriptable_hosts()));
  withheld_permissions_unsafe_.reset(new PermissionSet());
}

PermissionsData::~PermissionsData() {
}

// static
void PermissionsData::SetPolicyDelegate(PolicyDelegate* delegate) {
  g_policy_delegate = delegate;
}

// static
bool PermissionsData::CanExecuteScriptEverywhere(const Extension* extension) {
  if (extension->location() == Manifest::COMPONENT)
    return true;

  const ExtensionsClient::ScriptingWhitelist& whitelist =
      ExtensionsClient::Get()->GetScriptingWhitelist();

  return base::ContainsValue(whitelist, extension->id());
}

// static
bool PermissionsData::ShouldSkipPermissionWarnings(
    const std::string& extension_id) {
  // See http://b/4946060 for more details.
  return extension_id == extension_misc::kProdHangoutsExtensionId;
}

// static
bool PermissionsData::IsRestrictedUrl(const GURL& document_url,
                                      const Extension* extension,
                                      std::string* error) {
  if (extension && CanExecuteScriptEverywhere(extension))
    return false;

  // Check if the scheme is valid for extensions. If not, return.
  if (!URLPattern::IsValidSchemeForExtensions(document_url.scheme()) &&
      document_url.spec() != url::kAboutBlankURL) {
    if (error) {
      if (extension->permissions_data()->active_permissions().HasAPIPermission(
              APIPermission::kTab)) {
        *error = ErrorUtils::FormatErrorMessage(
            manifest_errors::kCannotAccessPageWithUrl, document_url.spec());
      } else {
        *error = manifest_errors::kCannotAccessPage;
      }
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

  if (extension && document_url.SchemeIs(kExtensionScheme) &&
      document_url.host() != extension->id() && !allow_on_chrome_urls) {
    if (error)
      *error = manifest_errors::kCannotAccessExtensionUrl;
    return true;
  }

  return false;
}

bool PermissionsData::UsesDefaultPolicyHostRestrictions() const {
  DCHECK(!thread_checker_ || thread_checker_->CalledOnValidThread());
  return uses_default_policy_host_restrictions;
}

const URLPatternSet& PermissionsData::default_policy_blocked_hosts() {
  return default_runtime_policy.Get().blocked_hosts;
}

const URLPatternSet& PermissionsData::default_policy_allowed_hosts() {
  return default_runtime_policy.Get().allowed_hosts;
}

const URLPatternSet PermissionsData::policy_blocked_hosts() const {
  base::AutoLock auto_lock(runtime_lock_);
  return PolicyBlockedHostsUnsafe();
}

const URLPatternSet& PermissionsData::PolicyBlockedHostsUnsafe() const {
  runtime_lock_.AssertAcquired();
  if (uses_default_policy_host_restrictions)
    return default_policy_blocked_hosts();
  return policy_blocked_hosts_unsafe_;
}

const URLPatternSet PermissionsData::policy_allowed_hosts() const {
  base::AutoLock auto_lock(runtime_lock_);
  return PolicyAllowedHostsUnsafe();
}

const URLPatternSet& PermissionsData::PolicyAllowedHostsUnsafe() const {
  runtime_lock_.AssertAcquired();
  if (uses_default_policy_host_restrictions)
    return default_policy_allowed_hosts();
  return policy_allowed_hosts_unsafe_;
}

void PermissionsData::BindToCurrentThread() const {
  DCHECK(!thread_checker_);
  thread_checker_.reset(new base::ThreadChecker());
}

void PermissionsData::SetPermissions(
    std::unique_ptr<const PermissionSet> active,
    std::unique_ptr<const PermissionSet> withheld) const {
  AutoLockOnValidThread lock(runtime_lock_, thread_checker_.get());
  active_permissions_unsafe_ = std::move(active);
  withheld_permissions_unsafe_ = std::move(withheld);
}

void PermissionsData::SetPolicyHostRestrictions(
    const URLPatternSet& runtime_blocked_hosts,
    const URLPatternSet& runtime_allowed_hosts) const {
  AutoLockOnValidThread lock(runtime_lock_, thread_checker_.get());
  policy_blocked_hosts_unsafe_ = runtime_blocked_hosts;
  policy_allowed_hosts_unsafe_ = runtime_allowed_hosts;
  uses_default_policy_host_restrictions = false;
}

void PermissionsData::SetUsesDefaultHostRestrictions() const {
  AutoLockOnValidThread lock(runtime_lock_, thread_checker_.get());
  uses_default_policy_host_restrictions = true;
}

// static
void PermissionsData::SetDefaultPolicyHostRestrictions(
    const URLPatternSet& default_runtime_blocked_hosts,
    const URLPatternSet& default_runtime_allowed_hosts) {
  default_runtime_policy.Get().blocked_hosts = default_runtime_blocked_hosts;
  default_runtime_policy.Get().allowed_hosts = default_runtime_allowed_hosts;
}

void PermissionsData::SetActivePermissions(
    std::unique_ptr<const PermissionSet> active) const {
  AutoLockOnValidThread lock(runtime_lock_, thread_checker_.get());
  active_permissions_unsafe_ = std::move(active);
}

void PermissionsData::UpdateTabSpecificPermissions(
    int tab_id,
    const PermissionSet& permissions) const {
  AutoLockOnValidThread lock(runtime_lock_, thread_checker_.get());
  CHECK_GE(tab_id, 0);
  TabPermissionsMap::const_iterator iter =
      tab_specific_permissions_.find(tab_id);
  std::unique_ptr<const PermissionSet> new_permissions =
      PermissionSet::CreateUnion(
          iter == tab_specific_permissions_.end()
              ? static_cast<const PermissionSet&>(PermissionSet())
              : *iter->second,
          permissions);
  tab_specific_permissions_[tab_id] = std::move(new_permissions);
}

void PermissionsData::ClearTabSpecificPermissions(int tab_id) const {
  AutoLockOnValidThread lock(runtime_lock_, thread_checker_.get());
  CHECK_GE(tab_id, 0);
  tab_specific_permissions_.erase(tab_id);
}

bool PermissionsData::HasAPIPermission(APIPermission::ID permission) const {
  base::AutoLock auto_lock(runtime_lock_);
  return active_permissions_unsafe_->HasAPIPermission(permission);
}

bool PermissionsData::HasAPIPermission(
    const std::string& permission_name) const {
  base::AutoLock auto_lock(runtime_lock_);
  return active_permissions_unsafe_->HasAPIPermission(permission_name);
}

bool PermissionsData::HasAPIPermissionForTab(
    int tab_id,
    APIPermission::ID permission) const {
  base::AutoLock auto_lock(runtime_lock_);
  if (active_permissions_unsafe_->HasAPIPermission(permission))
    return true;

  const PermissionSet* tab_permissions = GetTabSpecificPermissions(tab_id);
  return tab_permissions && tab_permissions->HasAPIPermission(permission);
}

bool PermissionsData::CheckAPIPermissionWithParam(
    APIPermission::ID permission,
    const APIPermission::CheckParam* param) const {
  base::AutoLock auto_lock(runtime_lock_);
  return active_permissions_unsafe_->CheckAPIPermissionWithParam(permission,
                                                                 param);
}

URLPatternSet PermissionsData::GetEffectiveHostPermissions() const {
  base::AutoLock auto_lock(runtime_lock_);
  URLPatternSet effective_hosts = active_permissions_unsafe_->effective_hosts();
  for (const auto& val : tab_specific_permissions_)
    effective_hosts.AddPatterns(val.second->effective_hosts());
  return effective_hosts;
}

bool PermissionsData::HasHostPermission(const GURL& url) const {
  base::AutoLock auto_lock(runtime_lock_);
  return active_permissions_unsafe_->HasExplicitAccessToOrigin(url) &&
         !IsRuntimeBlockedHostUnsafe(url);
}

bool PermissionsData::HasEffectiveAccessToAllHosts() const {
  base::AutoLock auto_lock(runtime_lock_);
  return active_permissions_unsafe_->HasEffectiveAccessToAllHosts();
}

PermissionMessages PermissionsData::GetPermissionMessages() const {
  base::AutoLock auto_lock(runtime_lock_);
  return PermissionMessageProvider::Get()->GetPermissionMessages(
      PermissionMessageProvider::Get()->GetAllPermissionIDs(
          *active_permissions_unsafe_, manifest_type_));
}

PermissionMessages PermissionsData::GetNewPermissionMessages(
    const PermissionSet& granted_permissions) const {
  base::AutoLock auto_lock(runtime_lock_);

  std::unique_ptr<const PermissionSet> new_permissions =
      PermissionSet::CreateDifference(*active_permissions_unsafe_,
                                      granted_permissions);

  return PermissionMessageProvider::Get()->GetPermissionMessages(
      PermissionMessageProvider::Get()->GetAllPermissionIDs(*new_permissions,
                                                            manifest_type_));
}

bool PermissionsData::HasWithheldImpliedAllHosts() const {
  base::AutoLock auto_lock(runtime_lock_);
  // Since we currently only withhold all_hosts, it's sufficient to check
  // that either set is not empty.
  return !withheld_permissions_unsafe_->explicit_hosts().is_empty() ||
         !withheld_permissions_unsafe_->scriptable_hosts().is_empty();
}

bool PermissionsData::CanAccessPage(const Extension* extension,
                                    const GURL& document_url,
                                    int tab_id,
                                    std::string* error) const {
  AccessType result = GetPageAccess(extension, document_url, tab_id, error);

  // TODO(rdevlin.cronin) Update callers so that they only need ACCESS_ALLOWED.
  return result == ACCESS_ALLOWED || result == ACCESS_WITHHELD;
}

PermissionsData::AccessType PermissionsData::GetPageAccess(
    const Extension* extension,
    const GURL& document_url,
    int tab_id,
    std::string* error) const {
  base::AutoLock auto_lock(runtime_lock_);

  const PermissionSet* tab_permissions = GetTabSpecificPermissions(tab_id);
  return CanRunOnPage(
      extension, document_url, tab_id,
      active_permissions_unsafe_->explicit_hosts(),
      withheld_permissions_unsafe_->explicit_hosts(),
      tab_permissions ? &tab_permissions->explicit_hosts() : nullptr, error);
}

bool PermissionsData::CanRunContentScriptOnPage(const Extension* extension,
                                                const GURL& document_url,
                                                int tab_id,
                                                std::string* error) const {
  AccessType result =
      GetContentScriptAccess(extension, document_url, tab_id, error);

  // TODO(rdevlin.cronin) Update callers so that they only need ACCESS_ALLOWED.
  return result == ACCESS_ALLOWED || result == ACCESS_WITHHELD;
}

PermissionsData::AccessType PermissionsData::GetContentScriptAccess(
    const Extension* extension,
    const GURL& document_url,
    int tab_id,
    std::string* error) const {
  base::AutoLock auto_lock(runtime_lock_);

  const PermissionSet* tab_permissions = GetTabSpecificPermissions(tab_id);
  return CanRunOnPage(
      extension, document_url, tab_id,
      active_permissions_unsafe_->scriptable_hosts(),
      withheld_permissions_unsafe_->scriptable_hosts(),
      tab_permissions ? &tab_permissions->scriptable_hosts() : nullptr, error);
}

bool PermissionsData::CanCaptureVisiblePage(const GURL& document_url,
                                            const Extension* extension,
                                            int tab_id,
                                            std::string* error) const {
  bool has_active_tab = false;
  {
    base::AutoLock auto_lock(runtime_lock_);
    const PermissionSet* tab_permissions = GetTabSpecificPermissions(tab_id);
    has_active_tab = tab_permissions &&
                     tab_permissions->HasAPIPermission(APIPermission::kTab);
  }
  // We check GetPageAccess() (in addition the the <all_urls> and activeTab
  // checks below) for the case of URLs that can be conditionally granted (such
  // as file:// URLs or chrome:// URLs for component extensions), and to respect
  // policy restrictions, if any. If an extension has <all_urls>,
  // GetPageAccess() will still (correctly) return false if, for instance, the
  // URL is a file:// URL and the extension does not have file access.
  // See https://crbug.com/810220.
  if (GetPageAccess(extension, document_url, tab_id, error) != ACCESS_ALLOWED) {
    if (!document_url.SchemeIs(content::kChromeUIScheme))
      return false;

    // Most extensions will not have (and cannot get) access to chrome:// URLs
    // (which are restricted). However, allowing them to capture these URLs can
    // be useful, such as in the case of capturing a screenshot. Allow
    // extensions that have been explicitly invoked (and have the activeTab)
    // permission to capture chrome:// URLs.
    if (has_active_tab)
      return true;

    if (error)
      *error = manifest_errors::kActiveTabPermissionNotGranted;

    return false;
  }

  const URLPattern all_urls(URLPattern::SCHEME_ALL,
                            URLPattern::kAllUrlsPattern);

  base::AutoLock auto_lock(runtime_lock_);
  if (active_permissions_unsafe_->explicit_hosts().ContainsPattern(all_urls))
    return true;

  const PermissionSet* tab_permissions = GetTabSpecificPermissions(tab_id);
  if (tab_permissions &&
      tab_permissions->HasAPIPermission(APIPermission::kTab)) {
    return true;
  }

  if (error)
    *error = manifest_errors::kAllURLOrActiveTabNeeded;
  return false;
}

const PermissionSet* PermissionsData::GetTabSpecificPermissions(
    int tab_id) const {
  runtime_lock_.AssertAcquired();
  TabPermissionsMap::const_iterator iter =
      tab_specific_permissions_.find(tab_id);
  return iter != tab_specific_permissions_.end() ? iter->second.get() : nullptr;
}

bool PermissionsData::IsRuntimeBlockedHostUnsafe(const GURL& url) const {
  runtime_lock_.AssertAcquired();
  return PolicyBlockedHostsUnsafe().MatchesURL(url) &&
         !PolicyAllowedHostsUnsafe().MatchesURL(url);
}

PermissionsData::AccessType PermissionsData::CanRunOnPage(
    const Extension* extension,
    const GURL& document_url,
    int tab_id,
    const URLPatternSet& permitted_url_patterns,
    const URLPatternSet& withheld_url_patterns,
    const URLPatternSet* tab_url_patterns,
    std::string* error) const {
  runtime_lock_.AssertAcquired();
  if (g_policy_delegate && !g_policy_delegate->CanExecuteScriptOnPage(
                               extension, document_url, tab_id, error))
    return ACCESS_DENIED;

  if (extension->location() != Manifest::COMPONENT &&
      extension->permissions_data()->IsRuntimeBlockedHostUnsafe(document_url)) {
    if (error)
      *error = extension_misc::kPolicyBlockedScripting;
    return ACCESS_DENIED;
  }

  if (IsRestrictedUrl(document_url, extension, error))
    return ACCESS_DENIED;

  if (tab_url_patterns && tab_url_patterns->MatchesURL(document_url))
    return ACCESS_ALLOWED;

  if (permitted_url_patterns.MatchesURL(document_url))
    return ACCESS_ALLOWED;

  if (withheld_url_patterns.MatchesURL(document_url))
    return ACCESS_WITHHELD;

  if (error) {
    if (extension->permissions_data()->active_permissions().HasAPIPermission(
            APIPermission::kTab)) {
      *error = ErrorUtils::FormatErrorMessage(
          manifest_errors::kCannotAccessPageWithUrl, document_url.spec());
    } else {
      *error = manifest_errors::kCannotAccessPage;
    }
  }

  return ACCESS_DENIED;
}

}  // namespace extensions
