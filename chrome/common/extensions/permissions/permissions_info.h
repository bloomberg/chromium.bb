// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_PERMISSIONS_INFO_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_PERMISSIONS_INFO_H_

#include <map>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/lazy_instance.h"
#include "chrome/common/extensions/permissions/api_permission.h"
#include "chrome/common/extensions/permissions/api_permission_set.h"
#include "chrome/common/extensions/permissions/permission_message.h"

namespace extensions {

class ChromeAPIPermissions;

// A global object that holds the extension permission instances and provides
// methods for accessing them.
class PermissionsInfo {
 public:
  // An alias for a given permission |name|.
  struct AliasInfo {
    const char* name;
    const char* alias;

    AliasInfo(const char* name, const char* alias)
        : name(name), alias(alias) {
    }
  };

  // The delegate creates the APIPermissions instances. It is only
  // needed at startup time.
  class Delegate {
   public:
    // Returns all the known permissions. The caller, PermissionsInfo,
    // takes ownership of the APIPermissionInfos.
    virtual std::vector<APIPermissionInfo*> GetAllPermissions() const = 0;

    // Returns all the known permission aliases.
    virtual std::vector<AliasInfo> GetAllAliases() const = 0;
  };

  static PermissionsInfo* GetInstance();

  virtual ~PermissionsInfo();

  // Initializes the permissions from the delegate.
  void InitializeWithDelegate(const Delegate& delegate);

  // Returns the permission with the given |id|, and NULL if it doesn't exist.
  const APIPermissionInfo* GetByID(APIPermission::ID id) const;

  // Returns the permission with the given |name|, and NULL if none
  // exists.
  const APIPermissionInfo* GetByName(const std::string& name) const;

  // Returns a set containing all valid api permission ids.
  APIPermissionSet GetAll() const;

  // Converts all the permission names in |permission_names| to permission ids.
  APIPermissionSet GetAllByName(
      const std::set<std::string>& permission_names) const;

  // Checks if any permissions have names that start with |name| followed by a
  // period.
  bool HasChildPermissions(const std::string& name) const;

  // Gets the total number of API permissions.
  size_t get_permission_count() const { return permission_count_; }

 private:
  friend class ScopedTestingPermissionsInfo;
  friend struct base::DefaultLazyInstanceTraits<PermissionsInfo>;

  PermissionsInfo();

  // Overrides the global PermissionsInfo for unit tests. Should only be
  // called from ScopedTestingPermissionsInfo.
  static void SetForTesting(PermissionsInfo* info);

  // Registers an |alias| for a given permission |name|.
  void RegisterAlias(const char* name, const char* alias);

  // Registers a permission with the specified attributes and flags.
  void RegisterPermission(APIPermissionInfo* permission);

  // Maps permission ids to permissions.
  typedef std::map<APIPermission::ID, APIPermissionInfo*> IDMap;

  // Maps names and aliases to permissions.
  typedef std::map<std::string, APIPermissionInfo*> NameMap;

  IDMap id_map_;
  NameMap name_map_;

  size_t hosted_app_permission_count_;
  size_t permission_count_;

  // Set to true after the delegate has created the known permissions.
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(PermissionsInfo);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_PERMISSIONS_INFO_H_
