// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_SOCKET_PERMISSION_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_SOCKET_PERMISSION_H_

#include <set>
#include <string>

#include "chrome/common/extensions/permissions/api_permission.h"
#include "chrome/common/extensions/permissions/socket_permission_data.h"

namespace base {
class Value;
}

namespace IPC {
class Message;
}

namespace extensions {

class SocketPermission : public APIPermissionDetail {
 public:
  struct CheckParam : APIPermissionDetail::CheckParam {
    CheckParam(SocketPermissionData::OperationType type,
        const std::string& host,
        int port)
      : type(type),
        host(host),
        port(port) { }
    SocketPermissionData::OperationType type;
    std::string host;
    int port;
  };

  explicit SocketPermission(const APIPermission* permission);

  // Returns true if the given permission in param is allowed.
  virtual bool Check(
      const APIPermissionDetail::CheckParam* param) const OVERRIDE;

  // Returns true if |detail| is a subset of this.
  virtual bool Contains(const APIPermissionDetail* rhs) const OVERRIDE;

  // Returns true if |detail| is equal to this.
  virtual bool Equal(const APIPermissionDetail* rhs) const OVERRIDE;

  // Parses the detail from |value|. Returns false if error happens.
  virtual bool FromValue(const base::Value* value) OVERRIDE;

  // Stores this into a new created |value|.
  virtual void ToValue(base::Value** value) const OVERRIDE;

  // Clones this.
  virtual APIPermissionDetail* Clone() const OVERRIDE;

  // Returns a new API permission detail which equals this - |detail|.
  virtual APIPermissionDetail* Diff(
      const APIPermissionDetail* rhs) const OVERRIDE;

  // Returns a new API permission detail which equals the union of this and
  // |detail|.
  virtual APIPermissionDetail* Union(
      const APIPermissionDetail* rhs) const OVERRIDE;

  // Returns a new API permission detail which equals the intersect of this and
  // |detail|.
  virtual APIPermissionDetail* Intersect(
      const APIPermissionDetail* rhs) const OVERRIDE;

  // IPC functions
  // Writes this into the given IPC message |m|.
  virtual void Write(IPC::Message* m) const OVERRIDE;

  // Reads from the given IPC message |m|.
  virtual bool Read(const IPC::Message* m, PickleIterator* iter) OVERRIDE;

  // Logs this detail.
  virtual void Log(std::string* log) const OVERRIDE;

 protected:
  friend class base::RefCounted<APIPermissionDetail>;

  virtual ~SocketPermission();

 private:
  std::set<SocketPermissionData> data_set_;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_SOCKET_PERMISSION_H_
