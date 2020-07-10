// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_PERMISSION_SET_H_
#define EXTENSIONS_COMMON_PERMISSIONS_PERMISSION_SET_H_

#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/manifest_permission.h"
#include "extensions/common/permissions/manifest_permission_set.h"
#include "extensions/common/url_pattern_set.h"

namespace extensions {

// The PermissionSet is an immutable class that encapsulates an
// extension's permissions. The class exposes set operations for combining and
// manipulating the permissions.
// TODO(sashab): PermissionIDSet should be called PermissionSet. Once
// PermissionMessageProvider::GetPermissionMessages() is the only
// method used for generating permission messages, find the other users of this
// class and deprecate or rename it as appropriate.
class PermissionSet {
 public:
  // Creates an empty permission set (e.g. default permissions).
  PermissionSet();

  // Creates a new permission set based on the specified data: the API
  // permissions, manifest key permissions, host permissions, and scriptable
  // hosts. The effective hosts of the newly created permission set will be
  // inferred from the given host permissions.
  PermissionSet(APIPermissionSet apis,
                ManifestPermissionSet manifest_permissions,
                URLPatternSet explicit_hosts,
                URLPatternSet scriptable_hosts);
  ~PermissionSet();

  // Creates a new permission set equal to |set1| - |set2|.
  static std::unique_ptr<const PermissionSet> CreateDifference(
      const PermissionSet& set1,
      const PermissionSet& set2);

  // Creates a new permission set equal to the intersection of |set1| and
  // |set2|.
  // TODO(https://crbug.com/867549): Audit callers of CreateIntersection() and
  // have them determine the proper intersection behavior.
  static std::unique_ptr<const PermissionSet> CreateIntersection(
      const PermissionSet& set1,
      const PermissionSet& set2,
      URLPatternSet::IntersectionBehavior intersection_behavior =
          URLPatternSet::IntersectionBehavior::kPatternsContainedByBoth);

  // Creates a new permission set equal to the union of |set1| and |set2|.
  static std::unique_ptr<const PermissionSet> CreateUnion(
      const PermissionSet& set1,
      const PermissionSet& set2);

  bool operator==(const PermissionSet& rhs) const;
  bool operator!=(const PermissionSet& rhs) const;

  // Returns a copy of this PermissionSet.
  std::unique_ptr<const PermissionSet> Clone() const;

  // Returns true if every API or host permission available to |set| is also
  // available to this. In other words, if the API permissions of |set| are a
  // subset of this, and the host permissions in this encompass those in |set|.
  bool Contains(const PermissionSet& set) const;

  // Gets the API permissions in this set as a set of strings.
  std::set<std::string> GetAPIsAsStrings() const;

  // Returns true if this is an empty set (e.g., the default permission set).
  bool IsEmpty() const;

  // Returns true if the set has the specified API permission.
  bool HasAPIPermission(APIPermission::ID permission) const;

  // Returns true if the |extension| explicitly requests access to the given
  // |permission_name|. Note this does not include APIs without no corresponding
  // permission, like "runtime" or "browserAction".
  bool HasAPIPermission(const std::string& permission_name) const;

  // Returns true if the set allows the given permission with the default
  // permission detal.
  bool CheckAPIPermission(APIPermission::ID permission) const;

  // Returns true if the set allows the given permission and permission param.
  bool CheckAPIPermissionWithParam(APIPermission::ID permission,
      const APIPermission::CheckParam* param) const;

  // Returns true if this includes permission to access |origin|.
  bool HasExplicitAccessToOrigin(const GURL& origin) const;

  // Returns true if this permission set includes effective access to all
  // origins.
  bool HasEffectiveAccessToAllHosts() const;

  // Returns true if this permission set has access to so many hosts, that we
  // should treat it as all hosts for warning purposes.
  // For example, '*://*.com/*'.
  // If |include_api_permissions| is true, this will look at both host
  // permissions and API permissions. Otherwise, this only looks at
  // host permissions.
  bool ShouldWarnAllHosts(bool include_api_permissions = true) const;

  // Returns true if this permission set includes effective access to |url|.
  bool HasEffectiveAccessToURL(const GURL& url) const;

  const APIPermissionSet& apis() const { return apis_; }

  const ManifestPermissionSet& manifest_permissions() const {
      return manifest_permissions_;
  }

  const URLPatternSet& effective_hosts() const { return effective_hosts_; }

  const URLPatternSet& explicit_hosts() const { return explicit_hosts_; }

  const URLPatternSet& scriptable_hosts() const { return scriptable_hosts_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(PermissionsTest, GetWarningMessages_AudioVideo);
  FRIEND_TEST_ALL_PREFIXES(PermissionsTest, AccessToDevicesMessages);

  // Deliberate copy constructor for cloning the set.
  PermissionSet(const PermissionSet& permission_set);

  // Adds permissions implied independently of other context.
  void InitImplicitPermissions();

  // Initializes the effective host permission based on the data in this set.
  void InitEffectiveHosts();

  // Initializes whether we should present the user with the "all hosts" warning
  // for either the included host permissions or API permissions.
  void InitShouldWarnAllHostsForHostPermissions() const;
  void InitShouldWarnAllHostsForAPIPermissions() const;

  // The api list is used when deciding if an extension can access certain
  // extension APIs and features.
  APIPermissionSet apis_;

  // The manifest key permission list is used when deciding if an extension
  // can access certain extension APIs and features.
  ManifestPermissionSet manifest_permissions_;

  // The list of hosts that can be accessed directly from the extension.
  // TODO(jstritar): Rename to "hosts_"?
  URLPatternSet explicit_hosts_;

  // The list of hosts that can be scripted by content scripts.
  // TODO(jstritar): Rename to "user_script_hosts_"?
  URLPatternSet scriptable_hosts_;

  // The list of hosts this effectively grants access to.
  URLPatternSet effective_hosts_;

  enum ShouldWarnAllHostsType {
    UNINITIALIZED = 0,
    WARN_ALL_HOSTS,
    DONT_WARN_ALL_HOSTS
  };
  // Cache whether this set implies access to all hosts, because it's
  // non-trivial to compute (lazily initialized).
  mutable ShouldWarnAllHostsType host_permissions_should_warn_all_hosts_ =
      UNINITIALIZED;
  mutable ShouldWarnAllHostsType api_permissions_should_warn_all_hosts_ =
      UNINITIALIZED;

  DISALLOW_ASSIGN(PermissionSet);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_PERMISSION_SET_H_
