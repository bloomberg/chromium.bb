// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider_observers.h"

#include "base/values.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/authentication_notification_details.h"
#include "content/common/notification_service.h"

using chromeos::CrosLibrary;
using chromeos::NetworkLibrary;

NetworkManagerInitObserver::NetworkManagerInitObserver(
    AutomationProvider* automation)
    : automation_(automation->AsWeakPtr()) {}

NetworkManagerInitObserver::~NetworkManagerInitObserver() {
    CrosLibrary::Get()->GetNetworkLibrary()->RemoveNetworkManagerObserver(this);
}

bool NetworkManagerInitObserver::Init() {
  if (!CrosLibrary::Get()->EnsureLoaded()) {
    // If cros library fails to load, don't wait for the network
    // library to finish initializing, because it'll wait forever.
    automation_->OnNetworkLibraryInit();
    return false;
  }

  CrosLibrary::Get()->GetNetworkLibrary()->AddNetworkManagerObserver(this);
  return true;
}

void NetworkManagerInitObserver::OnNetworkManagerChanged(NetworkLibrary* obj) {
  if (!obj->wifi_scanning()) {
    automation_->OnNetworkLibraryInit();
    delete this;
  }
}

LoginManagerObserver::LoginManagerObserver(
    AutomationProvider* automation,
    IPC::Message* reply_message)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message) {
  registrar_.Add(this, NotificationType::LOGIN_USER_CHANGED,
                 NotificationService::AllSources());
}

LoginManagerObserver::~LoginManagerObserver() {}

void LoginManagerObserver::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  DCHECK(type == NotificationType::LOGIN_USER_CHANGED);

  if (!automation_) {
    delete this;
    return;
  }

  AutomationJSONReply reply(automation_, reply_message_.release());
  Details<AuthenticationNotificationDetails> auth_details(details);
  if (auth_details->success())
    reply.SendSuccess(NULL);
  else
    reply.SendError("Login failure.");
  delete this;
}

ScreenLockUnlockObserver::ScreenLockUnlockObserver(
    AutomationProvider* automation,
    IPC::Message* reply_message,
    bool lock_screen)
    : automation_(automation),
      reply_message_(reply_message),
      lock_screen_(lock_screen) {
  registrar_.Add(this, NotificationType::SCREEN_LOCK_STATE_CHANGED,
                 NotificationService::AllSources());
}

ScreenLockUnlockObserver::~ScreenLockUnlockObserver() {}

void ScreenLockUnlockObserver::Observe(NotificationType type,
                                       const NotificationSource& source,
                                       const NotificationDetails& details) {
  DCHECK(type == NotificationType::SCREEN_LOCK_STATE_CHANGED);
  AutomationJSONReply reply(automation_, reply_message_);
  bool is_screen_locked = *Details<bool>(details).ptr();
  if (lock_screen_ == is_screen_locked)
    reply.SendSuccess(NULL);
  else
    reply.SendError("Screen lock failure.");
  delete this;
}

NetworkScanObserver::NetworkScanObserver(AutomationProvider* automation,
                                         IPC::Message* reply_message)
    : automation_(automation), reply_message_(reply_message) {
  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();
  network_library->AddNetworkManagerObserver(this);
}

NetworkScanObserver::~NetworkScanObserver() {
  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();
  network_library->RemoveNetworkManagerObserver(this);
}

void NetworkScanObserver::OnNetworkManagerChanged(NetworkLibrary* obj) {
  if (obj->wifi_scanning())
    return;

  AutomationJSONReply(automation_, reply_message_).SendSuccess(NULL);
  delete this;
}

NetworkConnectObserver::NetworkConnectObserver(AutomationProvider* automation,
                                               IPC::Message* reply_message,
                                               const std::string& service_path)
    : automation_(automation),
    reply_message_(reply_message),
    service_path_(service_path) {
  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();
  network_library->AddNetworkManagerObserver(this);
}

NetworkConnectObserver::~NetworkConnectObserver() {
  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();
  network_library->RemoveNetworkManagerObserver(this);
}

void NetworkConnectObserver::OnNetworkManagerChanged(NetworkLibrary* obj) {
  AutomationJSONReply reply(automation_, reply_message_);
  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();
  chromeos::WifiNetwork* wifi =
      network_library->FindWifiNetworkByPath(service_path_);
  if (!wifi)
    return;
  if (wifi->failed()) {
    scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
    return_value->SetString("error_code", wifi->GetErrorString());
    reply.SendSuccess(return_value.get());
    delete this;
  }
  if (wifi->connected()) {
    reply.SendSuccess(NULL);
    delete this;
  }
}

