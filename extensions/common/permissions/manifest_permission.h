// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_MANIFEST_PERMISSION_H_
#define EXTENSIONS_COMMON_PERMISSIONS_MANIFEST_PERMISSION_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/pickle.h"
#include "extensions/common/permissions/permission_message.h"

class PickleIterator;

namespace base {
class Value;
class ListValue;
}

namespace IPC {
class Message;
}

namespace extensions {

// Represent the custom behavior of a top-level manifest entry contributing to
// permission messages and storage.
class ManifestPermission {
 public:
  ManifestPermission();
  virtual ~ManifestPermission();

  // The manifest key this permission applies to.
  virtual std::string name() const = 0;

  // Same as name(), needed for compatibility with APIPermission.
  virtual std::string id() const = 0;

  // Returns true if this permission has any PermissionMessages.
  virtual bool HasMessages() const = 0;

  // Returns the localized permission messages of this permission.
  virtual PermissionMessages GetMessages() const = 0;

  // Parses the ManifestPermission from |value|. Returns false if error happens.
  virtual bool FromValue(const base::Value* value) = 0;

  // Stores this into a new created Value.
  virtual scoped_ptr<base::Value> ToValue() const = 0;

  // Clones this.
  virtual ManifestPermission* Clone() const = 0;

  // Returns a new manifest permission which equals this - |rhs|.
  virtual ManifestPermission* Diff(const ManifestPermission* rhs) const = 0;

  // Returns a new manifest permission which equals the union of this and |rhs|.
  virtual ManifestPermission* Union(const ManifestPermission* rhs) const = 0;

  // Returns a new manifest permission which equals the intersect of this and
  // |rhs|.
  virtual ManifestPermission* Intersect(const ManifestPermission* rhs)
      const = 0;

  // Returns true if |rhs| is a subset of this.
  virtual bool Contains(const ManifestPermission* rhs) const = 0;

  // Returns true if |rhs| is equal to this.
  virtual bool Equal(const ManifestPermission* rhs) const = 0;

  // IPC functions
  // Writes this into the given IPC message |m|.
  virtual void Write(IPC::Message* m) const = 0;

  // Reads from the given IPC message |m|.
  virtual bool Read(const IPC::Message* m, PickleIterator* iter) = 0;

  // Logs this permission.
  virtual void Log(std::string* log) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ManifestPermission);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_MANIFEST_PERMISSION_H_
