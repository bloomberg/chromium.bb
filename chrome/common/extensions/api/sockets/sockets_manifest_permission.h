// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_SOCKETS_SOCKETS_MANIFEST_PERMISSION_H_
#define CHROME_COMMON_EXTENSIONS_API_SOCKETS_SOCKETS_MANIFEST_PERMISSION_H_

#include <set>
#include <vector>

#include "chrome/common/extensions/permissions/socket_permission_entry.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/permissions/manifest_permission.h"

namespace content {
struct SocketPermissionRequest;
}

namespace extensions {
class Extension;
}

namespace extensions {

class SocketsManifestPermission : public ManifestPermission {
 public:
  typedef std::set<SocketPermissionEntry> SocketPermissionEntrySet;
  enum PermissionKind {
    kNone = 0,
    kTcpPermission = 1 << 0,
    kUdpPermission = 1 << 1,
    kTcpServerPermission = 1 << 2
  };

  SocketsManifestPermission();
  virtual ~SocketsManifestPermission();

  // Tries to construct the info based on |value|, as it would have appeared in
  // the manifest. Sets |error| and returns an empty scoped_ptr on failure.
  static scoped_ptr<SocketsManifestPermission> FromValue(
      const base::Value& value,
      string16* error);

  bool CheckRequest(const Extension* extension,
                    const content::SocketPermissionRequest& request) const;

  // extensions::ManifestPermission overrides.
  virtual std::string name() const OVERRIDE;
  virtual std::string id() const OVERRIDE;
  virtual bool HasMessages() const OVERRIDE;
  virtual PermissionMessages GetMessages() const OVERRIDE;
  virtual bool FromValue(const base::Value* value) OVERRIDE;
  virtual scoped_ptr<base::Value> ToValue() const OVERRIDE;
  virtual ManifestPermission* Clone() const OVERRIDE;
  virtual ManifestPermission* Diff(const ManifestPermission* rhs) const
      OVERRIDE;
  virtual ManifestPermission* Union(const ManifestPermission* rhs) const
      OVERRIDE;
  virtual ManifestPermission* Intersect(const ManifestPermission* rhs) const
      OVERRIDE;
  virtual bool Contains(const ManifestPermission* rhs) const OVERRIDE;
  virtual bool Equal(const ManifestPermission* rhs) const OVERRIDE;
  virtual void Write(IPC::Message* m) const OVERRIDE;
  virtual bool Read(const IPC::Message* m, PickleIterator* iter) OVERRIDE;
  virtual void Log(std::string* log) const OVERRIDE;

  bool has_udp() const { return (kinds_ & kUdpPermission) != 0; }
  bool has_tcp() const { return (kinds_ & kTcpPermission) != 0; }
  bool has_tcp_server() const { return (kinds_ & kTcpServerPermission) != 0; }
  const SocketPermissionEntrySet& entries() const { return permissions_; }

 private:
  static bool ParseHostPattern(
      SocketsManifestPermission* manifest_data,
      content::SocketPermissionRequest::OperationType operation_type,
      const scoped_ptr<std::string>& value,
      string16* error);

  static PermissionKind HasOperationType(
      const SocketPermissionEntrySet& set,
      content::SocketPermissionRequest::OperationType operation_type,
      PermissionKind kind);

  scoped_ptr<std::string> CreateHostPattern(
    content::SocketPermissionRequest::OperationType operation_type) const;

  void AddPermission(const SocketPermissionEntry& entry);

  bool AddAnyHostMessage(PermissionMessages& messages) const;
  void AddSubdomainHostMessage(PermissionMessages& messages) const;
  void AddSpecificHostMessage(PermissionMessages& messages) const;
  void AddNetworkListMessage(PermissionMessages& messages) const;

  SocketPermissionEntrySet permissions_;
  int kinds_;  // PermissionKind bits
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_SOCKETS_SOCKETS_MANIFEST_PERMISSION_H_
