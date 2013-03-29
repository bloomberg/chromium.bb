// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider_observers.h"

#include "base/values.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/authentication_notification_details.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_screen_actor.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/user_image.h"
#include "chrome/browser/chromeos/login/user_image_manager.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"

using chromeos::CrosLibrary;
using chromeos::NetworkLibrary;
using chromeos::WizardController;

namespace {

// Fake screen name for the user session (reported by WizardControllerObserver).
const char kSessionScreenName[] = "session";

}

NetworkManagerInitObserver::NetworkManagerInitObserver(
    AutomationProvider* automation)
    : automation_(automation->AsWeakPtr()) {
}

NetworkManagerInitObserver::~NetworkManagerInitObserver() {
    CrosLibrary::Get()->GetNetworkLibrary()->RemoveNetworkManagerObserver(this);
}

bool NetworkManagerInitObserver::Init() {
  if (!CrosLibrary::Get()->libcros_loaded()) {
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
    if (automation_)
      automation_->OnNetworkLibraryInit();
    delete this;
  }
}

OOBEWebuiReadyObserver::OOBEWebuiReadyObserver(AutomationProvider* automation)
    : automation_(automation->AsWeakPtr()) {
  if (WizardController::default_controller() &&
      WizardController::default_controller()->current_screen()) {
    OOBEWebuiReady();
  } else {
    registrar_.Add(this, chrome::NOTIFICATION_LOGIN_WEBUI_VISIBLE,
                   content::NotificationService::AllSources());
  }
}

void OOBEWebuiReadyObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_LOGIN_WEBUI_VISIBLE);
  OOBEWebuiReady();
}

void OOBEWebuiReadyObserver::OOBEWebuiReady() {
  if (automation_)
    automation_->OnOOBEWebuiReady();
  delete this;
}

LoginObserver::LoginObserver(chromeos::ExistingUserController* controller,
                             AutomationProvider* automation,
                             IPC::Message* reply_message)
    : controller_(controller),
      automation_(automation->AsWeakPtr()),
      reply_message_(reply_message) {
  controller_->set_login_status_consumer(this);
}

LoginObserver::~LoginObserver() {
  controller_->set_login_status_consumer(NULL);
}

void LoginObserver::OnLoginFailure(const chromeos::LoginFailure& error) {
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  return_value->SetString("error_string", error.GetErrorString());
  AutomationJSONReply(automation_, reply_message_.release())
      .SendSuccess(return_value.get());
  delete this;
}

void LoginObserver::OnLoginSuccess(
    const chromeos::UserContext& user_context,
    bool pending_requests,
    bool using_oauth) {
  controller_->set_login_status_consumer(NULL);
  AutomationJSONReply(automation_, reply_message_.release()).SendSuccess(NULL);
  delete this;
}

WizardControllerObserver::WizardControllerObserver(
    WizardController* wizard_controller,
    AutomationProvider* automation,
    IPC::Message* reply_message)
    : wizard_controller_(wizard_controller),
      automation_(automation->AsWeakPtr()),
      reply_message_(reply_message) {
  wizard_controller_->AddObserver(this);
  registrar_.Add(this, chrome::NOTIFICATION_LOGIN_WEBUI_LOADED,
                 content::NotificationService::AllSources());
}

WizardControllerObserver::~WizardControllerObserver() {
  wizard_controller_->RemoveObserver(this);
}

void WizardControllerObserver::OnScreenChanged(
    chromeos::WizardScreen* next_screen) {
  std::string screen_name = next_screen->GetName();
  if (screen_to_wait_for_.empty() || screen_to_wait_for_ == screen_name) {
    SendReply(screen_name);
  } else {
    DVLOG(2) << "Still waiting for " << screen_to_wait_for_;
  }
}

void WizardControllerObserver::OnSessionStart() {
  SendReply(kSessionScreenName);
}

void WizardControllerObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_LOGIN_WEBUI_LOADED);
  SendReply(WizardController::kLoginScreenName);
}

void WizardControllerObserver::SendReply(const std::string& screen_name) {
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  return_value->SetString("next_screen", screen_name);
  AutomationJSONReply(automation_, reply_message_.release())
      .SendSuccess(return_value.get());
  delete this;
}

ScreenLockUnlockObserver::ScreenLockUnlockObserver(
    AutomationProvider* automation,
    IPC::Message* reply_message,
    bool lock_screen)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      lock_screen_(lock_screen) {
  registrar_.Add(this, chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
                 content::NotificationService::AllSources());
}

