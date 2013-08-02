// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/permission_set.h"

#include <algorithm>
#include <iterator>
#include <string>

#include "base/stl_util.h"
#include "chrome/common/extensions/permissions/chrome_scheme_hosts.h"
#include "chrome/common/extensions/permissions/media_galleries_permission.h"
#include "chrome/common/extensions/permissions/permissions_info.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "grit/generated_resources.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using extensions::URLPatternSet;

namespace {

// Helper for GetDistinctHosts(): com > net > org > everything else.
bool RcdBetterThan(const std::string& a, const std::string& b) {
  if (a == b)
    return false;
  if (a == "com")
    return true;
  if (a == "net")
    return b != "com";
  if (a == "org")
    return b != "com" && b != "net";
  return false;
}

void AddPatternsAndRemovePaths(const URLPatternSet& set, URLPatternSet* out) {
  DCHECK(out);
  for (URLPatternSet::const_iterator i = set.begin(); i != set.end(); ++i) {
    URLPattern p = *i;
    p.SetPath("/*");
    out->AddPattern(p);
  }
}

}  // namespace

namespace extensions {

//
// PermissionSet
//

PermissionSet::PermissionSet() {}

PermissionSet::PermissionSet(
    const APIPermissionSet& apis,
    const URLPatternSet& explicit_hosts,
    const URLPatternSet& scriptable_hosts)
    : apis_(apis),
      scriptable_hosts_(scriptable_hosts) {
  AddPatternsAndRemovePaths(explicit_hosts, &explicit_hosts_);
  InitImplicitPermissions();
  InitEffectiveHosts();
}

// static
PermissionSet* PermissionSet::CreateDifference(
    const PermissionSet* set1,
    const PermissionSet* set2) {
  scoped_refptr<PermissionSet> empty = new PermissionSet();
  const PermissionSet* set1_safe = (set1 == NULL) ? empty.get() : set1;
  const PermissionSet* set2_safe = (set2 == NULL) ? empty.get() : set2;

  APIPermissionSet apis;
  APIPermissionSet::Difference(set1_safe->apis(), set2_safe->apis(), &apis);

  URLPatternSet explicit_hosts;
  URLPatternSet::CreateDifference(set1_safe->explicit_hosts(),
                                  set2_safe->explicit_hosts(),
                                  &explicit_hosts);

  URLPatternSet scriptable_hosts;
  URLPatternSet::CreateDifference(set1_safe->scriptable_hosts(),
                                  set2_safe->scriptable_hosts(),
                                  &scriptable_hosts);

  return new PermissionSet(apis, explicit_hosts, scriptable_hosts);
}

// static
PermissionSet* PermissionSet::CreateIntersection(
    const PermissionSet* set1,
    const PermissionSet* set2) {
  scoped_refptr<PermissionSet> empty = new PermissionSet();
  const PermissionSet* set1_safe = (set1 == NULL) ? empty.get() : set1;
  const PermissionSet* set2_safe = (set2 == NULL) ? empty.get() : set2;

  APIPermissionSet apis;
  APIPermissionSet::Intersection(set1_safe->apis(), set2_safe->apis(), &apis);

  URLPatternSet explicit_hosts;
  URLPatternSet::CreateIntersection(set1_safe->explicit_hosts(),
                                    set2_safe->explicit_hosts(),
                                    &explicit_hosts);

  URLPatternSet scriptable_hosts;
  URLPatternSet::CreateIntersection(set1_safe->scriptable_hosts(),
                                    set2_safe->scriptable_hosts(),
                                    &scriptable_hosts);

  return new PermissionSet(apis, explicit_hosts, scriptable_hosts);
}

// static
PermissionSet* PermissionSet::CreateUnion(
    const PermissionSet* set1,
    const PermissionSet* set2) {
  scoped_refptr<PermissionSet> empty = new PermissionSet();
  const PermissionSet* set1_safe = (set1 == NULL) ? empty.get() : set1;
  const PermissionSet* set2_safe = (set2 == NULL) ? empty.get() : set2;

  APIPermissionSet apis;
  APIPermissionSet::Union(set1_safe->apis(), set2_safe->apis(), &apis);

  URLPatternSet explicit_hosts;
  URLPatternSet::CreateUnion(set1_safe->explicit_hosts(),
                             set2_safe->explicit_hosts(),
                             &explicit_hosts);

  URLPatternSet scriptable_hosts;
  URLPatternSet::CreateUnion(set1_safe->scriptable_hosts(),
                             set2_safe->scriptable_hosts(),
                             &scriptable_hosts);

  return new PermissionSet(apis, explicit_hosts, scriptable_hosts);
}

// static
PermissionSet* PermissionSet::ExcludeNotInManifestPermissions(
    const PermissionSet* set) {
  if (!set)
    return new PermissionSet();

  APIPermissionSet apis;
  for (APIPermissionSet::const_iterator i = set->apis().begin();
       i != set->apis().end(); ++i) {
    if (!i->ManifestEntryForbidden())
      apis.insert(i->Clone());
  }

  return new PermissionSet(
      apis, set->explicit_hosts(), set->scriptable_hosts());
}

bool PermissionSet::operator==(
    const PermissionSet& rhs) const {
  return apis_ == rhs.apis_ &&
      scriptable_hosts_ == rhs.scriptable_hosts_ &&
      explicit_hosts_ == rhs.explicit_hosts_;
}

bool PermissionSet::Contains(const PermissionSet& set) const {
  return apis_.Contains(set.apis()) &&
         explicit_hosts().Contains(set.explicit_hosts()) &&
         scriptable_hosts().Contains(set.scriptable_hosts());
}

std::set<std::string> PermissionSet::GetAPIsAsStrings() const {
  std::set<std::string> apis_str;
  for (APIPermissionSet::const_iterator i = apis_.begin();
       i != apis_.end(); ++i) {
    apis_str.insert(i->name());
  }
  return apis_str;
}

std::set<std::string> PermissionSet::GetDistinctHostsForDisplay() const {
  URLPatternSet hosts_displayed_as_url;
  // Filters out every URL pattern that matches chrome:// scheme.
  for (URLPatternSet::const_iterator i = effective_hosts_.begin();
       i != effective_hosts_.end(); ++i) {
    if (i->scheme() != chrome::kChromeUIScheme) {
      hosts_displayed_as_url.AddPattern(*i);
    }
  }
  return GetDistinctHosts(hosts_displayed_as_url, true, true);
}

PermissionMessages PermissionSet::GetPermissionMessages(
    Manifest::Type extension_type) const {
  PermissionMessages messages;

  if (HasEffectiveFullAccess()) {
    messages.push_back(PermissionMessage(
        PermissionMessage::kFullAccess,
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_FULL_ACCESS)));
    return messages;
  }

