// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_P2P_HOST_ADDRESS_REQUEST_H_
#define CONTENT_RENDERER_P2P_HOST_ADDRESS_REQUEST_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_util.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace content {

class P2PSocketDispatcher;

class P2PHostAddressRequest :
    public base::RefCountedThreadSafe<P2PHostAddressRequest>  {
 public:
  typedef base::Callback<void(const net::IPAddressNumber&)> DoneCallback;

  P2PHostAddressRequest(P2PSocketDispatcher* dispatcher);

  // Sends host address request.
  void Request(const std::string& host_name,
               const DoneCallback& done_callback);

  // Cancels the request. The callback passed to Request() will not be
  // called after Cancel() is called.
  void Cancel();

 private:
  enum State {
    STATE_CREATED,
    STATE_SENT,
    STATE_FINISHED,
  };

  friend class P2PSocketDispatcher;

  friend class base::RefCountedThreadSafe<P2PHostAddressRequest>;
  virtual ~P2PHostAddressRequest();

  void DoSendRequest(const std::string& host_name,
                     const DoneCallback& done_callback);
  void DoUnregister();
  void OnResponse(const net::IPAddressNumber& address);
  void DeliverResponse(const net::IPAddressNumber& address);

  P2PSocketDispatcher* dispatcher_;
  scoped_refptr<base::MessageLoopProxy> ipc_message_loop_;
  scoped_refptr<base::MessageLoopProxy> delegate_message_loop_;
  DoneCallback done_callback_;

  // State must be accessed from delegate thread only.
  State state_;

  // Accessed on the IPC thread only.
  int32 request_id_;
  bool registered_;

  DISALLOW_COPY_AND_ASSIGN(P2PHostAddressRequest);
};

}  // namespace content

#endif  // CONTENT_RENDERER_P2P_HOST_ADDRESS_REQUEST_H_
