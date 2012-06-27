// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_PERMISSIONS_INFO_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_PERMISSIONS_INFO_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/memory/singleton.h"
#include "chrome/common/extensions/permissions/api_permission.h"
#include "chrome/common/extensions/permissions/permission_message.h"

namespace extensions {

// Singleton that holds the extension permission instances and provides static
// methods for accessing them.
class PermissionsInfo {
 public:
  // Returns a pointer to the singleton instance.
  static PermissionsInfo* GetInstance();

  // Returns the permission with the given |id|, and NULL if it doesn't exist.
  APIPermission* GetByID(APIPermission::ID id);

  // Returns the permission with the given |name|, and NULL if none
  // exists.
  APIPermission* GetByName(const std::string& name);

  // Returns a set containing all valid api permission ids.
  APIPermissionSet GetAll();

  // Converts all the permission names in |permission_names| to permission ids.
  APIPermissionSet GetAllByName(
      const std::set<std::string>& permission_names);

  // Gets the total number of API permissions.
  size_t get_permission_count() { return permission_count_; }

 private:
  friend class APIPermission;

  ~PermissionsInfo();
  PermissionsInfo();

  // Registers an |alias| for a given permission |name|.
  void RegisterAlias(const char* name, const char* alias);

  // Registers a permission with the specified attributes and flags.
  APIPermission* RegisterPermission(
      APIPermission::ID id,
      const char* name,
      int l10n_message_id,
      PermissionMessage::ID message_id,
      int flags);

  // Maps permission ids to permissions.
  typedef std::map<APIPermission::ID, APIPermission*> IDMap;

  // Maps names and aliases to permissions.
  typedef std::map<std::string, APIPermission*> NameMap;

  IDMap id_map_;
  NameMap name_map_;

  size_t hosted_app_permission_count_;
  size_t permission_count_;

  friend struct DefaultSingletonTraits<PermissionsInfo>;
  DISALLOW_COPY_AND_ASSIGN(PermissionsInfo);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_PERMISSIONS_INFO_H_
