// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/api_permission_set.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permissions_info.h"

namespace extensions {

namespace errors = manifest_errors;

namespace {

bool CreateAPIPermission(
    const std::string& permission_str,
    const base::Value* permission_value,
    APIPermissionSet::ParseSource source,
    APIPermissionSet* api_permissions,
    base::string16* error,
    std::vector<std::string>* unhandled_permissions) {

  const APIPermissionInfo* permission_info =
      PermissionsInfo::GetInstance()->GetByName(permission_str);
  if (permission_info) {
    scoped_ptr<APIPermission> permission(
        permission_info->CreateAPIPermission());
    if (source != APIPermissionSet::kAllowInternalPermissions &&
        permission_info->is_internal()) {
      // An internal permission specified in permissions list is an error.
      if (error) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kPermissionNotAllowedInManifest, permission_str);
      }
      return false;
    }

    std::string error_details;
    if (!permission->FromValue(permission_value, &error_details,
                               unhandled_permissions)) {
      if (error) {
        if (error_details.empty()) {
          *error = ErrorUtils::FormatErrorMessageUTF16(
              errors::kInvalidPermission,
              permission_info->name());
        } else {
          *error = ErrorUtils::FormatErrorMessageUTF16(
              errors::kInvalidPermissionWithDetail,
              permission_info->name(),
              error_details);
        }
        return false;
      }
      LOG(WARNING) << "Parse permission failed.";
    } else {
      api_permissions->insert(permission.release());
    }
    return true;
  }

  if (unhandled_permissions)
    unhandled_permissions->push_back(permission_str);
  else
    LOG(WARNING) << "Unknown permission[" << permission_str << "].";

  return true;
}

bool ParseChildPermissions(const std::string& base_name,
                           const base::Value* permission_value,
                           APIPermissionSet::ParseSource source,
                           APIPermissionSet* api_permissions,
                           base::string16* error,
                           std::vector<std::string>* unhandled_permissions) {
  if (permission_value) {
    const base::ListValue* permissions;
    if (!permission_value->GetAsList(&permissions)) {
      if (error) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidPermission, base_name);
        return false;
      }
      LOG(WARNING) << "Permission value is not a list.";
      // Failed to parse, but since error is NULL, failures are not fatal so
      // return true here anyway.
      return true;
    }

    for (size_t i = 0; i < permissions->GetSize(); ++i) {
      std::string permission_str;
      if (!permissions->GetString(i, &permission_str)) {
        // permission should be a string
        if (error) {
          *error = ErrorUtils::FormatErrorMessageUTF16(
              errors::kInvalidPermission,
              base_name + '.' + base::IntToString(i));
          return false;
        }
        LOG(WARNING) << "Permission is not a string.";
        continue;
      }

      if (!CreateAPIPermission(
              base_name + '.' + permission_str, NULL, source,
              api_permissions, error, unhandled_permissions))
        return false;
    }
  }

  return CreateAPIPermission(base_name, NULL, source,
                             api_permissions, error, NULL);
}

}  // namespace

void APIPermissionSet::insert(APIPermission::ID id) {
  const APIPermissionInfo* permission_info =
      PermissionsInfo::GetInstance()->GetByID(id);
  DCHECK(permission_info);
  insert(permission_info->CreateAPIPermission());
}

void APIPermissionSet::insert(APIPermission* permission) {
  BaseSetOperators<APIPermissionSet>::insert(permission);
}

// static
bool APIPermissionSet::ParseFromJSON(
    const base::ListValue* permissions,
    APIPermissionSet::ParseSource source,
    APIPermissionSet* api_permissions,
    base::string16* error,
    std::vector<std::string>* unhandled_permissions) {
  for (size_t i = 0; i < permissions->GetSize(); ++i) {
    std::string permission_str;
    const base::Value* permission_value = NULL;
    if (!permissions->GetString(i, &permission_str)) {
      const base::DictionaryValue* dict = NULL;
      // permission should be a string or a single key dict.
      if (!permissions->GetDictionary(i, &dict) || dict->size() != 1) {
        if (error) {
          *error = ErrorUtils::FormatErrorMessageUTF16(
              errors::kInvalidPermission, base::IntToString(i));
          return false;
        }
        LOG(WARNING) << "Permission is not a string or single key dict.";
        continue;
      }
      base::DictionaryValue::Iterator it(*dict);
      permission_str = it.key();
      permission_value = &it.value();
    }

    // Check if this permission is a special case where its value should
    // be treated as a list of child permissions.
    if (PermissionsInfo::GetInstance()->HasChildPermissions(permission_str)) {
      if (!ParseChildPermissions(permission_str, permission_value, source,
                                 api_permissions, error, unhandled_permissions))
        return false;
      continue;
    }

    if (!CreateAPIPermission(permission_str, permission_value, source,
                             api_permissions, error, unhandled_permissions))
      return false;
  }
  return true;
}

