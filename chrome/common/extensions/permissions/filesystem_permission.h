// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_FILESYSTEM_PERMISSION_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_FILESYSTEM_PERMISSION_H_

#include <set>
#include <string>

#include "chrome/common/extensions/permissions/api_permission.h"

namespace extensions {

class Extension;

// This file should be merged with others, see http://crbug.com/147531

// The default filesystem permission (no permission parameter) grants read
// access to files based on user action, i.e., through a file chooser.
// Specifically, the "read" permission may be specified in a manifest, but
// is always implicitly added here.

// <filesystem permissions>
//     :=  'write' | 'read' | <filesystem permissions>

class FileSystemPermission : public APIPermission {
 public:
  enum PermissionTypes {
    kNone = 0,
    kImplicitRead,
    kWrite,
  };

  struct CheckParam : APIPermission::CheckParam {
    explicit CheckParam(PermissionTypes permission) : permission(permission) {
    }
    PermissionTypes permission;
  };

  explicit FileSystemPermission(const APIPermissionInfo* info);

  virtual ~FileSystemPermission();

  // Returns true if the passed |extension| has implicit read filesystem
  // permission.
  static bool HasReadAccess(const Extension& extension);

  // Returns true if the passed |extension| has the write permission parameter.
  static bool HasWriteAccess(const Extension& extension);

  // Converts a string to a PermissionType. Return kNone for an unrecognized
  // input.
  static PermissionTypes PermissionStringToType(const std::string& str);

  // Converts a PermissionType to a string.
  static const char* PermissionTypeToString(PermissionTypes type);

  // Returns true if this permission has PermissionMessages.
  virtual bool HasMessages() const OVERRIDE;

  // Returns the localized permission messages of this permission.
  virtual PermissionMessages GetMessages() const OVERRIDE;

  // Returns true if the given permission in param is allowed.
  virtual bool Check(
      const APIPermission::CheckParam* param) const OVERRIDE;

  // Returns true if |rhs| is a subset of this.
  virtual bool Contains(const APIPermission* rhs) const OVERRIDE;

  // Returns true if |rhs| is equal to this.
  virtual bool Equal(const APIPermission* rhs) const OVERRIDE;

  // Parses the rhs from |value|. Returns false if error happens.
  virtual bool FromValue(const base::Value* value) OVERRIDE;

  // Stores this into a new created |value|.
  virtual void ToValue(base::Value** value) const OVERRIDE;

  // Clones this.
  virtual APIPermission* Clone() const OVERRIDE;

  // Returns a new API permission rhs which equals this - |rhs|.
  virtual APIPermission* Diff(const APIPermission* rhs) const OVERRIDE;

  // Returns a new API permission rhs which equals the union of this and
  // |rhs|.
  virtual APIPermission* Union(const APIPermission* rhs) const OVERRIDE;

  // Returns a new API permission rhs which equals the intersect of this and
  // |rhs|.
  virtual APIPermission* Intersect(const APIPermission* rhs) const OVERRIDE;

  // IPC functions
  // Writes this into the given IPC message |m|.
  virtual void Write(IPC::Message* m) const OVERRIDE;

  // Reads from the given IPC message |m|.
  virtual bool Read(const IPC::Message* m, PickleIterator* iter) OVERRIDE;

  // Logs this rhs.
  virtual void Log(std::string* log) const OVERRIDE;

 private:
  std::set<PermissionTypes> permissions_;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_FILESYSTEM_PERMISSION_H_
