// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_P2P_SOCKET_CLIENT_H_
#define CONTENT_RENDERER_P2P_SOCKET_CLIENT_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "content/common/p2p_sockets.h"
#include "net/base/ip_endpoint.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace content {

class P2PSocketDispatcher;

// P2P socket that rountes all calls over IPC.
//
// The object runs on two threads: IPC thread and delegate thread. The
// IPC thread is used to interact with P2PSocketDispatcher. All
// callbacks to the user of this class are called on the delegate
// thread which is specified in Init().
class P2PSocketClient : public base::RefCountedThreadSafe<P2PSocketClient> {
 public:
  // Delegate is called on the the same thread on which P2PSocketCLient is
  // created.
  class Delegate {
   public:
    virtual ~Delegate() { }

    virtual void OnOpen(const net::IPEndPoint& address) = 0;
    virtual void OnIncomingTcpConnection(const net::IPEndPoint& address,
                                         P2PSocketClient* client) = 0;
    virtual void OnSendComplete() = 0;
    virtual void OnError() = 0;
    virtual void OnDataReceived(const net::IPEndPoint& address,
                                const std::vector<char>& data) = 0;
  };

  explicit P2PSocketClient(P2PSocketDispatcher* dispatcher);

  // Initialize socket of the specified |type| and connected to the
  // specified |address|. |address| matters only when |type| is set to
  // P2P_SOCKET_TCP_CLIENT.
  void Init(P2PSocketType type,
            const net::IPEndPoint& local_address,
            const net::IPEndPoint& remote_address,
            Delegate* delegate);

  // Send the |data| to the |address|.
  void Send(const net::IPEndPoint& address, const std::vector<char>& data);

  // Must be called before the socket is destroyed. The delegate may
  // not be called after |closed_task| is executed.
  void Close();

  int socket_id() const { return socket_id_; }

  void set_delegate(Delegate* delegate);

 private:
  enum State {
    STATE_UNINITIALIZED,
    STATE_OPENING,
    STATE_OPEN,
    STATE_CLOSED,
    STATE_ERROR,
  };

  friend class P2PSocketDispatcher;

  // Calls destructor.
  friend class base::RefCountedThreadSafe<P2PSocketClient>;

  virtual ~P2PSocketClient();

  // Message handlers that run on IPC thread.
  void OnSocketCreated(const net::IPEndPoint& address);
  void OnIncomingTcpConnection(const net::IPEndPoint& address);
  void OnSendComplete(int packet_id);
  void OnSendComplete();
  void OnError();
  void OnDataReceived(const net::IPEndPoint& address,
                      const std::vector<char>& data);

  // Proxy methods that deliver messages to the delegate thread.
  void DeliverOnSocketCreated(const net::IPEndPoint& address);
  void DeliverOnIncomingTcpConnection(
      const net::IPEndPoint& address,
      scoped_refptr<P2PSocketClient> new_client);
  void DeliverOnSendComplete();
  void DeliverOnError();
  void DeliverOnDataReceived(const net::IPEndPoint& address,
                             const std::vector<char>& data);

  // Scheduled on the IPC thread to finish initialization.
  void DoInit(P2PSocketType type,
              const net::IPEndPoint& local_address,
              const net::IPEndPoint& remote_address);

  // Scheduled on the IPC thread to finish closing the connection.
  void DoClose();

  // Called by the dispatcher when it is destroyed.
  void Detach();

  P2PSocketDispatcher* dispatcher_;
  scoped_refptr<base::MessageLoopProxy> ipc_message_loop_;
  scoped_refptr<base::MessageLoopProxy> delegate_message_loop_;
  int socket_id_;
  Delegate* delegate_;
  State state_;

  DISALLOW_COPY_AND_ASSIGN(P2PSocketClient);
};

}  // namespace content

#endif  // CONTENT_RENDERER_P2P_SOCKET_CLIENT_H_
