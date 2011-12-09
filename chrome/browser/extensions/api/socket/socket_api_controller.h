// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SOCKET_SOCKET_API_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SOCKET_SOCKET_API_CONTROLLER_H_
#pragma once

#include <string>
#include <map>

#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "googleurl/src/gurl.h"
#include "net/base/completion_callback.h"

class Profile;

namespace base {
class ListValue;
class Value;
}

namespace net {
class UDPClientSocket;
class IPEndPoint;
}

namespace extensions {

class Socket;

// SocketController keeps track of a collection of Sockets and provides a
// convenient set of methods to manipulate them.
class SocketController {
 public:
  SocketController();
  virtual ~SocketController();

  // Create/Destroy are a pair. They represent the allocation and deallocation
  // of the Socket object in memory.
  //
  // TODO(miket): aa's suggestion to track lifetime of callbacks associated
  // with each socket, which will then let us clean up when we go out of scope
  // rather than requiring that the app developer remember to call Destroy.
  int CreateUdp(const Profile* profile, const std::string& extension_id,
                const GURL& src_url);
  bool DestroyUdp(int socket_id);

  // Connect, Close, Read, and Write map to the equivalent methods in
  // UDPClientSocket.
  //
  // TODO(miket): Implement Read.
  bool ConnectUdp(int socket_id, const std::string address, int port);
  void CloseUdp(int socket_id);
  int WriteUdp(int socket_id, const std::string msg);

  // Converts a string IP address and integer port into a format that
  // UDPClientSocket can deal with. Public so test harness can use it.
  static bool CreateIPEndPoint(const std::string address, int port,
                               net::IPEndPoint* ip_end_point);

 private:
  int next_socket_id_;
  typedef std::map<int, linked_ptr<Socket> > SocketMap;
  SocketMap socket_map_;

  // Convenience method for accessing SocketMap.
  Socket* GetSocket(int socket_id);

  DISALLOW_COPY_AND_ASSIGN(SocketController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SOCKET_SOCKET_API_CONTROLLER_H_
