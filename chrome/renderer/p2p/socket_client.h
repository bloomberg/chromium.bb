// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_P2P_SOCKET_CLIENT_H_
#define CHROME_RENDERER_P2P_SOCKET_CLIENT_H_

#include <vector>

#include "base/ref_counted.h"
#include "content/common/p2p_sockets.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

class P2PSocketDispatcher;
class Task;

// P2P socket that rountes all calls over IPC. It can be created and
// used from any thread.
class P2PSocketClient : public base::RefCountedThreadSafe<P2PSocketClient> {
 public:
  // Delegate is called on the IPC thread.
  class Delegate {
   public:
    virtual ~Delegate() { }

    virtual void OnOpen(P2PSocketAddress address) = 0;
    virtual void OnError() = 0;
    virtual void OnDataReceived(P2PSocketAddress address,
                                const std::vector<char>& data) = 0;
  };

  explicit P2PSocketClient(P2PSocketDispatcher* dispatcher);

  // Initialize socket of the specified |type| and connected to the
  // specified |address|. |address| matters only when |type| is set to
  // P2P_SOCKET_TCP_CLIENT.
  void Init(P2PSocketType type, P2PSocketAddress address, Delegate* delegate);

  // Send the |data| to the |address|.
  void Send(P2PSocketAddress address, const std::vector<char>& data);

  // Must be called before the socket is destroyed. The delegate may
  // not be called after |closed_task| is executed.
  void Close(Task* closed_task);

  int socket_id() const { return socket_id_; }

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

  void OnSocketCreated(P2PSocketAddress address);
  void OnError();
  void OnDataReceived(P2PSocketAddress address,
                      const std::vector<char>& data);

  // Called by the dispatcher when it is destroyed.
  void Detach();

  P2PSocketDispatcher* dispatcher_;
  scoped_refptr<base::MessageLoopProxy> message_loop_;
  int socket_id_;
  Delegate* delegate_;
  State state_;

  DISALLOW_COPY_AND_ASSIGN(P2PSocketClient);
};

#endif  // CHROME_RENDERER_P2P_SOCKET_CLIENT_H_
