// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/permission_set.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/url_pattern.h"
#include "url/gurl.h"

namespace extensions {

namespace {

void AddPatternsAndRemovePaths(const URLPatternSet& set, URLPatternSet* out) {
  DCHECK(out);
  for (URLPatternSet::const_iterator i = set.begin(); i != set.end(); ++i) {
    URLPattern p = *i;
    p.SetPath("/*");
    out->AddPattern(p);
  }
}

}  // namespace

//
// PermissionSet
//

PermissionSet::PermissionSet() : should_warn_all_hosts_(UNINITIALIZED) {}

PermissionSet::PermissionSet(
    const APIPermissionSet& apis,
    const ManifestPermissionSet& manifest_permissions,
    const URLPatternSet& explicit_hosts,
    const URLPatternSet& scriptable_hosts)
    : apis_(apis),
      manifest_permissions_(manifest_permissions),
      scriptable_hosts_(scriptable_hosts),
      should_warn_all_hosts_(UNINITIALIZED) {
  AddPatternsAndRemovePaths(explicit_hosts, &explicit_hosts_);
  InitImplicitPermissions();
  InitEffectiveHosts();
}

PermissionSet::~PermissionSet() {}

// static
std::unique_ptr<const PermissionSet> PermissionSet::CreateDifference(
    const PermissionSet& set1,
    const PermissionSet& set2) {
  APIPermissionSet apis;
  APIPermissionSet::Difference(set1.apis(), set2.apis(), &apis);

  ManifestPermissionSet manifest_permissions;
  ManifestPermissionSet::Difference(set1.manifest_permissions(),
                                    set2.manifest_permissions(),
                                    &manifest_permissions);

  URLPatternSet explicit_hosts = URLPatternSet::CreateDifference(
      set1.explicit_hosts(), set2.explicit_hosts());

  URLPatternSet scriptable_hosts = URLPatternSet::CreateDifference(
      set1.scriptable_hosts(), set2.scriptable_hosts());

  return base::WrapUnique(new PermissionSet(apis, manifest_permissions,
                                            explicit_hosts, scriptable_hosts));
}

// static
std::unique_ptr<const PermissionSet> PermissionSet::CreateIntersection(
    const PermissionSet& set1,
    const PermissionSet& set2) {
  APIPermissionSet apis;
  APIPermissionSet::Intersection(set1.apis(), set2.apis(), &apis);

  ManifestPermissionSet manifest_permissions;
  ManifestPermissionSet::Intersection(set1.manifest_permissions(),
                                      set2.manifest_permissions(),
                                      &manifest_permissions);

  URLPatternSet explicit_hosts = URLPatternSet::CreateSemanticIntersection(
      set1.explicit_hosts(), set2.explicit_hosts());
  URLPatternSet scriptable_hosts = URLPatternSet::CreateSemanticIntersection(
      set1.scriptable_hosts(), set2.scriptable_hosts());

  return base::WrapUnique(new PermissionSet(apis, manifest_permissions,
                                            explicit_hosts, scriptable_hosts));
}

// static
std::unique_ptr<const PermissionSet> PermissionSet::CreateUnion(
    const PermissionSet& set1,
    const PermissionSet& set2) {
  APIPermissionSet apis;
  APIPermissionSet::Union(set1.apis(), set2.apis(), &apis);

  ManifestPermissionSet manifest_permissions;
  ManifestPermissionSet::Union(set1.manifest_permissions(),
                               set2.manifest_permissions(),
                               &manifest_permissions);

  URLPatternSet explicit_hosts =
      URLPatternSet::CreateUnion(set1.explicit_hosts(), set2.explicit_hosts());

  URLPatternSet scriptable_hosts = URLPatternSet::CreateUnion(
      set1.scriptable_hosts(), set2.scriptable_hosts());

  return base::WrapUnique(new PermissionSet(apis, manifest_permissions,
                                            explicit_hosts, scriptable_hosts));
}

bool PermissionSet::operator==(
    const PermissionSet& rhs) const {
  return apis_ == rhs.apis_ &&
      manifest_permissions_ == rhs.manifest_permissions_ &&
      scriptable_hosts_ == rhs.scriptable_hosts_ &&
      explicit_hosts_ == rhs.explicit_hosts_;
}

bool PermissionSet::operator!=(const PermissionSet& rhs) const {
  return !(*this == rhs);
}

std::unique_ptr<const PermissionSet> PermissionSet::Clone() const {
  return base::WrapUnique(new PermissionSet(*this));
}

bool PermissionSet::Contains(const PermissionSet& set) const {
  return apis_.Contains(set.apis()) &&
         manifest_permissions_.Contains(set.manifest_permissions()) &&
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

bool PermissionSet::IsEmpty() const {
  // Not default if any host permissions are present.
  if (!(explicit_hosts().is_empty() && scriptable_hosts().is_empty()))
    return false;

  // Or if it has no api permissions.
  return apis().empty() && manifest_permissions().empty();
}

bool PermissionSet::HasAPIPermission(
    APIPermission::ID id) const {
  return apis().find(id) != apis().end();
}

bool PermissionSet::HasAPIPermission(const std::string& permission_name) const {
  const APIPermissionInfo* permission =
      PermissionsInfo::GetInstance()->GetByName(permission_name);
  // Ensure our PermissionsProvider is aware of this permission.
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
  if (effective_hosts().MatchesAllURLs())
    return true;

  for (APIPermissionSet::const_iterator i = apis().begin();
       i != apis().end(); ++i) {
    if (i->info()->implies_full_url_access())
      return true;
  }
  return false;
}

bool PermissionSet::ShouldWarnAllHosts() const {
  if (should_warn_all_hosts_ == UNINITIALIZED)
    InitShouldWarnAllHosts();
  return should_warn_all_hosts_ == WARN_ALL_HOSTS;
}

bool PermissionSet::HasEffectiveAccessToURL(const GURL& url) const {
  return effective_hosts().MatchesURL(url);
}

PermissionSet::PermissionSet(const PermissionSet& permissions)
    : apis_(permissions.apis_),
      manifest_permissions_(permissions.manifest_permissions_),
      explicit_hosts_(permissions.explicit_hosts_),
      scriptable_hosts_(permissions.scriptable_hosts_),
      effective_hosts_(permissions.effective_hosts_),
      should_warn_all_hosts_(permissions.should_warn_all_hosts_) {}

void PermissionSet::InitImplicitPermissions() {
  // The downloads permission implies the internal version as well.
  if (apis_.find(APIPermission::kDownloads) != apis_.end())
    apis_.insert(APIPermission::kDownloadsInternal);

  // The fileBrowserHandler permission implies the internal version as well.
  if (apis_.find(APIPermission::kFileBrowserHandler) != apis_.end())
    apis_.insert(APIPermission::kFileBrowserHandlerInternal);
}

void PermissionSet::InitEffectiveHosts() {
  effective_hosts_ =
      URLPatternSet::CreateUnion(explicit_hosts(), scriptable_hosts());
}

void PermissionSet::InitShouldWarnAllHosts() const {
  if (HasEffectiveAccessToAllHosts()) {
    should_warn_all_hosts_ = WARN_ALL_HOSTS;
    return;
  }

  for (URLPatternSet::const_iterator iter = effective_hosts_.begin();
       iter != effective_hosts_.end();
       ++iter) {
    if (iter->ImpliesAllHosts()) {
      should_warn_all_hosts_ = WARN_ALL_HOSTS;
      return;
    }
  }

  should_warn_all_hosts_ = DONT_WARN_ALL_HOSTS;
}

}  // namespace extensions