  std::set<PermissionMessage> host_msgs =
      GetHostPermissionMessages(extension_type);
  std::set<PermissionMessage> api_msgs = GetAPIPermissionMessages();
  messages.insert(messages.end(), host_msgs.begin(), host_msgs.end());
  messages.insert(messages.end(), api_msgs.begin(), api_msgs.end());

  return messages;
}

std::vector<string16> PermissionSet::GetWarningMessages(
    Manifest::Type extension_type) const {
  std::vector<string16> messages;
  PermissionMessages permissions = GetPermissionMessages(extension_type);

  bool audio_capture = false;
  bool video_capture = false;
  bool media_galleries_read = false;
  bool media_galleries_copy_to = false;
  for (PermissionMessages::const_iterator i = permissions.begin();
       i != permissions.end(); ++i) {
    switch (i->id()) {
      case PermissionMessage::kAudioCapture:
        audio_capture = true;
        break;
      case PermissionMessage::kVideoCapture:
        video_capture = true;
        break;
      case PermissionMessage::kMediaGalleriesAllGalleriesRead:
        media_galleries_read = true;
        break;
      case PermissionMessage::kMediaGalleriesAllGalleriesCopyTo:
        media_galleries_copy_to = true;
        break;
      default:
        break;
    }
  }

  for (PermissionMessages::const_iterator i = permissions.begin();
       i != permissions.end(); ++i) {
    int id = i->id();
    if (audio_capture && video_capture) {
      if (id == PermissionMessage::kAudioCapture) {
        messages.push_back(l10n_util::GetStringUTF16(
            IDS_EXTENSION_PROMPT_WARNING_AUDIO_AND_VIDEO_CAPTURE));
        continue;
      } else if (id == PermissionMessage::kVideoCapture) {
        // The combined message will be pushed above.
        continue;
      }
    }
    if (media_galleries_read && media_galleries_copy_to) {
      if (id == PermissionMessage::kMediaGalleriesAllGalleriesRead) {
        messages.push_back(l10n_util::GetStringUTF16(
            IDS_EXTENSION_PROMPT_WARNING_MEDIA_GALLERIES_READ_WRITE));
        continue;
      } else if (id == PermissionMessage::kMediaGalleriesAllGalleriesCopyTo) {
        // The combined message will be pushed above.
        continue;
      }
    }

    messages.push_back(i->message());
  }

  return messages;
}

