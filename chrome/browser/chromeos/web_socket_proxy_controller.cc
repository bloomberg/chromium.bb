// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web_socket_proxy_controller.h"

#include <netinet/in.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/web_socket_proxy.h"
#include "content/browser/browser_thread.h"

namespace {

class ProxyTask : public Task {
  virtual void Run() OVERRIDE;
};

struct ProxyLifetime {
  ProxyLifetime() : delay_ms(1000), shutdown_requested(false) {
    BrowserThread::PostTask(
        BrowserThread::WEB_SOCKET_PROXY, FROM_HERE, new ProxyTask());
  }

  // Delay between next attempt to run proxy.
  int delay_ms;

  chromeos::WebSocketProxy* volatile server;

  volatile bool shutdown_requested;

  base::Lock shutdown_lock;
};

base::LazyInstance<ProxyLifetime> g_proxy_lifetime(base::LINKER_INITIALIZED);

void ProxyTask::Run() {
  LOG(INFO) << "Attempt to run web socket proxy task";
  const int kPort = 10101;

  // Configure allowed origins. Empty vector allows any origin.
  std::vector<std::string> allowed_origins;
  allowed_origins.push_back(
      "chrome-extension://haiffjcadagjlijoggckpgfnoeiflnem");

  struct sockaddr_in sa;
  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_port = htons(kPort);
  sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  chromeos::WebSocketProxy* server = new chromeos::WebSocketProxy(
      allowed_origins, reinterpret_cast<sockaddr*>(&sa), sizeof(sa));
  {
    base::AutoLock alk(g_proxy_lifetime.Get().shutdown_lock);
    if (g_proxy_lifetime.Get().shutdown_requested)
      return;
    delete g_proxy_lifetime.Get().server;
    g_proxy_lifetime.Get().server = server;
  }
  server->Run();
  {
    base::AutoLock alk(g_proxy_lifetime.Get().shutdown_lock);
    delete server;
    g_proxy_lifetime.Get().server = NULL;
    if (!g_proxy_lifetime.Get().shutdown_requested) {
      // Proxy terminated unexpectedly or failed to start (it can happen due to
      // a network problem). Keep trying.
      if (g_proxy_lifetime.Get().delay_ms < 100 * 1000)
        (g_proxy_lifetime.Get().delay_ms *= 3) /= 2;
      BrowserThread::PostDelayedTask(
          BrowserThread::WEB_SOCKET_PROXY, FROM_HERE, new ProxyTask(),
          g_proxy_lifetime.Get().delay_ms);
    }
  }
}

}  // namespace

namespace chromeos {

// static
void WebSocketProxyController::Initiate() {
  LOG(INFO) << "WebSocketProxyController initiation";
  g_proxy_lifetime.Get();
}

// static
bool WebSocketProxyController::IsInitiated() {
  return !(g_proxy_lifetime == NULL);
}

// static
void WebSocketProxyController::Shutdown() {
  if (IsInitiated()) {
    LOG(INFO) << "WebSocketProxyController shutdown";
    base::AutoLock alk(g_proxy_lifetime.Get().shutdown_lock);
    g_proxy_lifetime.Get().shutdown_requested = true;
    if (g_proxy_lifetime.Get().server)
      g_proxy_lifetime.Get().server->Shutdown();
  }
}

}  // namespace chromeos

