// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_web_socket_proxy_private_api.h"

#include "base/logging.h"
#include "base/values.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/internal_auth.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_details.h"
#include "net/base/escape.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/web_socket_proxy_controller.h"
#endif

namespace {
const char kPermissionDeniedError[] =
    "Extension does not have permission to use this method.";
}

WebSocketProxyPrivate::WebSocketProxyPrivate()
    : is_finalized_(false), listening_port_(-1) {
}

WebSocketProxyPrivate::~WebSocketProxyPrivate() {
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
  Finalize();
}

void WebSocketProxyPrivate::Finalize() {
  if (is_finalized_)
    return;
  is_finalized_ = true;
  SendResponse(listening_port_ > 0);
  Release();
}

void WebSocketProxyPrivateGetURLForTCPFunction::Observe(
    int type, const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  listening_port_ = *content::Details<int>(details).ptr();
  WebSocketProxyPrivate::Observe(type, source, details);
}

void WebSocketProxyPrivateGetURLForTCPFunction::Finalize() {
#if defined(OS_CHROMEOS)
  if (listening_port_ < 1)
    listening_port_ = chromeos::WebSocketProxyController::GetPort();
  StringValue* url = Value::CreateStringValue(std::string(
      "ws://127.0.0.1:" + base::IntToString(listening_port_) +
      "/tcpproxy?" + query_));
  result_.reset(url);
#endif
  WebSocketProxyPrivate::Finalize();
}

bool WebSocketProxyPrivateGetURLForTCPFunction::RunImpl() {
  AddRef();
  bool delay_response = false;
  result_.reset(Value::CreateStringValue(""));

#if defined(OS_CHROMEOS)
  std::string hostname;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &hostname));
  int port = -1;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(1, &port));
  bool do_tls = false;
  DictionaryValue* qualification = NULL;
  if (args_->GetDictionary(2, &qualification)) {
    const char kTlsOption[] = "tls";
    if (qualification->HasKey(kTlsOption)) {
      EXTENSION_FUNCTION_VALIDATE(qualification->GetBoolean(
          kTlsOption, &do_tls));
    }
  }
  if (chromeos::WebSocketProxyController::CheckCredentials(
          extension_id(), hostname, port,
          do_tls ? chromeos::WebSocketProxyController::TLS_OVER_TCP :
              chromeos::WebSocketProxyController::PLAIN_TCP)) {
    listening_port_ = chromeos::WebSocketProxyController::GetPort();
    if (listening_port_ < 1) {
      delay_response = true;
      registrar_.Add(
          this, chrome::NOTIFICATION_WEB_SOCKET_PROXY_STARTED,
          content::NotificationService::AllSources());
    }

    std::map<std::string, std::string> map;
    map["hostname"] = hostname;
    map["port"] = base::IntToString(port);
    map["extension_id"] = extension_id();
    map["tls"] = do_tls ? "true" : "false";
    std::string passport = browser::InternalAuthGeneration::GeneratePassport(
        "web_socket_proxy", map);
    query_ = std::string("hostname=") +
        net::EscapeQueryParamValue(hostname, false) + "&port=" + map["port"] +
        "&tls=" + map["tls"] + "&passport=" +
        net::EscapeQueryParamValue(passport, false);
  } else {
    error_ = kPermissionDeniedError;
    return false;
  }
#endif  // defined(OS_CHROMEOS)

  if (delay_response) {
    const int kTimeout = 12;
    timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(kTimeout),
        this, &WebSocketProxyPrivate::Finalize);
  } else {
    Finalize();
  }
  return true;
}

WebSocketProxyPrivateGetPassportForTCPFunction::
    WebSocketProxyPrivateGetPassportForTCPFunction() {
  // This obsolete API uses fixed port to listen websocket connections.
  listening_port_ = 10101;
}

bool WebSocketProxyPrivateGetPassportForTCPFunction::RunImpl() {
  AddRef();
  bool delay_response = false;
  result_.reset(Value::CreateStringValue(""));

#if defined(OS_CHROMEOS)
  std::string hostname;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &hostname));
  int port = -1;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(1, &port));

  if (chromeos::WebSocketProxyController::CheckCredentials(
          extension_id(), hostname, port,
          chromeos::WebSocketProxyController::PLAIN_TCP)) {
    listening_port_ = chromeos::WebSocketProxyController::GetPort();
    if (listening_port_ < 1) {
      delay_response = true;
      registrar_.Add(
          this, chrome::NOTIFICATION_WEB_SOCKET_PROXY_STARTED,
          content::NotificationService::AllSources());
    }

    std::map<std::string, std::string> map;
    map["hostname"] = hostname;
    map["port"] = base::IntToString(port);
    map["extension_id"] = extension_id();
    StringValue* passport = Value::CreateStringValue(
        browser::InternalAuthGeneration::GeneratePassport(
            "web_socket_proxy", map));
    result_.reset(passport);
  } else {
    error_ = kPermissionDeniedError;
    return false;
  }
#endif  // defined(OS_CHROMEOS)

  if (delay_response) {
    const int kTimeout = 3;
    timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(kTimeout),
        this, &WebSocketProxyPrivateGetPassportForTCPFunction::Finalize);
  } else {
    Finalize();
  }
  return true;
}