std::vector<string16> PermissionSet::GetWarningMessagesDetails(
    Manifest::Type extension_type) const {
  std::vector<string16> messages;
  PermissionMessages permissions = GetPermissionMessages(extension_type);

  for (PermissionMessages::const_iterator i = permissions.begin();
       i != permissions.end(); ++i)
    messages.push_back(i->details());

  return messages;
}

bool PermissionSet::IsEmpty() const {
  // Not default if any host permissions are present.
  if (!(explicit_hosts().is_empty() && scriptable_hosts().is_empty()))
    return false;

  // Or if it has no api permissions.
  return apis().empty();
}

bool PermissionSet::HasAPIPermission(
    APIPermission::ID id) const {
  return apis().find(id) != apis().end();
}

bool PermissionSet::HasAPIPermission(const std::string& permission_name) const {
  const APIPermissionInfo* permission =
      PermissionsInfo::GetInstance()->GetByName(permission_name);
  CHECK(permission) << permission_name;
  return (permission && apis_.count(permission->id()));
}

bool PermissionSet::CheckAPIPermission(APIPermission::ID permission) const {
  return CheckAPIPermissionWithParam(permission, NULL);
}

bool PermissionSet::CheckAPIPermissionWithParam(
    APIPermission::ID permission,
    const APIPermission::CheckParam* param) const {
  APIPermissionSet::const_iterator iter = apis().find(permission);
  if (iter == apis().end())
    return false;
  return iter->Check(param);
}

bool PermissionSet::HasExplicitAccessToOrigin(
    const GURL& origin) const {
  return explicit_hosts().MatchesURL(origin);
}

bool PermissionSet::HasScriptableAccessToURL(
    const GURL& origin) const {
  // We only need to check our host list to verify access. The host list should
  // already reflect any special rules (such as chrome://favicon, all hosts
  // access, etc.).
  return scriptable_hosts().MatchesURL(origin);
}

bool PermissionSet::HasEffectiveAccessToAllHosts() const {
  // There are two ways this set can have effective access to all hosts:
  //  1) it has an <all_urls> URL pattern.
  //  2) it has a named permission with implied full URL access.
  for (URLPatternSet::const_iterator host = effective_hosts().begin();
       host != effective_hosts().end(); ++host) {
    if (host->match_all_urls() ||
        (host->match_subdomains() && host->host().empty()))
      return true;
  }

  for (APIPermissionSet::const_iterator i = apis().begin();
       i != apis().end(); ++i) {
    if (i->info()->implies_full_url_access())
      return true;
  }
  return false;
}

