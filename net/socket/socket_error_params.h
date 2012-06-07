// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SOCKET_ERROR_PARAMS_H_
#define NET_SOCKET_SOCKET_ERROR_PARAMS_H_
#pragma once

#include "net/base/net_log.h"

namespace net {

// Extra parameters to attach to the NetLog when we receive a socket error.
class SocketErrorParams : public NetLog::EventParameters {
 public:
  SocketErrorParams(int net_error, int os_error);

  virtual base::Value* ToValue() const OVERRIDE;

 protected:
  virtual ~SocketErrorParams();

 private:
  const int net_error_;
  const int os_error_;
};

}  // namespace net

#endif  // NET_SOCKET_SOCKET_ERROR_PARAMS_H_