ScreenLockUnlockObserver::~ScreenLockUnlockObserver() {}

void ScreenLockUnlockObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED);
  if (automation_) {
    AutomationJSONReply reply(automation_, reply_message_.release());
    bool is_screen_locked = *content::Details<bool>(details).ptr();
    if (lock_screen_ == is_screen_locked)
      reply.SendSuccess(NULL);
    else
      reply.SendError("Screen lock failure.");
  }
  delete this;
}

ScreenUnlockObserver::ScreenUnlockObserver(AutomationProvider* automation,
                                           IPC::Message* reply_message)
    : ScreenLockUnlockObserver(automation, reply_message, false) {
  chromeos::ScreenLocker::default_screen_locker()->SetLoginStatusConsumer(this);
}

ScreenUnlockObserver::~ScreenUnlockObserver() {
  chromeos::ScreenLocker* screen_locker =
      chromeos::ScreenLocker::default_screen_locker();
  if (screen_locker)
    screen_locker->SetLoginStatusConsumer(NULL);
}

void ScreenUnlockObserver::OnLoginFailure(const chromeos::LoginFailure& error) {
  if (automation_) {
    scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
    return_value->SetString("error_string", error.GetErrorString());
    AutomationJSONReply(automation_, reply_message_.release())
        .SendSuccess(return_value.get());
  }
  delete this;
}

NetworkScanObserver::NetworkScanObserver(AutomationProvider* automation,
                                         IPC::Message* reply_message)
    : automation_(automation->AsWeakPtr()), reply_message_(reply_message) {
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

  if (automation_) {
    AutomationJSONReply(automation_,
                        reply_message_.release()).SendSuccess(NULL);
  }
  delete this;
}

ToggleNetworkDeviceObserver::ToggleNetworkDeviceObserver(
    AutomationProvider* automation, IPC::Message* reply_message,
    const std::string& device, bool enable)
    : automation_(automation->AsWeakPtr()), reply_message_(reply_message),
      device_(device), enable_(enable) {
  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();
  network_library->AddNetworkManagerObserver(this);
}

ToggleNetworkDeviceObserver::~ToggleNetworkDeviceObserver() {
  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();
  network_library->RemoveNetworkManagerObserver(this);
}

void ToggleNetworkDeviceObserver::OnNetworkManagerChanged(NetworkLibrary* obj) {
  if ((device_ == "ethernet" && enable_ == obj->ethernet_enabled()) ||
      (device_ == "wifi" && enable_ == obj->wifi_enabled()) ||
      (device_ == "cellular" && enable_ == obj->cellular_enabled())) {
    if (automation_)
      AutomationJSONReply(automation_,
                          reply_message_.release()).SendSuccess(NULL);
    delete this;
  }
}

NetworkStatusObserver::NetworkStatusObserver(AutomationProvider* automation,
                                               IPC::Message* reply_message)
    : automation_(automation->AsWeakPtr()), reply_message_(reply_message) {
  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();
  network_library->AddNetworkManagerObserver(this);
}

NetworkStatusObserver::~NetworkStatusObserver() {
  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();
  network_library->RemoveNetworkManagerObserver(this);
}

void NetworkStatusObserver::OnNetworkManagerChanged(NetworkLibrary* obj) {
  const chromeos::Network* network = GetNetwork(obj);
  if (!network) {
    // The network was not found, and we assume it no longer exists.
    // This could be because the ssid is invalid, or the network went away.
    if (automation_) {
      scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
      return_value->SetString("error_string", "Network not found.");
      AutomationJSONReply(automation_, reply_message_.release())
          .SendSuccess(return_value.get());
    }
    delete this;
    return;
  }

  NetworkStatusCheck(network);
}

NetworkConnectObserver::NetworkConnectObserver(
    AutomationProvider* automation, IPC::Message* reply_message)
    : NetworkStatusObserver(automation, reply_message) {}

void NetworkConnectObserver::NetworkStatusCheck(const chromeos::Network*
                                                network) {
  if (network->failed()) {
    if (automation_) {
      scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
      return_value->SetString("error_string", network->GetErrorString());
      AutomationJSONReply(automation_, reply_message_.release())
          .SendSuccess(return_value.get());
    }
    delete this;
  } else if (network->connected()) {
    if (automation_)
      AutomationJSONReply(automation_,
                          reply_message_.release()).SendSuccess(NULL);
    delete this;
  }

  // The network is in the NetworkLibrary's list, but there's no failure or
  // success condition, so just continue waiting for more network events.
}

