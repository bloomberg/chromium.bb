// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/web_socket_proxy_controller.h"

#include <algorithm>

#include <netinet/in.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "base/string_tokenizer.h"
#include "base/threading/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/web_socket_proxy.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "net/base/network_change_notifier.h"

namespace {

class ProxyLifetime
    : public net::NetworkChangeNotifier::OnlineStateObserver,
      public content::NotificationObserver {
 public:
  ProxyLifetime()
      : delay_ms_(1000),
        port_(-1),
        shutdown_requested_(false),
        web_socket_proxy_thread_("Chrome_WebSocketproxyThread") {
    DLOG(INFO) << "WebSocketProxyController initiation";
    base::Thread::Options options(MessageLoop::TYPE_IO, 0);
    web_socket_proxy_thread_.StartWithOptions(options);
    web_socket_proxy_thread_.message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&ProxyLifetime::ProxyCallback, base::Unretained(this)));
    net::NetworkChangeNotifier::AddOnlineStateObserver(this);
    registrar_.Add(
        this, chrome::NOTIFICATION_WEB_SOCKET_PROXY_STARTED,
        content::NotificationService::AllSources());
  }

  virtual ~ProxyLifetime() {
    net::NetworkChangeNotifier::RemoveOnlineStateObserver(this);
  }

  virtual void Observe(int type, const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    base::AutoLock alk(lock_);
    port_ = *content::Details<int>(details).ptr();
  }

  int GetPort() {
    base::AutoLock alk(lock_);
    return port_;
  }

 private:
  // net::NetworkChangeNotifier::OnlineStateObserver implementation.
  virtual void OnOnlineStateChanged(bool online) OVERRIDE {
    DCHECK(chromeos::WebSocketProxyController::IsInitiated());
    base::AutoLock alk(lock_);
    if (server_)
      server_->OnNetworkChange();
  }

  void ProxyCallback() {
    LOG(INFO) << "Attempt to run web socket proxy task";
    chromeos::WebSocketProxy* server = new chromeos::WebSocketProxy();
    {
      base::AutoLock alk(lock_);
      if (shutdown_requested_)
        return;
      delete server_;
      server_ = server;
    }
    server->Run();
    {
      base::AutoLock alk(lock_);
      delete server;
      server_ = NULL;
      if (!shutdown_requested_) {
        // Proxy terminated unexpectedly or failed to start (it can happen due
        // to a network problem). Keep trying.
        if (delay_ms_ < 100 * 1000)
          (delay_ms_ *= 3) /= 2;

        MessageLoop::current()->PostDelayedTask(
            FROM_HERE,
            base::Bind(&ProxyLifetime::ProxyCallback, base::Unretained(this)),
            base::TimeDelta::FromMilliseconds(delay_ms_));
      }
    }
  }

  // Delay in milliseconds between next attempt to run proxy.
  int volatile delay_ms_;

  // Proxy listens for incoming websocket connections on this port.
  int volatile port_;

  chromeos::WebSocketProxy* volatile server_;
  volatile bool shutdown_requested_;
  base::Lock lock_;
  content::NotificationRegistrar registrar_;
  friend class chromeos::WebSocketProxyController;
  base::Thread web_socket_proxy_thread_;
};

base::LazyInstance<ProxyLifetime> g_proxy_lifetime = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace chromeos {

// static
void WebSocketProxyController::Initiate() {
  g_proxy_lifetime.Get();
}

// static
bool WebSocketProxyController::IsInitiated() {
  return !(g_proxy_lifetime == NULL);
}

// static
int WebSocketProxyController::GetPort() {
  int port = g_proxy_lifetime.Get().GetPort();
  DCHECK(IsInitiated());
  return port;
}

// static
void WebSocketProxyController::Shutdown() {
  if (!IsInitiated())
    return;

  DLOG(INFO) << "WebSocketProxyController shutdown";
  {
    base::AutoLock alk(g_proxy_lifetime.Get().lock_);
    g_proxy_lifetime.Get().shutdown_requested_ = true;
    if (g_proxy_lifetime.Get().server_)
      g_proxy_lifetime.Get().server_->Shutdown();
  }
  g_proxy_lifetime.Get().web_socket_proxy_thread_.Stop();
}

}  // namespace chromeos
