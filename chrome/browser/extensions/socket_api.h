// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SOCKET_API_H_
#define CHROME_BROWSER_EXTENSIONS_SOCKET_API_H_
#pragma once

#include "chrome/browser/extensions/extension_function.h"

namespace extensions {

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

class SocketConnectFunction : public AsyncExtensionFunction {
 public:
  SocketConnectFunction();
  virtual ~SocketConnectFunction();
  virtual bool RunImpl() OVERRIDE;

 protected:
  void WorkOnIOThread();
  void RespondOnUIThread();

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.socket.connect")
};

class SocketDisconnectFunction : public AsyncExtensionFunction {
 public:
  SocketDisconnectFunction();
  virtual ~SocketDisconnectFunction();
  virtual bool RunImpl() OVERRIDE;

 protected:
  void WorkOnIOThread();
  void RespondOnUIThread();

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.socket.disconnect")
};

class SocketSendFunction : public AsyncExtensionFunction {
 public:
  SocketSendFunction();
  virtual ~SocketSendFunction();
  virtual bool RunImpl() OVERRIDE;

 protected:
  void WorkOnIOThread();
  void RespondOnUIThread();

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.socket.send")
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SOCKET_API_H_
