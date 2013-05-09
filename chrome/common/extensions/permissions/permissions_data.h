// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_PERMISSIONS_DATA_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_PERMISSIONS_DATA_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"

namespace extensions {

class PermissionSet;
class APIPermissionSet;
class Extension;

// A container for the permissions data of the extension; also responsible for
// parsing the "permissions" and "optional_permissions" manifest keys.
// This class holds the permissions which were established in the extension's
// manifest; the runtime extensions of the extension (which may be different)
// are stored in Extension::RuntimeData.
class PermissionsData {
 public:
  PermissionsData();
  ~PermissionsData();

  // Parse the permissions of a given extension in the initialization process.
  bool ParsePermissions(Extension* extension, string16* error);

  // Finalize permissions after the initialization process completes.
  void FinalizePermissions(Extension* extension);

  // Return the optional or required permission set for the given |extension|.
  static const PermissionSet* GetOptionalPermissions(
      const Extension* extension);
  static const PermissionSet* GetRequiredPermissions(
      const Extension* extension);

  // Return the temporary API permission set which is used during extension
  // initialization. Once initialization completes, this is NULL.
  static const APIPermissionSet* GetInitialAPIPermissions(
      const Extension* extension);
  static APIPermissionSet* GetInitialAPIPermissions(Extension* extension);

 private:
  struct InitialPermissions;

  // Temporary permissions during the initialization process; NULL after
  // initialization completes.
  scoped_ptr<InitialPermissions> initial_required_permissions_;
  scoped_ptr<InitialPermissions> initial_optional_permissions_;

  // The set of permissions the extension can request at runtime.
  scoped_refptr<const PermissionSet> optional_permission_set_;

  // The extension's required / default set of permissions.
  scoped_refptr<const PermissionSet> required_permission_set_;

  DISALLOW_COPY_AND_ASSIGN(PermissionsData);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_PERMISSIONS_DATA_H_
