// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_ENDPOINTS_COPRESENCE_SOCKET_H_
#define COMPONENTS_COPRESENCE_ENDPOINTS_COPRESENCE_SOCKET_H_

#include <string>

#include "base/macros.h"

namespace net {
class IOBuffer;
}

namespace copresence_endpoints {

// A CopresenceSocket is an object that is used to send receive data to and
// from CopresencePeers.
// TODO(rkc): Add the ability to connect to a remote CopresencePeer.
class CopresenceSocket {
 public:
  typedef base::Callback<void(const scoped_refptr<net::IOBuffer>& buffer,
                              int buffer_size)> ReceiveCallback;

  CopresenceSocket() {}
  virtual ~CopresenceSocket() {}

  // Send data on this socket. If unable to send the data, return false. This
  // operation only guarantees that if the return value is a success, the send
  // was started. It does not make any guarantees about the completion of the
  // operation.
  // TODO(rkc): Expand the bool into more a more detailed failures enum.
  virtual bool Send(const scoped_refptr<net::IOBuffer>& buffer,
                    int buffer_size) = 0;
  virtual void Receive(const ReceiveCallback& callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CopresenceSocket);
};

}  // namespace copresence_endpoints

#endif  // COMPONENTS_COPRESENCE_ENDPOINTS_COPRESENCE_SOCKET_H_
