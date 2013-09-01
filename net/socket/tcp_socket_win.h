// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TCP_SOCKET_WIN_H_
#define NET_SOCKET_TCP_SOCKET_WIN_H_

#include <winsock2.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/non_thread_safe.h"
#include "base/win/object_watcher.h"
#include "net/base/address_family.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"
#include "net/base/net_log.h"

namespace net {

class IPEndPoint;

// TODO(yzshen): This class is incomplete. TCP client operations (Connect/Read/
// Write/etc.) will be added. And TCPClientSocket will be changed to be a
// wrapper around TCPSocket.
class NET_EXPORT TCPSocketWin : NON_EXPORTED_BASE(public base::NonThreadSafe),
                                public base::win::ObjectWatcher::Delegate  {
 public:
  TCPSocketWin(NetLog* net_log, const NetLog::Source& source);
  virtual ~TCPSocketWin();

  int Create(AddressFamily family);
  // Takes ownership of |socket|.
  int Adopt(SOCKET socket);
  // Returns a socket descriptor. The ownership is transferred to the caller.
  SOCKET Release();
  int Bind(const IPEndPoint& address);
  int GetLocalAddress(IPEndPoint* address) const;
  int Listen(int backlog);
  int Accept(scoped_ptr<TCPSocketWin>* socket,
             IPEndPoint* address,
             const CompletionCallback& callback);
  int SetDefaultOptionsForServer();
  int SetExclusiveAddrUse();
  void Close();

  const BoundNetLog& net_log() const { return net_log_; }

  // base::ObjectWatcher::Delegate implementation.
  virtual void OnObjectSignaled(HANDLE object) OVERRIDE;

 private:
  int AcceptInternal(scoped_ptr<TCPSocketWin>* socket,
                     IPEndPoint* address);

  SOCKET socket_;
  HANDLE socket_event_;

  base::win::ObjectWatcher accept_watcher_;

  scoped_ptr<TCPSocketWin>* accept_socket_;
  IPEndPoint* accept_address_;
  CompletionCallback accept_callback_;

  BoundNetLog net_log_;

  DISALLOW_COPY_AND_ASSIGN(TCPSocketWin);
};

}  // namespace net

#endif  // NET_SOCKET_TCP_SOCKET_WIN_H_
