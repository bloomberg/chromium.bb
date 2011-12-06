// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SOCKET_SOCKET_API_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SOCKET_SOCKET_API_CONTROLLER_H_
#pragma once

#include <string>
#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
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

// The SocketController singleton keeps track of all our Sockets, and provides
// a convenient set of methods to manipulate them.
class SocketController {
 public:
  static SocketController* GetInstance();

  SocketController();
  virtual ~SocketController();

  // Create/Destroy are a pair. They represent the allocation and deallocation
  // of the Socket object in memory.
  //
  // TODO(miket): we currently require the app developer to remember to call
  // Destroy, which is a buzzkill in JavaScript. I believe that to solve this,
  // we'll have to associate each Socket with a creator extension, and then
  // clean up when the extension goes out of scope. As the API is defined
  // today, we're exposing only primitive socketIds to JavaScript, which seems
  // to imply that we won't be able to garbage-collect when individual sockets
  // "go out of scope" (in quotes because they never do).
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
  typedef std::map<int, Socket*> SocketMap;
  SocketMap socket_map_;

  // Convenience method for accessing SocketMap.
  Socket* GetSocket(int socket_id);

  friend struct DefaultSingletonTraits<SocketController>;

  DISALLOW_COPY_AND_ASSIGN(SocketController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SOCKET_SOCKET_API_CONTROLLER_H_
