// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_MEDIA_GALLERIES_PERMISSION_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_MEDIA_GALLERIES_PERMISSION_H_

#include <set>
#include <string>

#include "chrome/common/extensions/permissions/api_permission.h"

namespace base {
class Value;
}

namespace IPC {
class Message;
}

namespace extensions {

class Extension;

// There's room to share code with related classes, see http://crbug.com/147531

// There are two kinds of media galleries permissions, location permissions
// and access type permissions.
//
// The default location permission (no permission parameter) does not grant
// access to any galleries, but lets the user grant access on a per gallery
// basis.  The other location permission "all-auto-detected" gives the user
// access to all auto detected galleries.  This includes per platform default
// galleries and removable devices that are detected as media devices.
//
// Access type will include several different types in the future, but right
// now we only have one access type, "read."  Future access types may include
// "write," "delete," and "add-file."
//
// <media galleries permissions>
//     :=  'all-auto-detected' | 'read' | <media galleries permissions>

class MediaGalleriesPermission : public APIPermission {
 public:
  enum PermissionTypes {
    kNone = 0,
    kAllAutoDetected,
    kRead,
  };

  struct CheckParam : APIPermission::CheckParam {
    explicit CheckParam(PermissionTypes permission) : permission(permission) {
    }
    PermissionTypes permission;
  };

  explicit MediaGalleriesPermission(const APIPermissionInfo* info);

  virtual ~MediaGalleriesPermission();

  // Returns true if the passed |extension| has the read permission parameter.
  static bool HasReadAccess(const Extension& extension);

  // Returns true if the passed |extension| has the all galleries permission
  // parameter.
  static bool HasAllGalleriesAccess(const Extension& extension);

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

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_MEDIA_GALLERIES_PERMISSION_H_
