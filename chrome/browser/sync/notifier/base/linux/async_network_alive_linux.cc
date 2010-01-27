// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/base/async_network_alive.h"

#include <sys/socket.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include "base/logging.h"
#include "talk/base/physicalsocketserver.h"

namespace notifier {

class AsyncNetworkAliveLinux : public AsyncNetworkAlive {
 public:
  AsyncNetworkAliveLinux() {
    if (pipe(exit_pipe_) == -1) {
      PLOG(ERROR) << "Could not create pipe for exit signal.";
      exit_pipe_[0] = 0;
      exit_pipe_[1] = 0;
    }
  }

  virtual ~AsyncNetworkAliveLinux() {
    if (exit_pipe_[1]) {
      close(exit_pipe_[1]);
    }
  }

 protected:
  // SignalThread Interface
  virtual void DoWork() {
    if (!exit_pipe_[0]) {
      // If we don't have an exit flag to listen for, set the error flag and
      // abort.
      error_ = true;
      return;
    }

    // This function listens for changes to network interfaces, and link state.
    // It's copied from syncapi.cc.
    struct sockaddr_nl socket_address;

    memset(&socket_address, 0, sizeof(socket_address));
    socket_address.nl_family = AF_NETLINK;
    socket_address.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR;

    // NETLINK_ROUTE is the protocol used to update the kernel routing table.
    int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    bind(fd, (struct sockaddr *) &socket_address, sizeof(socket_address));

    fd_set rdfs;
    FD_ZERO(&rdfs);
    FD_SET(fd, &rdfs);
    FD_SET(exit_pipe_[0], &rdfs);

    int max_fd = fd > exit_pipe_[0] ? fd : exit_pipe_[0];

    int result = select(max_fd + 1, &rdfs, NULL, NULL, NULL);

    if (result <= 0) {
      error_ = true;
      PLOG(ERROR) << "select() returned unexpected result " << result;
      close(fd);
      close(exit_pipe_[0]);
      return;
    }

    // Since we recieved a change from the socket, read the change in.
    if (FD_ISSET(fd, &rdfs)) {
      char buf[4096];
      struct iovec iov = { buf, sizeof(buf) };
      struct sockaddr_nl sa;

      struct msghdr msg = { (void *)&sa, sizeof(sa), &iov, 1, NULL, 0, 0 };
      recvmsg(fd, &msg, 0);
    }

    close(fd);

    // If exit_pipe was written to, we must be shutting down.
    if (FD_ISSET(exit_pipe_[0], &rdfs)) {
      alive_ = false;
      error_ = true;
      close(exit_pipe_[0]);
      return;
    }

    // If there is an active connection, check that talk.google.com:5222
    // is reachable.
    talk_base::PhysicalSocketServer physical;
    scoped_ptr<talk_base::Socket> socket(physical.CreateSocket(SOCK_STREAM));
    if (socket->Connect(talk_base::SocketAddress("talk.google.com", 5222))) {
      alive_ = false;
    } else {
      alive_ = true;
    }
  }

  virtual void OnWorkStop() {
    if (exit_pipe_[1]) {
      char data = 0;
      // We can't ignore the return value on write(), since that generates a
      // compile warning.  However, since we're exiting, there's nothing we can
      // do if this fails except to log it.
      if (write(exit_pipe_[1], &data, 1) == -1) {
        PLOG(WARNING) << "Error sending error signal to AsyncNetworkAliveLinux";
      }
    }
  }

 private:
  int exit_pipe_[2];
  DISALLOW_COPY_AND_ASSIGN(AsyncNetworkAliveLinux);
};

AsyncNetworkAlive* AsyncNetworkAlive::Create() {
  return new AsyncNetworkAliveLinux();
}

}  // namespace notifier