bool PermissionSet::HasEffectiveAccessToURL(const GURL& url) const {
  return effective_hosts().MatchesURL(url);
}

bool PermissionSet::HasEffectiveFullAccess() const {
  for (APIPermissionSet::const_iterator i = apis().begin();
       i != apis().end(); ++i) {
    if (i->info()->implies_full_access())
      return true;
  }
  return false;
}

bool PermissionSet::HasLessPrivilegesThan(
    const PermissionSet* permissions,
    Manifest::Type extension_type) const {
  // Things can't get worse than native code access.
  if (HasEffectiveFullAccess())
    return false;

  // Otherwise, it's a privilege increase if the new one has full access.
  if (permissions->HasEffectiveFullAccess())
    return true;

  if (HasLessHostPrivilegesThan(permissions, extension_type))
    return true;

  if (HasLessAPIPrivilegesThan(permissions))
    return true;

  return false;
}

PermissionSet::~PermissionSet() {}

// static
std::set<std::string> PermissionSet::GetDistinctHosts(
    const URLPatternSet& host_patterns,
    bool include_rcd,
    bool exclude_file_scheme) {
  // Use a vector to preserve order (also faster than a map on small sets).
  // Each item is a host split into two parts: host without RCDs and
  // current best RCD.
  typedef std::vector<std::pair<std::string, std::string> > HostVector;
  HostVector hosts_best_rcd;
  for (URLPatternSet::const_iterator i = host_patterns.begin();
       i != host_patterns.end(); ++i) {
    if (exclude_file_scheme && i->scheme() == chrome::kFileScheme)
      continue;

    std::string host = i->host();

    // Add the subdomain wildcard back to the host, if necessary.
    if (i->match_subdomains())
      host = "*." + host;

    // If the host has an RCD, split it off so we can detect duplicates.
    std::string rcd;
    size_t reg_len = net::registry_controlled_domains::GetRegistryLength(
        host,
        net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
        net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
    if (reg_len && reg_len != std::string::npos) {
      if (include_rcd)  // else leave rcd empty
        rcd = host.substr(host.size() - reg_len);
      host = host.substr(0, host.size() - reg_len);
    }

    // Check if we've already seen this host.
    HostVector::iterator it = hosts_best_rcd.begin();
    for (; it != hosts_best_rcd.end(); ++it) {
      if (it->first == host)
        break;
    }
    // If this host was found, replace the RCD if this one is better.
    if (it != hosts_best_rcd.end()) {
      if (include_rcd && RcdBetterThan(rcd, it->second))
        it->second = rcd;
    } else {  // Previously unseen host, append it.
      hosts_best_rcd.push_back(std::make_pair(host, rcd));
    }
  }

  // Build up the final vector by concatenating hosts and RCDs.
  std::set<std::string> distinct_hosts;
  for (HostVector::iterator it = hosts_best_rcd.begin();
       it != hosts_best_rcd.end(); ++it)
    distinct_hosts.insert(it->first + it->second);
  return distinct_hosts;
}

void PermissionSet::InitImplicitPermissions() {
  // The downloads permission implies the internal version as well.
  if (apis_.find(APIPermission::kDownloads) != apis_.end())
    apis_.insert(APIPermission::kDownloadsInternal);

  // TODO(fsamuel): Is there a better way to request access to the WebRequest
  // API without exposing it to the Chrome App?
  if (apis_.find(APIPermission::kWebView) != apis_.end())
    apis_.insert(APIPermission::kWebRequestInternal);

  // The webRequest permission implies the internal version as well.
  if (apis_.find(APIPermission::kWebRequest) != apis_.end())
    apis_.insert(APIPermission::kWebRequestInternal);

  // The fileBrowserHandler permission implies the internal version as well.
  if (apis_.find(APIPermission::kFileBrowserHandler) != apis_.end())
    apis_.insert(APIPermission::kFileBrowserHandlerInternal);
}

void PermissionSet::InitEffectiveHosts() {
  effective_hosts_.ClearPatterns();

  URLPatternSet::CreateUnion(
      explicit_hosts(), scriptable_hosts(), &effective_hosts_);
}

std::set<PermissionMessage> PermissionSet::GetAPIPermissionMessages() const {
  std::set<PermissionMessage> messages;
  for (APIPermissionSet::const_iterator permission_it = apis_.begin();
       permission_it != apis_.end(); ++permission_it) {
    DCHECK_GT(PermissionMessage::kNone,
              PermissionMessage::kUnknown);
    if (permission_it->HasMessages()) {
      PermissionMessages new_messages = permission_it->GetMessages();
      messages.insert(new_messages.begin(), new_messages.end());
    }
  }
  return messages;
}

std::set<PermissionMessage> PermissionSet::GetHostPermissionMessages(
    Manifest::Type extension_type) const {
  // Since platform apps always use isolated storage, they can't (silently)
  // access user data on other domains, so there's no need to prompt.
  // Note: this must remain consistent with HasLessHostPrivilegesThan.
  // See crbug.com/255229.
  std::set<PermissionMessage> messages;
  if (extension_type == Manifest::TYPE_PLATFORM_APP)
    return messages;

  if (HasEffectiveAccessToAllHosts()) {
    messages.insert(PermissionMessage(
        PermissionMessage::kHostsAll,
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_ALL_HOSTS)));
  } else {
    PermissionMessages additional_warnings =
        GetChromeSchemePermissionWarnings(effective_hosts_);
    for (size_t i = 0; i < additional_warnings.size(); ++i)
      messages.insert(additional_warnings[i]);

    std::set<std::string> hosts = GetDistinctHostsForDisplay();
    if (!hosts.empty())
      messages.insert(PermissionMessage::CreateFromHostList(hosts));
  }
  return messages;
}

