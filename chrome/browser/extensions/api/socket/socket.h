// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SOCKET_SOCKET_H_
#define CHROME_BROWSER_EXTENSIONS_API_SOCKET_SOCKET_H_
#pragma once

#include <queue>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/api/api_resource.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"

namespace net {
class AddressList;
class IPEndPoint;
class Socket;
}

namespace extensions {

typedef base::Callback<void(int)> CompletionCallback;
typedef base::Callback<void(int, scoped_refptr<net::IOBuffer> io_buffer)>
    ReadCompletionCallback;
typedef base::Callback<
  void(int, scoped_refptr<net::IOBuffer> io_buffer, const std::string&, int)>
      RecvFromCompletionCallback;

// A Socket wraps a low-level socket and includes housekeeping information that
// we need to manage it in the context of an extension.
class Socket : public APIResource {
 public:
  virtual ~Socket();
  virtual void Connect(const std::string& address,
                       int port,
                       const CompletionCallback& callback) = 0;
  virtual void Disconnect() = 0;
  virtual int Bind(const std::string& address, int port) = 0;

  // The |callback| will be called with the number of bytes read into the
  // buffer, or a negative number if an error occurred.
  virtual void Read(int count,
                    const ReadCompletionCallback& callback) = 0;

  // The |callback| will be called with |byte_count| or a negative number if an
  // error occurred.
  void Write(scoped_refptr<net::IOBuffer> io_buffer,
             int byte_count,
             const CompletionCallback& callback);

  virtual void RecvFrom(int count,
                        const RecvFromCompletionCallback& callback) = 0;
  virtual void SendTo(scoped_refptr<net::IOBuffer> io_buffer,
                      int byte_count,
                      const std::string& address,
                      int port,
                      const CompletionCallback& callback) = 0;
  static bool StringAndPortToAddressList(const std::string& ip_address_str,
                                         int port,
                                         net::AddressList* address_list);
  static bool StringAndPortToIPEndPoint(const std::string& ip_address_str,
                                        int port,
                                        net::IPEndPoint* ip_end_point);
  static void IPEndPointToStringAndPort(const net::IPEndPoint& address,
                                        std::string* ip_address_str,
                                        int* port);

 protected:
  explicit Socket(APIResourceEventNotifier* event_notifier);

  void WriteData();
  virtual int WriteImpl(net::IOBuffer* io_buffer,
                        int io_buffer_size,
                        const net::CompletionCallback& callback) = 0;
  virtual void OnWriteComplete(int result);

  const std::string address_;
  int port_;
  bool is_connected_;

 private:
  struct WriteRequest {
    WriteRequest(scoped_refptr<net::IOBuffer> io_buffer,
                 int byte_count,
                 const CompletionCallback& callback)
      : io_buffer(io_buffer),
        byte_count(byte_count),
        callback(callback),
        bytes_written(0) { }
    ~WriteRequest() { }
    scoped_refptr<net::IOBuffer> io_buffer;
    int byte_count;
    CompletionCallback callback;
    int bytes_written;
  };
  std::queue<WriteRequest> write_queue_;
  scoped_refptr<net::IOBuffer> io_buffer_write_;
};

}  //  namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SOCKET_SOCKET_H_
