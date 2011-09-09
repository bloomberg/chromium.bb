// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/web_socket_proxy_controller.h"

#include <algorithm>

#include <netinet/in.h>
#include <sys/wait.h>
#include <unistd.h>

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
#include "content/browser/browser_thread.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_service.h"
#include "content/common/url_constants.h"
#include "googleurl/src/gurl.h"

namespace {

const char* kAllowedIds[] = {
    "haiffjcadagjlijoggckpgfnoeiflnem",
    "gnedhmakppccajfpfiihfcdlnpgomkcf",
    "fjcibdnjlbfnbfdjneajpipnlcppleek",
    "okddffdblfhhnmhodogpojmfkjmhinfp"
};

class OriginValidator {
 public:
  OriginValidator() {
    chromeos::FillWithExtensionsIdsWithPrivateAccess(&allowed_ids_);
    CommandLine* command_line = CommandLine::ForCurrentProcess();
    DCHECK(command_line);
    std::string allowed_list =
        command_line->GetSwitchValueASCII(switches::kAllowWebSocketProxy);
    if (!allowed_list.empty()) {
      StringTokenizer t(allowed_list, ",");
      while (t.GetNext()) {
        // It must be either extension id or origin.
        if (Extension::IdIsValid(t.token())) {
          allowed_ids_.push_back(t.token());
        } else {
          // It is not extension id, check if it is an origin.
          GURL origin = GURL(t.token()).GetOrigin();
          if (!origin.is_valid()) {
            LOG(ERROR) << "Invalid extension id or origin specified via "
                       << switches::kAllowWebSocketProxy << " switch";
            break;
          }
          allowed_origins_.push_back(origin.spec());
          if (origin.SchemeIs(chrome::kExtensionScheme))
            allowed_ids_.push_back(origin.host());
        }
      }
    }
    for (size_t i = 0; i < allowed_ids_.size(); ++i) {
      allowed_origins_.push_back(Extension::GetBaseURLFromExtensionId(
          allowed_ids_[i]).GetOrigin().spec());
    }
    std::sort(allowed_ids_.begin(), allowed_ids_.end());
    allowed_ids_.resize(std::unique(
        allowed_ids_.begin(), allowed_ids_.end()) - allowed_ids_.begin());
    std::sort(allowed_origins_.begin(), allowed_origins_.end());
    allowed_origins_.resize(std::unique(allowed_origins_.begin(),
        allowed_origins_.end()) - allowed_origins_.begin());
  }

  bool CheckCredentials(
      const std::string& extension_id,
      const std::string& hostname,
      unsigned short port,
      chromeos::WebSocketProxyController::ConnectionFlags flags) {
    if (flags & chromeos::WebSocketProxyController::TLS_OVER_TCP) {
      NOTIMPLEMENTED();
      return false;
    }
    return std::binary_search(
        allowed_ids_.begin(), allowed_ids_.end(), extension_id);
  }

  const std::vector<std::string>& allowed_origins() { return allowed_origins_; }

 private:
  std::vector<std::string> allowed_ids_;
  std::vector<std::string> allowed_origins_;
};

base::LazyInstance<OriginValidator> g_validator(base::LINKER_INITIALIZED);

class ProxyTask : public Task {
  virtual void Run() OVERRIDE;
};

class ProxyLifetime : public NotificationObserver {
 public:
  ProxyLifetime() : delay_ms_(1000), shutdown_requested_(false) {
    BrowserThread::PostTask(
        BrowserThread::WEB_SOCKET_PROXY, FROM_HERE, new ProxyTask());
    registrar_.Add(this, chrome::NOTIFICATION_NETWORK_STATE_CHANGED,
                   NotificationService::AllSources());
  }

 private:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    DCHECK_EQ(type, chrome::NOTIFICATION_NETWORK_STATE_CHANGED);
    DCHECK(chromeos::WebSocketProxyController::IsInitiated());
    base::AutoLock alk(lock_);
    if (server_)
      server_->OnNetworkChange();
  }

  // Delay between next attempt to run proxy.
  int delay_ms_;

  chromeos::WebSocketProxy* volatile server_;
  volatile bool shutdown_requested_;
  base::Lock lock_;
  NotificationRegistrar registrar_;
  friend class ProxyTask;
  friend class chromeos::WebSocketProxyController;
};

base::LazyInstance<ProxyLifetime> g_proxy_lifetime(base::LINKER_INITIALIZED);

void ProxyTask::Run() {
  LOG(INFO) << "Attempt to run web socket proxy task";
  const int kPort = 10101;

  struct sockaddr_in sa;
  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_port = htons(kPort);
  sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  chromeos::WebSocketProxy* server = new chromeos::WebSocketProxy(
      g_validator.Get().allowed_origins(),
      reinterpret_cast<sockaddr*>(&sa), sizeof(sa));
  {
    base::AutoLock alk(g_proxy_lifetime.Get().lock_);
    if (g_proxy_lifetime.Get().shutdown_requested_)
      return;
    delete g_proxy_lifetime.Get().server_;
    g_proxy_lifetime.Get().server_ = server;
  }
  server->Run();
  {
    base::AutoLock alk(g_proxy_lifetime.Get().lock_);
    delete server;
    g_proxy_lifetime.Get().server_ = NULL;
    if (!g_proxy_lifetime.Get().shutdown_requested_) {
      // Proxy terminated unexpectedly or failed to start (it can happen due to
      // a network problem). Keep trying.
      if (g_proxy_lifetime.Get().delay_ms_ < 100 * 1000)
        (g_proxy_lifetime.Get().delay_ms_ *= 3) /= 2;
      BrowserThread::PostDelayedTask(
          BrowserThread::WEB_SOCKET_PROXY, FROM_HERE, new ProxyTask(),
          g_proxy_lifetime.Get().delay_ms_);
    }
  }
}

}  // namespace

namespace chromeos {

void FillWithExtensionsIdsWithPrivateAccess(std::vector<std::string>* ids) {
  ids->clear();
  for (size_t i = 0; i < arraysize(kAllowedIds); ++i)
    ids->push_back(kAllowedIds[i]);
}

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
    base::AutoLock alk(g_proxy_lifetime.Get().lock_);
    g_proxy_lifetime.Get().shutdown_requested_ = true;
    if (g_proxy_lifetime.Get().server_)
      g_proxy_lifetime.Get().server_->Shutdown();
  }
}

// static
bool WebSocketProxyController::CheckCredentials(
    const std::string& extension_id,
    const std::string& hostname,
    unsigned short port,
    ConnectionFlags flags) {
  return g_validator.Get().CheckCredentials(
      extension_id, hostname, port, flags);
}

}  // namespace chromeos
