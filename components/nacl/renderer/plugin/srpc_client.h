/*
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// A representation of an SRPC connection.  These can be either to the
// service runtime or to untrusted NaCl threads.

#ifndef COMPONENTS_NACL_RENDERER_PLUGIN_SRPC_CLIENT_H_
#define COMPONENTS_NACL_RENDERER_PLUGIN_SRPC_CLIENT_H_

#include <map>
#include <string>

#include "components/nacl/renderer/plugin/utility.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

namespace nacl {
class DescWrapper;
}  // namespace nacl

namespace plugin {

class MethodInfo;
class SrpcParams;

//  SrpcClient represents an SRPC connection to a client.
class SrpcClient {
 public:
  // Factory method for creating SrpcClients.
  static SrpcClient* New(nacl::DescWrapper* wrapper);

  //  Init is passed a DescWrapper.  The SrpcClient performs service
  //  discovery and provides the interface for future rpcs.
  bool Init(nacl::DescWrapper* socket);

  //  The destructor closes the connection to sel_ldr.
  ~SrpcClient();

  //  Test whether the SRPC service has a given method.
  bool HasMethod(const std::string& method_name);
  //  Invoke an SRPC method.
  bool Invoke(const std::string& method_name, SrpcParams* params);
  // Get the error status from that last method invocation
  NaClSrpcError GetLastError() const { return last_error_; }
  bool InitParams(const std::string& method_name, SrpcParams* params);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(SrpcClient);
  SrpcClient();
  void GetMethods();
  typedef std::map<std::string, MethodInfo*> Methods;
  Methods methods_;
  NaClSrpcChannel srpc_channel_;
  bool srpc_channel_initialised_;
  NaClSrpcError last_error_;
};

}  // namespace plugin

#endif  // COMPONENTS_NACL_RENDERER_PLUGIN_SRPC_CLIENT_H_
