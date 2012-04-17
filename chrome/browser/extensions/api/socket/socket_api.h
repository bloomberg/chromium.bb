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

// Many of these socket functions are synchronous in the sense that
// they don't involve blocking operations, but we've made them all
// AsyncExtensionFunctions because the underlying UDPClientSocket
// library wants all operations to happen on the same thread as the
// one that created the socket. Too bad.

class SocketCreateFunction : public AsyncIOAPIFunction {
 public:
  SocketCreateFunction();

 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  enum SocketType {
    kSocketTypeInvalid = -1,
    kSocketTypeTCP,
    kSocketTypeUDP
  };

  int src_id_;
  SocketType socket_type_;
  std::string address_;
  int port_;
  APIResourceEventNotifier* event_notifier_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.socket.create")
};

class SocketDestroyFunction : public AsyncIOAPIFunction {
 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  int socket_id_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.socket.destroy")
};

class SocketConnectFunction : public AsyncIOAPIFunction {
 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  int socket_id_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.socket.connect")
};

class SocketDisconnectFunction : public AsyncIOAPIFunction {
 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  int socket_id_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.socket.disconnect")
};

class SocketReadFunction : public AsyncIOAPIFunction {
 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  int socket_id_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.socket.read")
};

class SocketWriteFunction : public AsyncIOAPIFunction {
 public:
  SocketWriteFunction();
  virtual ~SocketWriteFunction();

 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  int socket_id_;
  scoped_refptr<net::IOBufferWithSize> io_buffer_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.socket.write")
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SOCKET_SOCKET_API_H_
