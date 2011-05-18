// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WEB_SOCKET_PROXY_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WEB_SOCKET_PROXY_PRIVATE_API_H_
#pragma once

#include "base/timer.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "chrome/browser/extensions/extension_function.h"

class WebSocketProxyPrivateGetPassportForTCPFunction
    : public AsyncExtensionFunction, public NotificationObserver {
 public:
  WebSocketProxyPrivateGetPassportForTCPFunction();

  virtual ~WebSocketProxyPrivateGetPassportForTCPFunction();

 private:
  // ExtensionFunction implementation.
  virtual bool RunImpl() OVERRIDE;

  // NotificationObserver implementation.
  virtual void Observe(
      NotificationType type, const NotificationSource& source,
      const NotificationDetails& details) OVERRIDE;

  // Finalizes async operation.
  void Finalize();

  // Used to signal timeout (when waiting for proxy initial launch).
  base::OneShotTimer<WebSocketProxyPrivateGetPassportForTCPFunction> timer_;

  NotificationRegistrar registrar_;

  DECLARE_EXTENSION_FUNCTION_NAME("webSocketProxyPrivate.getPassportForTCP")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WEB_SOCKET_PROXY_PRIVATE_API_H_
