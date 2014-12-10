// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_API_PERMISSION_SET_H_
#define EXTENSIONS_COMMON_PERMISSIONS_API_PERMISSION_SET_H_


#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/base_set_operators.h"

namespace base {
class ListValue;
}  // namespace base

namespace extensions {

class Extension;
class APIPermissionSet;

template<>
struct BaseSetOperatorsTraits<APIPermissionSet> {
  typedef APIPermission ElementType;
  typedef APIPermission::ID ElementIDType;
};

class APIPermissionSet : public BaseSetOperators<APIPermissionSet> {
 public:
  enum ParseSource {
    // Don't allow internal permissions to be parsed (e.g. entries in the
    // "permissions" list in a manifest).
    kDisallowInternalPermissions,

    // Allow internal permissions to be parsed (e.g. from the "api" field of a
    // permissions list in the prefs).
    kAllowInternalPermissions,
  };

  void insert(APIPermission::ID id);

  // Insert |permission| into the APIPermissionSet. The APIPermissionSet will
  // take the ownership of |permission|,
  void insert(APIPermission* permission);

  // Parses permissions from |permissions| and adds the parsed permissions to
  // |api_permissions|. If |source| is kDisallowInternalPermissions, treat
  // permissions with kFlagInternal as errors. If |unhandled_permissions| is
  // not NULL, the names of all permissions that couldn't be parsed will be
  // added to this vector. If |error| is NULL, parsing will continue with the
  // next permission if invalid data is detected. If |error| is not NULL, it
  // will be set to an error message and false is returned when an invalid
  // permission is found.
  static bool ParseFromJSON(
      const base::ListValue* permissions,
      ParseSource source,
      APIPermissionSet* api_permissions,
      base::string16* error,
      std::vector<std::string>* unhandled_permissions);

  void AddImpliedPermissions();
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_API_PERMISSION_SET_H_
