// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SOCKET_SOCKET_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SOCKET_SOCKET_API_H_
#pragma once

#include "chrome/browser/extensions/extension_function.h"

#include <string>

namespace extensions {

extern const char kBytesWrittenKey[];
extern const char kSocketIdKey[];
extern const char kUdpSocketType[];

// Many of these socket functions are synchronous in the sense that
// they don't involve blocking operations, but we've made them all
// AsyncExtensionFunctions because the underlying UDPClientSocket
// library wants all operations to happen on the same thread as the
// one that created the socket. Too bad.

class SocketCreateFunction : public AsyncExtensionFunction {
 public:
  SocketCreateFunction();
  virtual ~SocketCreateFunction();
  virtual bool RunImpl() OVERRIDE;

 protected:
  void WorkOnIOThread();
  void RespondOnUIThread();

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.socket.create")
};

class SocketDestroyFunction : public AsyncExtensionFunction {
 public:
  SocketDestroyFunction();
  virtual ~SocketDestroyFunction();
  virtual bool RunImpl() OVERRIDE;

 protected:
  void WorkOnIOThread();
  void RespondOnUIThread();

 private:
  int socket_id_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.socket.destroy")
};

class SocketConnectFunction : public AsyncExtensionFunction {
 public:
  SocketConnectFunction();
  virtual ~SocketConnectFunction();
  virtual bool RunImpl() OVERRIDE;

 protected:
  void WorkOnIOThread();
  void RespondOnUIThread();

 private:
  int socket_id_;
  std::string address_;
  int port_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.socket.connect")
};

class SocketCloseFunction : public AsyncExtensionFunction {
 public:
  SocketCloseFunction();
  virtual ~SocketCloseFunction();
  virtual bool RunImpl() OVERRIDE;

 protected:
  void WorkOnIOThread();
  void RespondOnUIThread();

 private:
  int socket_id_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.socket.close")
};

class SocketWriteFunction : public AsyncExtensionFunction {
 public:
  SocketWriteFunction();
  virtual ~SocketWriteFunction();
  virtual bool RunImpl() OVERRIDE;

 protected:
  void WorkOnIOThread();
  void RespondOnUIThread();

  int socket_id_;
  std::string message_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.socket.write")
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SOCKET_SOCKET_API_H_
