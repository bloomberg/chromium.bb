// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEVTOOLS_REMOTE_H_
#define CHROME_BROWSER_DEBUGGER_DEVTOOLS_REMOTE_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"

class DevToolsRemoteMessage;
class ListenSocket;

// This interface should be implemented by a class that wants to handle
// DevToolsRemoteMessages dispatched by some entity. It must extend
class DevToolsRemoteListener
    : public base::RefCountedThreadSafe<DevToolsRemoteListener> {
 public:
  DevToolsRemoteListener() {}
  virtual void HandleMessage(const DevToolsRemoteMessage& message) = 0;
  // This method is invoked on the UI thread whenever the debugger connection
  // has been lost.
  virtual void OnConnectionLost() = 0;
  virtual void OnAcceptConnection(ListenSocket* connection) {}

 protected:
  friend class base::RefCountedThreadSafe<DevToolsRemoteListener>;

  virtual ~DevToolsRemoteListener() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DevToolsRemoteListener);
};

// Interface exposed by DevToolsProtocolHandler to receive reply messages
// from registered tools.
class OutboundSocketDelegate {
 public:
  virtual ~OutboundSocketDelegate() {}
  virtual void Send(const DevToolsRemoteMessage& message) = 0;
};

#endif  // CHROME_BROWSER_DEBUGGER_DEVTOOLS_REMOTE_H_
