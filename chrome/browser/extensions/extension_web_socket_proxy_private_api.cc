// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_web_socket_proxy_private_api.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/internal_auth.h"
#include "chrome/browser/io_thread.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_details.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/base/single_request_host_resolver.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/web_socket_proxy_controller.h"
#endif

WebSocketProxyPrivate::WebSocketProxyPrivate()
    : port_(-1),
      listening_port_(-1),
      do_tls_(false),
      is_finalized_(false) {
}

WebSocketProxyPrivate::~WebSocketProxyPrivate() {
}

void WebSocketProxyPrivate::Finalize() {
  CustomFinalize();

  if (is_finalized_)
    return;
  is_finalized_ = true;
  SendResponse(listening_port_ > 0);
  Release();
}

void WebSocketProxyPrivate::Observe(
    int type, const content::NotificationSource& source,
    const content::NotificationDetails& details) {
#if defined(OS_CHROMEOS)
  DCHECK_EQ(chrome::NOTIFICATION_WEB_SOCKET_PROXY_STARTED, type);
#else
  NOTREACHED();
#endif

  timer_.Stop();  // Cancel timeout timer.
  ResolveHost();
}

void WebSocketProxyPrivate::ResolveHost() {
#if defined(OS_CHROMEOS)
  IOThread* io_thread = g_browser_process->io_thread();
  content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
      base::Bind(&WebSocketProxyPrivate::ResolveHostIOPart, this, io_thread));
#endif
}

void WebSocketProxyPrivate::ResolveHostIOPart(IOThread* io_thread) {
#if defined(OS_CHROMEOS)
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  DCHECK(resolver_ == NULL);
  if (io_thread && io_thread->globals()) {
    net::HostResolver* host_resolver =
        io_thread->globals()->host_resolver.get();
    if (host_resolver) {
      resolver_.reset(new net::SingleRequestHostResolver(host_resolver));
      net::HostResolver::RequestInfo info(net::HostPortPair(
          hostname_, port_));
      int result = resolver_->Resolve(info, &addr_,
          base::Bind(&WebSocketProxyPrivate::OnHostResolution, this),
          net::BoundNetLog());
      if (result != net::ERR_IO_PENDING)
        OnHostResolution(result);
      return;
    }
  }
  NOTREACHED();
  OnHostResolution(net::ERR_UNEXPECTED);
#endif
}

bool WebSocketProxyPrivate::RunImpl() {
  AddRef();
  result_.reset(Value::CreateStringValue(""));

#if defined(OS_CHROMEOS)
  bool delay_response = false;

  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &hostname_));
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(1, &port_));

  listening_port_ = chromeos::WebSocketProxyController::GetPort();
  if (listening_port_ < 1) {
    delay_response = true;
    registrar_.Add(
        this, chrome::NOTIFICATION_WEB_SOCKET_PROXY_STARTED,
        content::NotificationService::AllSources());
  }
  map_["hostname"] = hostname_;
  map_["port"] = base::IntToString(port_);
  map_["extension_id"] = extension_id();

  if (delay_response) {
    const int kTimeout = 12;
    timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(kTimeout),
        this, &WebSocketProxyPrivate::ResolveHost);
  } else {
    ResolveHost();
  }
#else
  Finalize();
#endif

  return true;
}

void WebSocketProxyPrivate::OnHostResolution(int result) {
#if defined(OS_CHROMEOS)
  if (result == 0 && !addr_.empty()) {
    std::string ip = addr_.front().ToStringWithoutPort();
    if (!ip.empty() && ip.find(':') != std::string::npos && ip[0] != '[')
      ip = '[' + ip + ']';
    map_["addr"] = ip;
  }
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&WebSocketProxyPrivate::Finalize, this));
#endif
}

WebSocketProxyPrivateGetURLForTCPFunction::
    WebSocketProxyPrivateGetURLForTCPFunction() {
}

WebSocketProxyPrivateGetURLForTCPFunction::
    ~WebSocketProxyPrivateGetURLForTCPFunction() {
}

void WebSocketProxyPrivateGetURLForTCPFunction::CustomFinalize() {
#if defined(OS_CHROMEOS)
  std::string passport = browser::InternalAuthGeneration::GeneratePassport(
      "web_socket_proxy", map_);
  std::string query = std::string("hostname=") +
      net::EscapeQueryParamValue(hostname_, false) + "&port=" + map_["port"] +
      "&tls=" + map_["tls"] + "&passport=" +
      net::EscapeQueryParamValue(passport, false);
  if (ContainsKey(map_, "addr"))
    query += std::string("&addr=") + map_["addr"];

  if (listening_port_ < 1)
    listening_port_ = chromeos::WebSocketProxyController::GetPort();
  StringValue* url = Value::CreateStringValue(std::string(
      "ws://127.0.0.1:" + base::IntToString(listening_port_) +
      "/tcpproxy?" + query));
  result_.reset(url);
#endif
}

bool WebSocketProxyPrivateGetURLForTCPFunction::RunImpl() {
#if defined(OS_CHROMEOS)
  DictionaryValue* qualification = NULL;
  if (args_->GetDictionary(2, &qualification)) {
    const char kTlsOption[] = "tls";
    if (qualification->HasKey(kTlsOption)) {
      EXTENSION_FUNCTION_VALIDATE(qualification->GetBoolean(
          kTlsOption, &do_tls_));
    }
  }
  map_["tls"] = do_tls_ ? "true" : "false";
#endif

  return WebSocketProxyPrivate::RunImpl();
}

WebSocketProxyPrivateGetPassportForTCPFunction::
    WebSocketProxyPrivateGetPassportForTCPFunction() {
  // This obsolete API uses fixed port to listen websocket connections.
  listening_port_ = 10101;
}

WebSocketProxyPrivateGetPassportForTCPFunction::
    ~WebSocketProxyPrivateGetPassportForTCPFunction() {
}

void WebSocketProxyPrivateGetPassportForTCPFunction::CustomFinalize() {
#if defined(OS_CHROMEOS)
  std::string passport =
      browser::InternalAuthGeneration::GeneratePassport(
          "web_socket_proxy", map_) + std::string(":");
  if (ContainsKey(map_, "addr"))
    passport += map_["addr"];
  result_.reset(Value::CreateStringValue(passport));
#endif
}
