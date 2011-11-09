// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WEB_SOCKET_PROXY_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WEB_SOCKET_PROXY_PRIVATE_API_H_
#pragma once

#include "base/timer.h"
#include "chrome/browser/extensions/extension_function.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class WebSocketProxyPrivate
    : public AsyncExtensionFunction, public content::NotificationObserver {
 public:
  WebSocketProxyPrivate();

  virtual ~WebSocketProxyPrivate();

  // Finalizes async operation.
  virtual void Finalize();

 protected:
  // content::NotificationObserver implementation.
  virtual void Observe(
      int type, const content::NotificationSource& source,
      const content::NotificationDetails& details) OVERRIDE;

  // Whether already finalized.
  bool is_finalized_;

  // Used to signal timeout (when waiting for proxy initial launch).
  base::OneShotTimer<WebSocketProxyPrivate> timer_;

  content::NotificationRegistrar registrar_;

  // Proxy accepts websocket connections on this port.
  int listening_port_;
};

class WebSocketProxyPrivateGetURLForTCPFunction
    : public WebSocketProxyPrivate {
 private:
  // ExtensionFunction implementation.
  virtual bool RunImpl() OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(
      int type, const content::NotificationSource& source,
      const content::NotificationDetails& details) OVERRIDE;

  // Finalizes async operation.
  virtual void Finalize() OVERRIDE;

  // Query component of resulting URL.
  std::string query_;

  DECLARE_EXTENSION_FUNCTION_NAME("webSocketProxyPrivate.getURLForTCP")
};

// Legacy, deprecated, to be eliminated.
class WebSocketProxyPrivateGetPassportForTCPFunction
    : public WebSocketProxyPrivate {
 public:
  WebSocketProxyPrivateGetPassportForTCPFunction();

 private:
  // ExtensionFunction implementation.
  virtual bool RunImpl() OVERRIDE;

  DECLARE_EXTENSION_FUNCTION_NAME("webSocketProxyPrivate.getPassportForTCP")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WEB_SOCKET_PROXY_PRIVATE_API_H_
