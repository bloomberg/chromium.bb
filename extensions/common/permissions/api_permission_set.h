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

// A set of permissions for an app or extension. Used for passing around groups
// of permissions, such as required or optional permissions. Has convenience
// constructors so that it can be constructed inline.
//
// Each permission can also store a string, such as a hostname or device number,
// as a parameter that helps identify the permission. This parameter can then
// be used when the permission message is generated. For example, the permission
// kHostReadOnly might have the parameter "google.com", which means that the app
// or extension has the permission to read the host google.com. This parameter
// may then be included in the permission message when it is generated later.
//
// Example:
//   // Create a PermissionIDSet.
//   PermissionIDSet p(APIPermission::kBluetooth, APIPermission::kFavicon);
//   // Add a permission to the set.
//   p.insertPermission(APIPermission::kNetworkState);
//   // Add a permission with a detail to the set.
//   p.insertPermission(APIPermission::kHostReadOnly,
//           base::ASCIIToUTF16("http://www.google.com"));
//
// TODO(sashab): Move this to its own file and rename it to PermissionSet after
// APIPermission is removed, the current PermissionSet is no longer used, and
// APIPermission::ID is the only type of Permission ID.
typedef std::pair<APIPermission::ID, base::string16> PermissionID;
class PermissionIDSet {
 public:
  PermissionIDSet();
  virtual ~PermissionIDSet();

  // Convenience constructors for inline initialization.
  PermissionIDSet(APIPermission::ID permission_one);
  PermissionIDSet(APIPermission::ID permission_one,
                  APIPermission::ID permission_two);
  PermissionIDSet(APIPermission::ID permission_one,
                  APIPermission::ID permission_two,
                  APIPermission::ID permission_three);
  PermissionIDSet(APIPermission::ID permission_one,
                  APIPermission::ID permission_two,
                  APIPermission::ID permission_three,
                  APIPermission::ID permission_four);

  // Adds the given permission, and an optional permission detail, to the set.
  void insert(APIPermission::ID permission);
  void insert(APIPermission::ID permission, base::string16 permission_detail);

 private:
  std::set<PermissionID> permissions;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_API_PERMISSION_SET_H_
