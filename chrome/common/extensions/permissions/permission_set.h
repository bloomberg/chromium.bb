// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_PERMISSION_SET_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_PERMISSION_SET_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/string16.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/common/extensions/permissions/api_permission.h"
#include "chrome/common/extensions/permissions/api_permission_set.h"
#include "chrome/common/extensions/permissions/permission_message.h"
#include "extensions/common/url_pattern_set.h"

namespace extensions {
class Extension;

// The PermissionSet is an immutable class that encapsulates an
// extension's permissions. The class exposes set operations for combining and
// manipulating the permissions.
class PermissionSet
    : public base::RefCountedThreadSafe<PermissionSet> {
 public:
  // Creates an empty permission set (e.g. default permissions).
  PermissionSet();

  // Creates a new permission set based on the |extension| manifest data, and
  // the api and host permissions (|apis| and |hosts|). The effective hosts
  // of the newly created permission set will be inferred from the |extension|
  // manifest, |apis| and |hosts|.
  PermissionSet(const extensions::Extension* extension,
                const APIPermissionSet& apis,
                const URLPatternSet& explicit_hosts);

  // Creates a new permission set based on the specified data.
  PermissionSet(const APIPermissionSet& apis,
                const URLPatternSet& explicit_hosts,
                const URLPatternSet& scriptable_hosts);

  // Creates a new permission set equal to |set1| - |set2|, passing ownership of
  // the new set to the caller.
  static PermissionSet* CreateDifference(
      const PermissionSet* set1, const PermissionSet* set2);

  // Creates a new permission set equal to the intersection of |set1| and
  // |set2|, passing ownership of the new set to the caller.
  static PermissionSet* CreateIntersection(
      const PermissionSet* set1, const PermissionSet* set2);

  // Creates a new permission set equal to the union of |set1| and |set2|.
  // Passes ownership of the new set to the caller.
  static PermissionSet* CreateUnion(
      const PermissionSet* set1, const PermissionSet* set2);

  // Creates a new permission set that only contains permissions that must be
  // in the manifest.  Passes ownership of the new set to the caller.
  static PermissionSet* ExcludeNotInManifestPermissions(
      const PermissionSet* set);

  bool operator==(const PermissionSet& rhs) const;

  // Returns true if every API or host permission available to |set| is also
  // available to this. In other words, if the API permissions of |set| are a
  // subset of this, and the host permissions in this encompass those in |set|.
  bool Contains(const PermissionSet& set) const;

  // Gets the API permissions in this set as a set of strings.
  std::set<std::string> GetAPIsAsStrings() const;

  // Returns whether this namespace has any functions which the extension has
  // permission to use.  For example, even though the extension may not have
  // the "tabs" permission, "tabs.create" requires no permissions so
  // HasAnyAccessToAPI("tabs") will return true.
  bool HasAnyAccessToAPI(const std::string& api_name) const;

  // Gets the localized permission messages that represent this set.
  // The set of permission messages shown varies by extension type.
  PermissionMessages GetPermissionMessages(Manifest::Type extension_type)
      const;

  // Gets the localized permission messages that represent this set (represented
  // as strings). The set of permission messages shown varies by extension type.
  std::vector<string16> GetWarningMessages(Manifest::Type extension_type)
      const;

  // Returns true if this is an empty set (e.g., the default permission set).
  bool IsEmpty() const;

  // Returns true if the set has the specified API permission.
  bool HasAPIPermission(APIPermission::ID permission) const;

  // Returns true if the set allows the given permission with the default
  // permission detal.
  bool CheckAPIPermission(APIPermission::ID permission) const;

  // Returns true if the set allows the given permission and permission param.
  bool CheckAPIPermissionWithParam(APIPermission::ID permission,
      const APIPermission::CheckParam* param) const;

  // Returns true if the permissions in this set grant access to the specified
  // |function_name|. The |allow_implicit| flag controls whether we
  // want to strictly check against just the explicit permissions, or also
  // include implicit "no permission needed" namespaces/functions.
  bool HasAccessToFunction(const std::string& function_name,
                           bool allow_implicit) const;

  // Returns true if this includes permission to access |origin|.
  bool HasExplicitAccessToOrigin(const GURL& origin) const;

  // Returns true if this permission set includes access to script |url|.
  bool HasScriptableAccessToURL(const GURL& url) const;

  // Returns true if this permission set includes effective access to all
  // origins.
  bool HasEffectiveAccessToAllHosts() const;

  // Returns true if this permission set includes effective access to |url|.
  bool HasEffectiveAccessToURL(const GURL& url) const;

  // Returns ture if this permission set effectively represents full access
  // (e.g. native code).
  bool HasEffectiveFullAccess() const;

  // Returns true if |permissions| has a greater privilege level than this
  // permission set (e.g., this permission set has less permissions).
  bool HasLessPrivilegesThan(const PermissionSet* permissions) const;

  const APIPermissionSet& apis() const { return apis_; }

  const URLPatternSet& effective_hosts() const { return effective_hosts_; }

  const URLPatternSet& explicit_hosts() const { return explicit_hosts_; }

  const URLPatternSet& scriptable_hosts() const { return scriptable_hosts_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(PermissionsTest, HasLessHostPrivilegesThan);
  FRIEND_TEST_ALL_PREFIXES(PermissionsTest, GetWarningMessages_AudioVideo);
  FRIEND_TEST_ALL_PREFIXES(PermissionsTest, GetDistinctHostsForDisplay);
  FRIEND_TEST_ALL_PREFIXES(PermissionsTest,
                           GetDistinctHostsForDisplay_ComIsBestRcd);
  FRIEND_TEST_ALL_PREFIXES(PermissionsTest,
                           GetDistinctHostsForDisplay_NetIs2ndBestRcd);
  FRIEND_TEST_ALL_PREFIXES(PermissionsTest,
                           GetDistinctHostsForDisplay_OrgIs3rdBestRcd);
  FRIEND_TEST_ALL_PREFIXES(PermissionsTest,
                           GetDistinctHostsForDisplay_FirstInListIs4thBestRcd);
  friend class base::RefCountedThreadSafe<PermissionSet>;

  ~PermissionSet();

  void AddAPIPermission(APIPermission::ID id);

  static std::set<std::string> GetDistinctHosts(
      const URLPatternSet& host_patterns,
      bool include_rcd,
      bool exclude_file_scheme);

  // Initializes the set based on |extension|'s manifest data.
  void InitImplicitExtensionPermissions(const extensions::Extension* extension);

  // Adds permissions implied independently of other context.
  void InitImplicitPermissions();

  // Initializes the effective host permission based on the data in this set.
  void InitEffectiveHosts();

  // Gets the permission messages for the API permissions.
  std::set<PermissionMessage> GetSimplePermissionMessages() const;

  // Returns true if |permissions| has an elevated API privilege level than
  // this set.
  bool HasLessAPIPrivilegesThan(
      const PermissionSet* permissions) const;

  // Returns true if |permissions| has more host permissions compared to this
  // set.
  bool HasLessHostPrivilegesThan(
      const PermissionSet* permissions) const;

  // Gets a list of the distinct hosts for displaying to the user.
  // NOTE: do not use this for comparing permissions, since this disgards some
  // information.
  std::set<std::string> GetDistinctHostsForDisplay() const;

  // The api list is used when deciding if an extension can access certain
  // extension APIs and features.
  APIPermissionSet apis_;

  // The list of hosts that can be accessed directly from the extension.
  // TODO(jstritar): Rename to "hosts_"?
  URLPatternSet explicit_hosts_;

  // The list of hosts that can be scripted by content scripts.
  // TODO(jstritar): Rename to "user_script_hosts_"?
  URLPatternSet scriptable_hosts_;

  // The list of hosts this effectively grants access to.
  URLPatternSet effective_hosts_;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_PERMISSION_SET_H_
