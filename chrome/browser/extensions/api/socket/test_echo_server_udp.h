// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SOCKET_TEST_ECHO_SERVER_UDP_H_
#define CHROME_BROWSER_EXTENSIONS_API_SOCKET_TEST_ECHO_SERVER_UDP_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "net/base/completion_callback.h"
#include "net/base/ip_endpoint.h"

namespace net {
class CapturingNetLog;
class IOBufferWithSize;
class UDPServerSocket;
}

namespace extensions {

// A simple echo server. Yes, simple even for an echo server. It is compatible
// with Chrome's threading model, and it exits after a single echo roundtrip.
//
// Highly recommended for functional tests.
//
// The server looks for a high port for listening. The Start() method returns
// that port.
class TestEchoServerUDP
 : public base::RefCountedThreadSafe<TestEchoServerUDP> {
 public:
  TestEchoServerUDP();

  // Spins up a task on the IO thread. Blocks until the server is listening on
  // that thread. Returns the listening port.
  int Start();

 private:
  static const int kMaxRead = 1024;

  // The port range to try listening. If listening fails, increment and try
  // again until we've tried enough times that something's obviously wrong.
  static const int kFirstPort = 32768;
  static const int kPortRange = 16384;

  base::WaitableEvent listening_event_;
  int port_;
  scoped_ptr<net::CapturingNetLog> server_log_;
  net::UDPServerSocket* socket_;  // See CleanUpOnIOThread re raw pointer.
  scoped_refptr<net::IOBufferWithSize> buffer_;
  net::IPEndPoint recv_from_address_;
  net::CompletionCallback callback_;

  ~TestEchoServerUDP();
  friend class base::RefCountedThreadSafe<TestEchoServerUDP>;

  void RunOnIOThread();
  void SetResult(int result);
  void HandleRequest(int result);
  void CleanUpOnIOThread();
  void CreateUDPAddress(std::string ip_str, int port,
                        net::IPEndPoint* address);
  int SendToSocket(net::UDPServerSocket* socket, std::string msg);
  int SendToSocket(net::UDPServerSocket* socket, std::string msg,
                   const net::IPEndPoint& address);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SOCKET_TEST_ECHO_SERVER_UDP_H_
