// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SOCKET_SOCKET_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SOCKET_SOCKET_API_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/api/api_function.h"
#include "net/base/io_buffer.h"

#include <string>

namespace extensions {

class APIResourceController;
class APIResourceEventNotifier;

extern const char kBytesWrittenKey[];
extern const char kSocketIdKey[];
extern const char kUdpSocketType[];

class SocketExtensionFunction : public AsyncIOAPIFunction {
 public:
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;
};

// Many of these socket functions are synchronous in the sense that
// they don't involve blocking operations, but we've made them all
// AsyncExtensionFunctions because the underlying UDPClientSocket
// library wants all operations to happen on the same thread as the
// one that created the socket. Too bad.

class SocketCreateFunction : public SocketExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.socket.create")

  SocketCreateFunction();

 protected:
  virtual ~SocketCreateFunction();

  // AsyncIOAPIFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  enum SocketType {
    kSocketTypeInvalid = -1,
    kSocketTypeTCP,
    kSocketTypeUDP
  };

  SocketType socket_type_;
  int src_id_;
  APIResourceEventNotifier* event_notifier_;
};

class SocketDestroyFunction : public SocketExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.socket.destroy")

 protected:
  virtual ~SocketDestroyFunction() {}

  // AsyncIOAPIFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  int socket_id_;
};

class SocketConnectFunction : public SocketExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.socket.connect")

  void OnCompleted(int result);

 protected:
  virtual ~SocketConnectFunction() {}

  // AsyncIOAPIFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  int socket_id_;
  std::string address_;
  int port_;
};

class SocketDisconnectFunction : public SocketExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.socket.disconnect")

 protected:
  virtual ~SocketDisconnectFunction() {}

  // AsyncIOAPIFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  int socket_id_;
};

class SocketBindFunction : public SocketExtensionFunction {
 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  int socket_id_;
  std::string address_;
  int port_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.socket.bind")
};

class SocketReadFunction : public SocketExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.socket.read")

  void OnCompleted(int result, scoped_refptr<net::IOBuffer> io_buffer);

 protected:
  virtual ~SocketReadFunction() {}

  // AsyncIOAPIFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  int socket_id_;
};

class SocketWriteFunction : public SocketExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.socket.write")

  SocketWriteFunction();
  void OnCompleted(int result);

 protected:
  virtual ~SocketWriteFunction();

  // AsyncIOAPIFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  int socket_id_;
  scoped_refptr<net::IOBufferWithSize> io_buffer_;
};

class SocketRecvFromFunction : public SocketExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.socket.recvFrom")

  void OnCompleted(int result,
                   scoped_refptr<net::IOBuffer> io_buffer,
                   const std::string& address,
                   int port);

 protected:
  virtual ~SocketRecvFromFunction();

  // AsyncIOAPIFunction
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  int socket_id_;
};

class SocketSendToFunction : public SocketExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.socket.sendTo")

  SocketSendToFunction();

 protected:
  virtual ~SocketSendToFunction();
  void OnCompleted(int result);

  // AsyncIOAPIFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  int socket_id_;
  scoped_refptr<net::IOBufferWithSize> io_buffer_;
  std::string address_;
  int port_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SOCKET_SOCKET_API_H_
