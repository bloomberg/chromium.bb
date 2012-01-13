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

// A UDP echo server. Collects a line of characters ending with 'ECHO', and
// then echoes what it received. Quits when it receives 'QUIT' as four
// consecutive characters. Highly recommended for functional tests.
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

  // Waits until pending cleanup is finished. This method doesn't exactly match
  // the Start() method because quitting is initiated by a command sent by UDP
  // from the client to the server, rather than being initiated by a method.
  //
  // Returns true if successful. Fails if we had to wait an unreasonably long
  // time, which likely means that we weren't asked to start cleaning up.
  bool WaitUntilFinished();

 private:
  static const int kMaxRead = 1024;
  static const std::string kEOLPattern;
  static const std::string kQuitPattern;

  // The port range to try listening. If listening fails, increment and try
  // again until we've tried enough times that something's obviously wrong.
  static const int kFirstPort = 32768;
  static const int kPortRange = 16384;

  ~TestEchoServerUDP();
  friend class base::RefCountedThreadSafe<TestEchoServerUDP>;

  void RunOnIOThread();
  void CleanUpOnIOThread();
  void CreateListeningSocket();
  void Echo();
  void HandleReceivedData(int result);
  void SendEchoString();
  void CreateUDPAddress(std::string ip_str, int port,
                        net::IPEndPoint* address);
  int SendToSocket(net::UDPServerSocket* socket, std::string msg);
  int SendToSocket(net::UDPServerSocket* socket, std::string msg,
                   const net::IPEndPoint& address);

  base::WaitableEvent listening_event_;
  base::WaitableEvent cleanup_completed_event_;
  int port_;
  scoped_ptr<net::CapturingNetLog> server_log_;
  net::UDPServerSocket* socket_;  // See CleanUpOnIOThread re raw pointer.
  scoped_refptr<net::IOBufferWithSize> buffer_;
  std::string echo_string_;
  net::IPEndPoint recv_from_address_;
  net::CompletionCallback callback_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SOCKET_TEST_ECHO_SERVER_UDP_H_