void APIPermissionSet::AddImpliedPermissions() {
  // The fileSystem.write and fileSystem.directory permissions imply
  // fileSystem.writeDirectory.
  // Has a corresponding rule in ChromePermissionMessageProvider.
  // TODO(sammc): Remove this. See http://crbug.com/284849.
  if (ContainsKey(map(), APIPermission::kFileSystemWrite) &&
      ContainsKey(map(), APIPermission::kFileSystemDirectory)) {
    insert(APIPermission::kFileSystemWriteDirectory);
  }
}

PermissionID::PermissionID(APIPermission::ID id)
    : std::pair<APIPermission::ID, base::string16>(id, base::string16()) {
}

PermissionID::PermissionID(APIPermission::ID id,
                           const base::string16& parameter)
    : std::pair<APIPermission::ID, base::string16>(id, parameter) {
}

PermissionID::~PermissionID() {
}

PermissionIDSet::PermissionIDSet() : permissions_() {
}

PermissionIDSet::~PermissionIDSet() {
}

void PermissionIDSet::insert(APIPermission::ID permission_id) {
  permissions_.insert(PermissionID(permission_id, base::string16()));
}

void PermissionIDSet::insert(APIPermission::ID permission_id,
                             base::string16 permission_detail) {
  permissions_.insert(PermissionID(permission_id, permission_detail));
}

void PermissionIDSet::InsertAll(const PermissionIDSet& permission_set) {
  for (const auto& permission : permission_set.permissions_) {
    permissions_.insert(permission);
  }
}

std::vector<base::string16> PermissionIDSet::GetAllPermissionParameters()
    const {
  std::vector<base::string16> details;
  for (const auto& permission : permissions_) {
    details.push_back(permission.parameter());
  }
  return details;
}

bool PermissionIDSet::ContainsAllIDs(
    std::set<APIPermission::ID> permission_ids) {
  // TODO(sashab): Find a more efficient way to implement this (e.g. store a set
  // of IDs and use STLIncludes in base/stl_util.h).
  PermissionIDSet subset;
  for (const auto& permission_id : permission_ids) {
    if (!ContainsID(permission_id)) {
      return false;
    }
  }
  return true;
}

PermissionIDSet PermissionIDSet::GetAllPermissionsWithIDs(
    const std::set<APIPermission::ID>& permission_ids) const {
  PermissionIDSet subset;
  for (const auto& permission : permissions_) {
    if (ContainsKey(permission_ids, permission.id())) {
      subset.permissions_.insert(permission);
    }
  }
  return subset;
}

bool PermissionIDSet::Includes(const PermissionIDSet& subset) const {
  return base::STLIncludes<std::set<PermissionID>>(permissions_,
                                                   subset.permissions_);
}

// static
PermissionIDSet PermissionIDSet::Difference(const PermissionIDSet& set_1,
                                            const PermissionIDSet& set_2) {
  return PermissionIDSet(base::STLSetDifference<std::set<PermissionID>>(
      set_1.permissions_, set_2.permissions_));
}

// static
PermissionIDSet PermissionIDSet::Intersection(const PermissionIDSet& set_1,
                                              const PermissionIDSet& set_2) {
  return PermissionIDSet(base::STLSetIntersection<std::set<PermissionID>>(
      set_1.permissions_, set_2.permissions_));
}

// static
PermissionIDSet PermissionIDSet::Union(const PermissionIDSet& set_1,
                                       const PermissionIDSet& set_2) {
  return PermissionIDSet(base::STLSetUnion<std::set<PermissionID>>(
      set_1.permissions_, set_2.permissions_));
}

size_t PermissionIDSet::size() const {
  return permissions_.size();
}

bool PermissionIDSet::empty() const {
  return permissions_.empty();
}

PermissionIDSet::PermissionIDSet(std::set<PermissionID> permissions)
    : permissions_(permissions) {
}

bool PermissionIDSet::ContainsID(APIPermission::ID permission_id) {
  // TODO(sashab): Find a more efficient way to implement this.
  PermissionIDSet subset;
  for (const auto& permission : permissions_) {
    if (permission.id() == permission_id) {
      return true;
    }
  }
  return false;
}

}  // namespace extensions
