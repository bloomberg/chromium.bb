// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_POSIX_H_
#define CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_POSIX_H_

#include "content/common/p2p_sockets.h"

#include "base/message_loop.h"
#include "content/browser/renderer_host/p2p_socket_host.h"

class P2PSocketHostPosix : public P2PSocketHost {
 public:
  P2PSocketHostPosix(P2PSocketsHost* host, int routing_id, int id);
  virtual ~P2PSocketHostPosix();

  virtual bool Init();
  virtual void Send(const net::IPEndPoint& socket_address,
                    const std::vector<char>& data);

 private:
  enum State {
    STATE_UNINITIALIZED,
    STATE_OPEN,
    STATE_ERROR,
  };

  // MessageLoopForIO::Watcher implementation used to catch read
  // events from the socket.
  class ReadWatcher : public MessageLoopForIO::Watcher {
   public:
    explicit ReadWatcher(P2PSocketHostPosix* socket) : socket_(socket) { }

    // MessageLoopForIO::Watcher methods
    virtual void OnFileCanReadWithoutBlocking(int /* fd */) {
      socket_->DidCompleteRead();
    }
    virtual void OnFileCanWriteWithoutBlocking(int /* fd */) {}

   private:
    P2PSocketHostPosix* socket_;

    DISALLOW_COPY_AND_ASSIGN(ReadWatcher);
  };

  void DidCompleteRead();
  void OnError();

  State state_;
  int socket_;
  MessageLoopForIO::FileDescriptorWatcher read_socket_watcher_;
  ReadWatcher read_watcher_;

  DISALLOW_COPY_AND_ASSIGN(P2PSocketHostPosix);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_POSIX_H_
