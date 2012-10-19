// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/permissions_info.h"

#include "base/logging.h"
#include "base/string_util.h"

namespace extensions {

// static
PermissionsInfo* PermissionsInfo::GetInstance() {
  return Singleton<PermissionsInfo>::get();
}

const APIPermissionInfo* PermissionsInfo::GetByID(
    APIPermission::ID id) const {
  IDMap::const_iterator i = id_map_.find(id);
  return (i == id_map_.end()) ? NULL : i->second;
}

const APIPermissionInfo* PermissionsInfo::GetByName(
    const std::string& name) const {
  NameMap::const_iterator i = name_map_.find(name);
  return (i == name_map_.end()) ? NULL : i->second;
}

APIPermissionSet PermissionsInfo::GetAll() const {
  APIPermissionSet permissions;
  for (IDMap::const_iterator i = id_map_.begin(); i != id_map_.end(); ++i)
    permissions.insert(i->second->id());
  return permissions;
}

APIPermissionSet PermissionsInfo::GetAllByName(
    const std::set<std::string>& permission_names) const {
  APIPermissionSet permissions;
  for (std::set<std::string>::const_iterator i = permission_names.begin();
       i != permission_names.end(); ++i) {
    const APIPermissionInfo* permission_info = GetByName(*i);
    if (permission_info)
      permissions.insert(permission_info->id());
  }
  return permissions;
}

bool PermissionsInfo::HasChildPermissions(const std::string& name) const {
  NameMap::const_iterator i = name_map_.lower_bound(name + '.');
  if (i == name_map_.end()) return false;
  return StartsWithASCII(i->first, name + '.', true);
}

PermissionsInfo::~PermissionsInfo() {
  for (IDMap::iterator i = id_map_.begin(); i != id_map_.end(); ++i)
    delete i->second;
}

PermissionsInfo::PermissionsInfo()
    : hosted_app_permission_count_(0),
      permission_count_(0) {
  APIPermissionInfo::RegisterAllPermissions(this);
}

void PermissionsInfo::RegisterAlias(
    const char* name,
    const char* alias) {
  DCHECK(name_map_.find(name) != name_map_.end());
  DCHECK(name_map_.find(alias) == name_map_.end());
  name_map_[alias] = name_map_[name];
}

const APIPermissionInfo* PermissionsInfo::RegisterPermission(
    APIPermission::ID id,
    const char* name,
    int l10n_message_id,
    PermissionMessage::ID message_id,
    int flags,
    const APIPermissionInfo::APIPermissionConstructor constructor) {
  DCHECK(id_map_.find(id) == id_map_.end());
  DCHECK(name_map_.find(name) == name_map_.end());

  APIPermissionInfo* permission = new APIPermissionInfo(
      id, name, l10n_message_id, message_id, flags, constructor);

  id_map_[id] = permission;
  name_map_[name] = permission;

  permission_count_++;

  return permission;
}

}  // namespace extensions
