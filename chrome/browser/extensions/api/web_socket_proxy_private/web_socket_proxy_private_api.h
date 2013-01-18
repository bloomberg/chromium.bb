// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEB_SOCKET_PROXY_PRIVATE_WEB_SOCKET_PROXY_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEB_SOCKET_PROXY_PRIVATE_WEB_SOCKET_PROXY_PRIVATE_API_H_

#include "base/timer.h"
#include "chrome/browser/extensions/extension_function.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "net/base/address_list.h"

class IOThread;

namespace net {
class SingleRequestHostResolver;
}

namespace extensions {

// Base class for web socket proxy functions.
class WebSocketProxyPrivate
    : public AsyncExtensionFunction, public content::NotificationObserver {
 public:
  WebSocketProxyPrivate();

 protected:
  virtual ~WebSocketProxyPrivate();

  // Custom finalization.
  virtual void CustomFinalize() = 0;

  // ExtensionFunction implementation.
  virtual bool RunImpl() OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(
      int type, const content::NotificationSource& source,
      const content::NotificationDetails& details) OVERRIDE;

  // Destination hostname.
  std::string hostname_;
  // Destination IP address.
  net::AddressList addr_;
  // Destination port.
  int port_;
  // Proxy accepts websocket connections on this port.
  int listening_port_;
  // Whether TLS should be used.
  bool do_tls_;
  // Requested parameters of connection.
  std::map<std::string, std::string> map_;

 private:
  // Finalizes and sends respond. Overwrite 'CustomFinalize' in inherited
  // classes.
  void Finalize();

  // Callback for DNS resolution.
  void OnHostResolution(int result);

  // Posts task to the IO thread, which will make dns resolution.
  void ResolveHost();
  // Executes on IO thread. Performs DNS resolution.
  void ResolveHostIOPart(IOThread* io_thread);

  // Used to signal timeout (when waiting for proxy initial launch).
  base::OneShotTimer<WebSocketProxyPrivate> timer_;
  // Used to register for notifications.
  content::NotificationRegistrar registrar_;
  // Used to cancel host resolution when out of scope.
  scoped_ptr<net::SingleRequestHostResolver> resolver_;
  // Callback which is called when host is resolved.
  bool is_finalized_;
};

// New API function for web socket proxy, which should be used.
class WebSocketProxyPrivateGetURLForTCPFunction
    : public WebSocketProxyPrivate {
 public:
  DECLARE_EXTENSION_FUNCTION("webSocketProxyPrivate.getURLForTCP",
                             WEBSOCKETPROXYPRIVATE_GETURLFORTCP)

  WebSocketProxyPrivateGetURLForTCPFunction();

 protected:
  virtual ~WebSocketProxyPrivateGetURLForTCPFunction();

  // ExtensionFunction implementation.
  virtual bool RunImpl() OVERRIDE;

  // WebSocketProxyPrivate implementation:
  virtual void CustomFinalize() OVERRIDE;
};

// Legacy API function for web socket proxy, to be eliminated.
class WebSocketProxyPrivateGetPassportForTCPFunction
    : public WebSocketProxyPrivate {
 public:
  DECLARE_EXTENSION_FUNCTION("webSocketProxyPrivate.getPassportForTCP",
                             WEBSOCKETPROXYPRIVATE_GETPASSPORTFORTCP)

  WebSocketProxyPrivateGetPassportForTCPFunction();

 protected:
  virtual ~WebSocketProxyPrivateGetPassportForTCPFunction();

  // WebSocketProxyPrivate implementation:
  virtual void CustomFinalize() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEB_SOCKET_PROXY_PRIVATE_WEB_SOCKET_PROXY_PRIVATE_API_H_
