// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/socket/test_echo_server_udp.h"

#include "base/memory/ref_counted.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/capturing_net_log.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/udp/udp_server_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using namespace net;

namespace extensions {

const std::string TestEchoServerUDP::kEOLPattern = "*ECHO";
const std::string TestEchoServerUDP::kQuitPattern = "*QUIT*";

TestEchoServerUDP::TestEchoServerUDP()
    : listening_event_(true, false),
      port_(0),
      server_log_(new CapturingNetLog(CapturingNetLog::kUnbounded)),
      socket_(NULL),
      buffer_(new IOBufferWithSize(kMaxRead)),
      ALLOW_THIS_IN_INITIALIZER_LIST(callback_(
          base::Bind(&TestEchoServerUDP::HandleReceivedData,
                     base::Unretained(this)))) {
}

TestEchoServerUDP::~TestEchoServerUDP() {
  EXPECT_EQ(NULL, socket_);
}

int TestEchoServerUDP::Start() {
  base::TimeDelta max_time = base::TimeDelta::FromSeconds(5);

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&TestEchoServerUDP::RunOnIOThread, this));

  if (listening_event_.TimedWait(max_time))
    return port_;
  else
    return -1;
}

void TestEchoServerUDP::RunOnIOThread() {
  CreateListeningSocket();
  listening_event_.Signal();
  Echo();
}

void TestEchoServerUDP::CreateListeningSocket() {
  int result = -1;

  int tries = 151;  // Birthday paradox. Possibly applicable. Sounds good.
  while (tries-- > 0) {
    socket_ = new UDPServerSocket(server_log_.get(), NetLog::Source());
    IPEndPoint bind_address;

    // Our implementation of RandInt() uses cryptographically secure random
    // numbers, rather than a seeded PRNG, so we ought to be protected against
    // the threat of multiple concurrent tests attempting to use the same
    // sequence of ports.
    port_ = base::RandInt(kFirstPort, kFirstPort + kPortRange);

    CreateUDPAddress("127.0.0.1", port_, &bind_address);
    result = socket_->Listen(bind_address);
    if (result == net::OK) {
      break;
    } else {
      delete socket_;
      socket_ = NULL;
    }
  }

  EXPECT_EQ(OK, result);
  EXPECT_TRUE(socket_ != NULL);
}

void TestEchoServerUDP::Echo() {
  TestCompletionCallback callback;
  int result = socket_->RecvFrom(buffer_, kMaxRead, &recv_from_address_,
                                 callback_);
  if (result == net::OK || result >= 0) {
    HandleReceivedData(result);
  } else if (result == net::ERR_IO_PENDING) {
    // No data available yet. When callback_ is called back with data, we will
    // pick things back up there.
  } else {
    // Something bad happened. Log it, then clean up. We expect that the
    // callback won't have AddRef'ed us, so that we can exit cleanly right from
    // here.
    VLOG(0) << "Error during RecvFrom: " << result;
    CleanUpOnIOThread();
  }
}

void TestEchoServerUDP::HandleReceivedData(int result) {
  bool should_quit = false;
  if (result > 0) {
    std::string packet(buffer_->data(), result);
    echo_string_ += packet;
    if (MatchPattern(echo_string_, kEOLPattern))
      SendEchoString();
    if (MatchPattern(echo_string_, kQuitPattern))
      should_quit = true;
  }

  if (should_quit) {
    CleanUpOnIOThread();
  } else {
    // We're done with the buffer, so let's make a new one.
    buffer_ = new IOBufferWithSize(kMaxRead);
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(&TestEchoServerUDP::Echo, this));
  }
}

void TestEchoServerUDP::SendEchoString() {
  int result = SendToSocket(socket_, echo_string_);
  EXPECT_EQ(echo_string_.length(), static_cast<size_t>(result));
  echo_string_.clear();
}

void TestEchoServerUDP::CleanUpOnIOThread() {
  // Our owner is going to destroy us on the UI thread, which will make the
  // socket complain about being called on the wrong (non-IO) thread. So
  // we'll delete it right now.
  delete socket_;
  socket_ = NULL;
}

void TestEchoServerUDP::CreateUDPAddress(std::string ip_str, int port,
                                         IPEndPoint* address) {
  IPAddressNumber ip_number;
  bool result = ParseIPLiteralToNumber(ip_str, &ip_number);
  if (!result)
    return;
  *address = IPEndPoint(ip_number, port);
}

int TestEchoServerUDP::SendToSocket(UDPServerSocket* socket, std::string msg) {
  return SendToSocket(socket, msg, recv_from_address_);
}

int TestEchoServerUDP::SendToSocket(UDPServerSocket* socket,
                                    std::string msg,
                                    const IPEndPoint& address) {
  TestCompletionCallback callback;

  int length = msg.length();
  scoped_refptr<StringIOBuffer> io_buffer(new StringIOBuffer(msg));
  scoped_refptr<DrainableIOBuffer> buffer(
      new DrainableIOBuffer(io_buffer, length));

  int bytes_sent = 0;
  while (buffer->BytesRemaining()) {
    int result = socket->SendTo(buffer, buffer->BytesRemaining(),
                                address, callback.callback());
    if (result == ERR_IO_PENDING)
      result = callback.WaitForResult();
    if (result <= 0)
      return bytes_sent > 0 ? bytes_sent : result;
    bytes_sent += result;
    buffer->DidConsume(result);
  }
  return bytes_sent;
}

}  // namespace
