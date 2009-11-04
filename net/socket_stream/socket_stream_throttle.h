// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_STREAM_SOCKET_STREAM_THROTTLE_H_
#define NET_SOCKET_STREAM_SOCKET_STREAM_THROTTLE_H_

#include <string>

#include "base/basictypes.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"

namespace net {

class SocketStream;

// Abstract interface to throttle SocketStream per URL scheme.
// Each URL scheme (protocol) could define own SocketStreamThrottle.
// These methods will be called on IO thread.
class SocketStreamThrottle {
 public:
  // Called when |socket| is about to open connection.
  // Returns net::OK if the connection can open now.
  // Returns net::ERR_IO_PENDING if the connection should wait.  In this case,
  // |callback| will be called when it's ready to open connection.
  virtual int OnStartOpenConnection(SocketStream* socket,
                                    CompletionCallback* callback) {
    // No throttle by default.
    return OK;
  }

  // Called when |socket| read |len| bytes of |data|.
  // May wake up another waiting socket.
  // Returns net::OK if |socket| can continue to run.
  // Returns net::ERR_IO_PENDING if |socket| should suspend to run.  In this
  // case, |callback| will be called when it's ready to resume running.
  virtual int OnRead(SocketStream* socket, const char* data, int len,
                     CompletionCallback* callback) {
    // No throttle by default.
    return OK;
  }

  // Called when |socket| wrote |len| bytes of |data|.
  // May wake up another waiting socket.
  // Returns net::OK if |socket| can continue to run.
  // Returns net::ERR_IO_PENDING if |socket| should suspend to run.  In this
  // case, |callback| will be called when it's ready to resume running.
  virtual int OnWrite(SocketStream* socket, const char* data, int len,
                      CompletionCallback* callback) {
    // No throttle by default.
    return OK;
  }

  // Called when |socket| is closed.
  // May wake up another waiting socket.
  virtual void OnClose(SocketStream* socket) {}

  // Gets SocketStreamThrottle for URL |scheme|.
  // Doesn't pass ownership of the SocketStreamThrottle.
  static SocketStreamThrottle* GetSocketStreamThrottleForScheme(
      const std::string& scheme);

  // Registers |throttle| for URL |scheme|.
  // Doesn't take ownership of |throttle|.  Typically |throttle| is
  // singleton instance.
  static void RegisterSocketStreamThrottle(
      const std::string& scheme,
      SocketStreamThrottle* throttle);

 protected:
  SocketStreamThrottle() {}
  virtual ~SocketStreamThrottle() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SocketStreamThrottle);
};

}  // namespace net

#endif  // NET_SOCKET_STREAM_SOCKET_STREAM_THROTTLE_H_
