// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_SOCKET_PERMISSION_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_SOCKET_PERMISSION_H_

#include <string>

#include "chrome/common/extensions/permissions/api_permission.h"
#include "chrome/common/extensions/permissions/set_disjunction_permission.h"
#include "chrome/common/extensions/permissions/socket_permission_data.h"

namespace extensions {

class SocketPermission : public SetDisjunctionPermission<SocketPermissionData,
                                                         SocketPermission> {
 public:
  struct CheckParam : APIPermission::CheckParam {
    CheckParam(content::SocketPermissionRequest::OperationType type,
        const std::string& host,
        int port)
      : request(type, host, port) { }
    content::SocketPermissionRequest request;
  };

  explicit SocketPermission(const APIPermissionInfo* info);

  virtual ~SocketPermission();

  // Returns the localized permission messages of this permission.
  virtual PermissionMessages GetMessages() const OVERRIDE;

 private:
  bool AddAnyHostMessage(PermissionMessages& messages) const;
  void AddSubdomainHostMessage(PermissionMessages& messages) const;
  void AddSpecificHostMessage(PermissionMessages& messages) const;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_SOCKET_PERMISSION_H_