bool PermissionSet::HasLessAPIPrivilegesThan(
    const PermissionSet* permissions) const {
  if (permissions == NULL)
    return false;

  typedef std::set<PermissionMessage> PermissionMsgSet;
  PermissionMsgSet current_warnings = GetAPIPermissionMessages();
  PermissionMsgSet new_warnings = permissions->GetAPIPermissionMessages();
  PermissionMsgSet delta_warnings =
      base::STLSetDifference<PermissionMsgSet>(new_warnings, current_warnings);

  // We have less privileges if there are additional warnings present.
  return !delta_warnings.empty();
}

bool PermissionSet::HasLessHostPrivilegesThan(
    const PermissionSet* permissions,
    Manifest::Type extension_type) const {
  // Platform apps host permission changes do not count as privilege increases.
  // Note: this must remain consistent with GetHostPermissionMessages.
  if (extension_type == Manifest::TYPE_PLATFORM_APP)
    return false;

  // If this permission set can access any host, then it can't be elevated.
  if (HasEffectiveAccessToAllHosts())
    return false;

  // Likewise, if the other permission set has full host access, then it must be
  // a privilege increase.
  if (permissions->HasEffectiveAccessToAllHosts())
    return true;

  const URLPatternSet& old_list = effective_hosts();
  const URLPatternSet& new_list = permissions->effective_hosts();

  // TODO(jstritar): This is overly conservative with respect to subdomains.
  // For example, going from *.google.com to www.google.com will be
  // considered an elevation, even though it is not (http://crbug.com/65337).
  std::set<std::string> new_hosts_set(GetDistinctHosts(new_list, false, false));
  std::set<std::string> old_hosts_set(GetDistinctHosts(old_list, false, false));
  std::set<std::string> new_hosts_only;

  std::set_difference(new_hosts_set.begin(), new_hosts_set.end(),
                      old_hosts_set.begin(), old_hosts_set.end(),
                      std::inserter(new_hosts_only, new_hosts_only.begin()));

  return !new_hosts_only.empty();
}

}  // namespace extensions
