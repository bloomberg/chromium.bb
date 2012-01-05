// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SOCKET_SOCKET_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SOCKET_SOCKET_API_H_
#pragma once

#include "chrome/browser/extensions/extension_function.h"

#include <string>

namespace extensions {

class SocketController;

extern const char kBytesWrittenKey[];
extern const char kSocketIdKey[];
extern const char kUdpSocketType[];

class SocketApiFunction : public AsyncExtensionFunction {
 protected:
  // Set up for work. Guaranteed to happen on UI thread.
  virtual bool Prepare() = 0;

  // Do actual work. Guaranteed to happen on IO thread.
  virtual void Work() = 0;

  // Respond. Guaranteed to happen on UI thread.
  virtual bool Respond() = 0;

  virtual bool RunImpl() OVERRIDE;

  SocketController* controller();

 private:
  void WorkOnIOThread();
  void RespondOnUIThread();
};

// Many of these socket functions are synchronous in the sense that
// they don't involve blocking operations, but we've made them all
// AsyncExtensionFunctions because the underlying UDPClientSocket
// library wants all operations to happen on the same thread as the
// one that created the socket. Too bad.

class SocketCreateFunction : public SocketApiFunction {
 public:
  SocketCreateFunction();

 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  int src_id_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.socket.create")
};

class SocketDestroyFunction : public SocketApiFunction {
 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  int socket_id_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.socket.destroy")
};

class SocketConnectFunction : public SocketApiFunction {
 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  int socket_id_;
  std::string address_;
  int port_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.socket.connect")
};

class SocketCloseFunction : public SocketApiFunction {
 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  int socket_id_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.socket.close")
};

class SocketReadFunction : public SocketApiFunction {
 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  int socket_id_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.socket.read")
};

class SocketWriteFunction : public SocketApiFunction {
 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  int socket_id_;
  std::string message_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.socket.write")
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SOCKET_SOCKET_API_H_
