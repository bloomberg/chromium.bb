// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SOCKET_SOCKET_H_
#define CHROME_BROWSER_EXTENSIONS_API_SOCKET_SOCKET_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/api_resource.h"
#include "net/base/io_buffer.h"

namespace net {
class Socket;
}

namespace extensions {

// A Socket wraps a low-level socket and includes housekeeping information that
// we need to manage it in the context of an extension.
class Socket : public APIResource {
 public:
  virtual ~Socket();

  // Returns true iff the socket was able to properly initialize itself.
  virtual bool IsValid() = 0;

  // Returns net::OK if successful, or an error code otherwise.
  virtual int Connect() = 0;
  virtual void Disconnect() = 0;

  // Returns a string representing what was able to be read without blocking.
  // If it reads an empty string, or blocks... the behavior is
  // indistinguishable! TODO(miket): this is awful. We should be returning a
  // blob, and we should be giving the caller all needed information about
  // what's happening with the read operation.
  virtual std::string Read();

  // Returns the number of bytes successfully written, or a negative error
  // code. Note that ERR_IO_PENDING means that the operation blocked, in which
  // case |event_notifier| will eventually be called with the final result
  // (again, either a nonnegative number of bytes written, or a negative
  // error).
  virtual int Write(const std::string& message);

  virtual void OnDataRead(int result);
  virtual void OnWriteComplete(int result);

 protected:
  Socket(const std::string& address, int port,
         APIResourceEventNotifier* event_notifier);
  virtual net::Socket* socket() = 0;

  const std::string address_;
  int port_;
  bool is_connected_;

 private:
  static const int kMaxRead = 1024;
  scoped_refptr<net::IOBufferWithSize> read_buffer_;
};

}  //  namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SOCKET_SOCKET_H_
