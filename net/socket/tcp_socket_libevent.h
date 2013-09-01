// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TCP_SOCKET_LIBEVENT_H_
#define NET_SOCKET_TCP_SOCKET_LIBEVENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/address_family.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"
#include "net/base/net_log.h"

namespace net {

class IPEndPoint;

// TODO(yzshen): This class is incomplete. TCP client operations (Connect/Read/
// Write/etc.) will be added. And TCPClientSocket will be changed to be a
// wrapper around TCPSocket.
class NET_EXPORT TCPSocketLibevent : public base::NonThreadSafe,
                                     public base::MessageLoopForIO::Watcher {
 public:
  TCPSocketLibevent(NetLog* net_log, const NetLog::Source& source);
  virtual ~TCPSocketLibevent();

  int Create(AddressFamily family);
  // Takes ownership of |socket|.
  int Adopt(int socket);
  // Returns a socket file descriptor. The ownership is transferred to the
  // caller.
  int Release();
  int Bind(const IPEndPoint& address);
  int GetLocalAddress(IPEndPoint* address) const;
  int Listen(int backlog);
  int Accept(scoped_ptr<TCPSocketLibevent>* socket,
             IPEndPoint* address,
             const CompletionCallback& callback);
  int SetDefaultOptionsForServer();
  int SetAddressReuse(bool allow);
  void Close();

  const BoundNetLog& net_log() const { return net_log_; }

  // MessageLoopForIO::Watcher implementation.
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE;

 private:
  int AcceptInternal(scoped_ptr<TCPSocketLibevent>* socket,
                     IPEndPoint* address);

  int socket_;

  base::MessageLoopForIO::FileDescriptorWatcher accept_socket_watcher_;

  scoped_ptr<TCPSocketLibevent>* accept_socket_;
  IPEndPoint* accept_address_;
  CompletionCallback accept_callback_;

  BoundNetLog net_log_;

  DISALLOW_COPY_AND_ASSIGN(TCPSocketLibevent);
};

}  // namespace net

#endif  // NET_SOCKET_TCP_SOCKET_LIBEVENT_H_