NetworkDisconnectObserver::NetworkDisconnectObserver(
    AutomationProvider* automation, IPC::Message* reply_message,
    const std::string& service_path)
    : NetworkStatusObserver(automation, reply_message),
      service_path_(service_path) {}

void NetworkDisconnectObserver::NetworkStatusCheck(const chromeos::Network*
                                                   network) {
  if (!network->connected()) {
      AutomationJSONReply(automation_, reply_message_.release()).SendSuccess(
                                                                 NULL);
      delete this;
  }
}

const chromeos::Network* NetworkDisconnectObserver::GetNetwork(
    NetworkLibrary* network_library) {
  return network_library->FindNetworkByPath(service_path_);
}

ServicePathConnectObserver::ServicePathConnectObserver(
    AutomationProvider* automation, IPC::Message* reply_message,
    const std::string& service_path)
    : NetworkConnectObserver(automation, reply_message),
      service_path_(service_path) {}

const chromeos::Network* ServicePathConnectObserver::GetNetwork(
    NetworkLibrary* network_library) {
  return network_library->FindNetworkByPath(service_path_);
}

SSIDConnectObserver::SSIDConnectObserver(
    AutomationProvider* automation, IPC::Message* reply_message,
    const std::string& ssid)
    : NetworkConnectObserver(automation, reply_message), ssid_(ssid) {}

const chromeos::Network* SSIDConnectObserver::GetNetwork(
    NetworkLibrary* network_library) {
  const chromeos::WifiNetworkVector& wifi_networks =
      network_library->wifi_networks();
  for (chromeos::WifiNetworkVector::const_iterator iter = wifi_networks.begin();
       iter != wifi_networks.end(); ++iter) {
    const chromeos::WifiNetwork* wifi = *iter;
    if (wifi->name() == ssid_)
      return wifi;
  }
  return NULL;
}

VirtualConnectObserver::VirtualConnectObserver(AutomationProvider* automation,
                                               IPC::Message* reply_message,
                                               const std::string& service_name)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      service_name_(service_name) {
  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();
  network_library->AddNetworkManagerObserver(this);
}

VirtualConnectObserver::~VirtualConnectObserver() {
  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();
  network_library->RemoveNetworkManagerObserver(this);
}

void VirtualConnectObserver::OnNetworkManagerChanged(NetworkLibrary* cros) {
  const chromeos::VirtualNetwork* virt = GetVirtualNetwork(cros);
  if (!virt) {
    // The network hasn't been added to the NetworkLibrary's list yet,
    // just continue waiting for more network events.
    return;
  }

  if (virt->failed()) {
    if (automation_) {
      scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
      return_value->SetString("error_string", virt->GetErrorString());
      AutomationJSONReply(automation_, reply_message_.release())
          .SendSuccess(return_value.get());
    }
    delete this;
  } else if (virt->connected()) {
    if (automation_)
      AutomationJSONReply(automation_,
                          reply_message_.release()).SendSuccess(NULL);
    delete this;
  }
  // The network is in the NetworkLibrary's list, but there's no failure or
  // success condition, so just continue waiting for more network events.
}

chromeos::VirtualNetwork* VirtualConnectObserver::GetVirtualNetwork(
    const chromeos::NetworkLibrary* cros) {
  chromeos::VirtualNetwork* virt = NULL;
  const chromeos::VirtualNetworkVector& virtual_networks =
      cros->virtual_networks();

  for (chromeos::VirtualNetworkVector::const_iterator iter =
       virtual_networks.begin(); iter != virtual_networks.end(); ++iter) {
    chromeos::VirtualNetwork* v = *iter;
    if (v->name() == service_name_) {
      virt = v;
      break;
    }
  }
  return virt;
}

EnrollmentObserver::EnrollmentObserver(AutomationProvider* automation,
    IPC::Message* reply_message,
    chromeos::EnterpriseEnrollmentScreen* enrollment_screen)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      enrollment_screen_(enrollment_screen) {
  enrollment_screen_->AddTestingObserver(this);
}

EnrollmentObserver::~EnrollmentObserver() {}

void EnrollmentObserver::OnEnrollmentComplete(bool succeeded) {
  enrollment_screen_->RemoveTestingObserver(this);
  if (automation_) {
    if (succeeded) {
      AutomationJSONReply(automation_,
                          reply_message_.release()).SendSuccess(NULL);
    } else {
      scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
      return_value->SetString("error_string", "Enrollment failed.");
      AutomationJSONReply(automation_, reply_message_.release())
          .SendSuccess(return_value.get());
    }
  }
  delete this;
}
