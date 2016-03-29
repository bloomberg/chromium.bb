// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_parser.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/numerics/safe_conversions.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/log/net_log.h"
#include "net/log/test_net_log.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/socket_test_util.h"
#include "url/gurl.h"

// Fuzzer for HttpStreamParser.
//
// |data| is the data received over a mock HTTP connection through one or more
// reads, along with metadata about the size of each read, and whether or not
// the read completely synchronously.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Needed for thread checks and waits.
  base::MessageLoopForIO message_loop;

  net::MockWrite writes[] = {
      net::MockWrite(net::ASYNC, 0, "GET / HTTP/1.1\r\n\r\n"),
  };

  // Break the buffer into a sequence of variable sized sync and async
  // reads.  Use the last bytes of |data| exclusively for determining
  // the size and type of each read.
  std::vector<net::MockRead> reads;
  // Sequence number for socket operations.
  int last_sequence_number = 0;

  // IoMode for the final read, where the server closes the mock socket.
  net::IoMode close_socket_io_mode = net::ASYNC;

  // Break |data| up into reads.  The test may or may not make to the final
  // read, where the server closes the socket.

  // Each read needs a one byte seed to determine read size and whether it
  // should be sync or async, so if there's only one byte left unused, can't use
  // it here.  The bytes used to get this metadata are not used as over-the-wire
  // bytes.
  while (size > 0) {
    size_t read_seed = data[size - 1];
    size--;
    net::IoMode io_mode = net::ASYNC;
    // Low order bit determines IoMode.
    if (read_seed & 0x1)
      io_mode = net::SYNCHRONOUS;

    // Use second bit to determine if the last read, when the socket is closed,
    // is synchronous.  The second bit only matters the last time this loop is
    // runs.
    if (read_seed & 0x2) {
      close_socket_io_mode = net::SYNCHRONOUS;
    } else {
      close_socket_io_mode = net::ASYNC;
    }

    // If there are no more bytes in |data|, next read is the connection close.
    if (size == 0)
      break;

    read_seed >>= 2;

    // Last 6 bits determine how many bytes are returned by the read.
    int read_size = static_cast<int>(std::min(1 + read_seed, size));
    reads.push_back(net::MockRead(io_mode, reinterpret_cast<const char*>(data),
                                  read_size, ++last_sequence_number));

    data += read_size;
    size -= read_size;
  }

  // Server closes the socket.
  reads.push_back(net::MockRead(close_socket_io_mode,
                                net::ERR_CONNECTION_CLOSED,
                                ++last_sequence_number));
  net::SequencedSocketData socket_data(reads.data(), reads.size(), writes,
                                       arraysize(writes));
  socket_data.set_connect_data(net::MockConnect(net::SYNCHRONOUS, net::OK));

  scoped_ptr<net::MockTCPClientSocket> socket(
      new net::MockTCPClientSocket(net::AddressList(), nullptr, &socket_data));

  net::TestCompletionCallback callback;
  CHECK_EQ(net::OK, socket->Connect(callback.callback()));

  net::ClientSocketHandle socket_handle;
  socket_handle.SetSocket(std::move(socket));

  net::HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://localhost/");

  scoped_refptr<net::GrowableIOBuffer> read_buffer(new net::GrowableIOBuffer());
  // Use a NetLog that listens to events, to get coverage of logging
  // callbacks.
  net::BoundTestNetLog net_log;
  net::HttpStreamParser parser(&socket_handle, &request_info, read_buffer.get(),
                               net_log.bound());

  net::HttpResponseInfo response_info;
  int result =
      parser.SendRequest("GET / HTTP/1.1\r\n", net::HttpRequestHeaders(),
                         &response_info, callback.callback());
  CHECK_EQ(net::OK, callback.GetResult(result));

  result = parser.ReadResponseHeaders(callback.callback());
  result = callback.GetResult(result);

  if (result != net::OK)
    return 0;

  while (true) {
    // 64 exactly matches the maximum amount of data returned by a single
    // MockRead, as created above.
    scoped_refptr<net::IOBufferWithSize> io_buffer(
        new net::IOBufferWithSize(64));
    result = parser.ReadResponseBody(io_buffer.get(), io_buffer->size(),
                                     callback.callback());

    // Releasing the pointer to IOBuffer immediately is more likely to lead to a
    // use-after-free.
    io_buffer = nullptr;

    if (callback.GetResult(result) <= 0)
      break;
  }

  return 0;
}
