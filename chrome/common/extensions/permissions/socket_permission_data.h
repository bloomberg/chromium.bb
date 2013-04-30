// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_SOCKET_PERMISSION_DATA_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_SOCKET_PERMISSION_DATA_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/common/extensions/permissions/api_permission.h"
#include "content/public/common/socket_permission_request.h"

namespace extensions {

// A pattern that can be used to match socket permission.
//   <socket-permission-pattern>
//          := <op> |
//             <op> ':' <host> |
//             <op> ':' ':' <port> |
//             <op> ':' <host> ':' <port> |
//             'udp-multicast-membership'
//   <op>   := 'tcp-connect' |
//             'tcp-listen' |
//             'udp-bind' |
//             'udp-send-to'
//   <host> := '*' |
//             '*.' <anychar except '/' and '*'>+ |
//             <anychar except '/' and '*'>+
//   <port> := '*' |
//             <port number between 0 and 65535>)
// The multicast membership permission implies a permission to any address.
class SocketPermissionData {
 public:
  enum HostType {
    ANY_HOST,
    HOSTS_IN_DOMAINS,
    SPECIFIC_HOSTS,
  };

  SocketPermissionData();
  ~SocketPermissionData();

  // operators <, == are needed by container std::set and algorithms
  // std::set_includes and std::set_differences.
  bool operator<(const SocketPermissionData& rhs) const;
  bool operator==(const SocketPermissionData& rhs) const;

  // Check if |param| (which must be a SocketPermissionData::CheckParam)
  // matches the spec of |this|.
  bool Check(const APIPermission::CheckParam* param) const;

  // Convert |this| into a base::Value.
  scoped_ptr<base::Value> ToValue() const;

  // Populate |this| from a base::Value.
  bool FromValue(const base::Value* value);

  HostType GetHostType() const;
  const std::string GetHost() const;

  const content::SocketPermissionRequest& pattern() const { return pattern_; }
  const bool& match_subdomains() const { return match_subdomains_; }

  // These accessors are provided for IPC_STRUCT_TRAITS_MEMBER.  Please
  // think twice before using them for anything else.
  content::SocketPermissionRequest& pattern();
  bool& match_subdomains();

  // TODO(bryeung): SocketPermissionData should be encoded as a base::Value
  // instead of a string.  Until that is done, expose these methods for
  // testing.
  bool ParseForTest(const std::string& permission) { return Parse(permission); }
  const std::string& GetAsStringForTest() const { return GetAsString(); }

 private:
  bool Parse(const std::string& permission);
  const std::string& GetAsString() const;
  void Reset();

  content::SocketPermissionRequest pattern_;
  bool match_subdomains_;
  mutable std::string spec_;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_SOCKET_PERMISSION_DATA_H_
