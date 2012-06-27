// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/permissions_info.h"

namespace extensions {

// static
PermissionsInfo* PermissionsInfo::GetInstance() {
  return Singleton<PermissionsInfo>::get();
}

APIPermission* PermissionsInfo::GetByID(
    APIPermission::ID id) {
  IDMap::iterator i = id_map_.find(id);
  return (i == id_map_.end()) ? NULL : i->second;
}

APIPermission* PermissionsInfo::GetByName(
    const std::string& name) {
  NameMap::iterator i = name_map_.find(name);
  return (i == name_map_.end()) ? NULL : i->second;
}

APIPermissionSet PermissionsInfo::GetAll() {
  APIPermissionSet permissions;
  for (IDMap::const_iterator i = id_map_.begin(); i != id_map_.end(); ++i)
    permissions.insert(i->second->id());
  return permissions;
}

APIPermissionSet PermissionsInfo::GetAllByName(
    const std::set<std::string>& permission_names) {
  APIPermissionSet permissions;
  for (std::set<std::string>::const_iterator i = permission_names.begin();
       i != permission_names.end(); ++i) {
    APIPermission* permission = GetByName(*i);
    if (permission)
      permissions.insert(permission->id());
  }
  return permissions;
}

PermissionsInfo::~PermissionsInfo() {
  for (IDMap::iterator i = id_map_.begin(); i != id_map_.end(); ++i)
    delete i->second;
}

PermissionsInfo::PermissionsInfo()
    : hosted_app_permission_count_(0),
      permission_count_(0) {
  APIPermission::RegisterAllPermissions(this);
}

void PermissionsInfo::RegisterAlias(
    const char* name,
    const char* alias) {
  DCHECK(name_map_.find(name) != name_map_.end());
  DCHECK(name_map_.find(alias) == name_map_.end());
  name_map_[alias] = name_map_[name];
}

APIPermission* PermissionsInfo::RegisterPermission(
    APIPermission::ID id,
    const char* name,
    int l10n_message_id,
    PermissionMessage::ID message_id,
    int flags) {
  DCHECK(id_map_.find(id) == id_map_.end());
  DCHECK(name_map_.find(name) == name_map_.end());

  APIPermission* permission = new APIPermission(
      id, name, l10n_message_id, message_id, flags);

  id_map_[id] = permission;
  name_map_[name] = permission;

  permission_count_++;

  return permission;
}

}  // namespace extensions
