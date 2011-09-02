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
#include "content/common/notification_service.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/web_socket_proxy_controller.h"
#endif

WebSocketProxyPrivateGetPassportForTCPFunction::
    WebSocketProxyPrivateGetPassportForTCPFunction() : is_finalized_(false) {
}

WebSocketProxyPrivateGetPassportForTCPFunction::
    ~WebSocketProxyPrivateGetPassportForTCPFunction() {
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
    if (!chromeos::WebSocketProxyController::IsInitiated()) {
      delay_response = true;
      registrar_.Add(
          this, chrome::NOTIFICATION_WEB_SOCKET_PROXY_STARTED,
          NotificationService::AllSources());
      chromeos::WebSocketProxyController::Initiate();
    }

    std::map<std::string, std::string> map;
    map["hostname"] = hostname;
    map["port"] = base::IntToString(port);
    map["extension_id"] = extension_id();
    StringValue* passport = Value::CreateStringValue(
        browser::InternalAuthGeneration::GeneratePassport(
            "web_socket_proxy", map));
    result_.reset(passport);
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

void WebSocketProxyPrivateGetPassportForTCPFunction::Observe(
    int type, const NotificationSource& source,
    const NotificationDetails& details) {
#if defined(OS_CHROMEOS)
  DCHECK(type == chrome::NOTIFICATION_WEB_SOCKET_PROXY_STARTED);
#else
  NOTREACHED();
#endif
  timer_.Stop();  // Cancel timeout timer.
  Finalize();
}

void WebSocketProxyPrivateGetPassportForTCPFunction::Finalize() {
  if (is_finalized_) {
    NOTREACHED();
    return;
  }
  is_finalized_ = true;
  SendResponse(true);
  Release();
}

