// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CURVECP_TEST_CLIENT_H_
#define NET_CURVECP_TEST_CLIENT_H_
#pragma once

#include "base/task.h"
#include "net/base/completion_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/test_data_stream.h"

namespace net {

class CurveCPClientSocket;

// The TestClient connects to a test server, sending a stream of verifiable
// bytes.  The TestClient expects to get the same bytes echoed back from the
// TestServer.  After sending all bytes and receiving the echoes, the
// TestClient closes itself.
//
// Several hooks are provided for testing edge cases and failures.
class TestClient {
 public:
  TestClient();
  virtual ~TestClient();

  // Starts the client, connecting to |server|.
  // Client will send |bytes_to_send| bytes from the verifiable stream.
  // When the client has received all echoed bytes from the server, or
  // when an error occurs causing the client to stop, |callback| will be
  // called with a net status code.
  // Returns true if successful in starting the client.
  bool Start(const HostPortPair& server,
             int bytes_to_send,
             OldCompletionCallback* callback);

  // Returns the number of errors this server encountered.
  int error_count() { return errors_; }

 private:
  static const int kMaxMessage = 1024;

  void OnConnectComplete(int result);
  void OnReadComplete(int result);
  void OnWriteComplete(int result);

  void ReadData();
  void SendData();
  void Finish(int result);

  CurveCPClientSocket* socket_;
  scoped_refptr<IOBuffer> read_buffer_;
  scoped_refptr<DrainableIOBuffer> write_buffer_;
  int errors_;
  int bytes_to_read_;
  int bytes_to_send_;
  TestDataStream sent_stream_;
  TestDataStream received_stream_;
  OldCompletionCallbackImpl<TestClient> connect_callback_;
  OldCompletionCallbackImpl<TestClient> read_callback_;
  OldCompletionCallbackImpl<TestClient> write_callback_;
  OldCompletionCallback* finished_callback_;
};

}  // namespace net

#endif  // NET_CURVECP_TEST_CLIENT_H_
