// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/network_message_observer.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/chromeos/network/network_observer.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/notifications/balloon_view_host_chromeos.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/time_format.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
ash::NetworkObserver::NetworkType GetAshNetworkType(
    chromeos::ConnectionType connection_type) {
  switch (connection_type) {
   case chromeos::TYPE_CELLULAR:
     return ash::NetworkObserver::NETWORK_CELLULAR;
   case chromeos::TYPE_ETHERNET:
     return ash::NetworkObserver::NETWORK_ETHERNET;
   case chromeos::TYPE_WIFI:
     return ash::NetworkObserver::NETWORK_WIFI;
   case chromeos::TYPE_BLUETOOTH:
     return ash::NetworkObserver::NETWORK_BLUETOOTH;
   default:
     return ash::NetworkObserver::NETWORK_UNKNOWN;
  }
}

}  // namespace

namespace chromeos {

class NetworkMessageNotification : public ash::NetworkTrayDelegate {
 public:
  NetworkMessageNotification(Profile* profile,
                             ash::NetworkObserver::MessageType error_type)
      : error_type_(error_type) {
    switch (error_type) {
      case ash::NetworkObserver::ERROR_CONNECT_FAILED:
        title_ = l10n_util::GetStringUTF16(IDS_NETWORK_CONNECTION_ERROR_TITLE);
        return;
      case ash::NetworkObserver::MESSAGE_DATA_PROMO:
        NOTREACHED();
        return;
    }
    NOTREACHED();
  }

  // Overridden from ash::NetworkTrayDelegate:
  virtual void NotificationLinkClicked(size_t index) OVERRIDE {
    base::ListValue empty_value;
    if (!callback_.is_null())
      callback_.Run(&empty_value);
  }

  void Hide() {
    ash::Shell::GetInstance()->system_tray_notifier()->
        NotifyClearNetworkMessage(error_type_);
  }

  void SetTitle(const string16& title) {
    title_ = title;
  }

  void Show(chromeos::ConnectionType connection_type,
            const string16& message,
            const string16& link_text,
            const BalloonViewHost::MessageCallback& callback,
            bool urgent, bool sticky) {
    callback_ = callback;
    std::vector<string16> links;
    links.push_back(link_text);
    ash::NetworkObserver::NetworkType network_type = GetAshNetworkType(
        connection_type);
    ash::Shell::GetInstance()->system_tray_notifier()->NotifySetNetworkMessage(
        this, error_type_, network_type, title_, message, links);
  }

  void ShowAlways(chromeos::ConnectionType connection_type,
                  const string16& message,
                  const string16& link_text,
                  const BalloonViewHost::MessageCallback& callback,
                  bool urgent, bool sticky) {
    Show(connection_type, message, link_text, callback, urgent, sticky);
  }

 private:
  string16 title_;
  ash::NetworkObserver::MessageType error_type_;
  BalloonViewHost::MessageCallback callback_;
};

NetworkMessageObserver::NetworkMessageObserver(Profile* profile) {
  notification_connection_error_.reset(
      new NetworkMessageNotification(
          profile, ash::NetworkObserver::ERROR_CONNECT_FAILED));
  NetworkLibrary* netlib = CrosLibrary::Get()->GetNetworkLibrary();
  OnNetworkManagerChanged(netlib);
  // Note that this gets added as a NetworkManagerObserver
  // and UserActionObserver in startup_browser_creator.cc
}

NetworkMessageObserver::~NetworkMessageObserver() {
  NetworkLibrary* netlib = CrosLibrary::Get()->GetNetworkLibrary();
  netlib->RemoveNetworkManagerObserver(this);
  netlib->RemoveUserActionObserver(this);
  notification_connection_error_->Hide();
}

void NetworkMessageObserver::OnNetworkManagerChanged(NetworkLibrary* cros) {
  const Network* new_failed_network = NULL;
  // Check to see if we have any newly failed networks.
  for (WifiNetworkVector::const_iterator it = cros->wifi_networks().begin();
       it != cros->wifi_networks().end(); it++) {
    const WifiNetwork* net = *it;
    if (net->notify_failure()) {
      new_failed_network = net;
      break;  // There should only be one failed network.
    }
  }

  if (!new_failed_network) {
    for (WimaxNetworkVector::const_iterator it = cros->wimax_networks().begin();
         it != cros->wimax_networks().end(); ++it) {
      const WimaxNetwork* net = *it;
      if (net->notify_failure()) {
        new_failed_network = net;
        break;  // There should only be one failed network.
      }
    }
  }

  if (!new_failed_network) {
    for (CellularNetworkVector::const_iterator it =
             cros->cellular_networks().begin();
         it != cros->cellular_networks().end(); it++) {
      const CellularNetwork* net = *it;
      if (net->notify_failure()) {
        new_failed_network = net;
        break;  // There should only be one failed network.
      }
    }
  }

  if (!new_failed_network) {
    for (VirtualNetworkVector::const_iterator it =
             cros->virtual_networks().begin();
         it != cros->virtual_networks().end(); it++) {
      const VirtualNetwork* net = *it;
      if (net->notify_failure()) {
        new_failed_network = net;
        break;  // There should only be one failed network.
      }
    }
  }

  // Show connection error notification if necessary.
  if (new_failed_network) {
    VLOG(1) << "Failed Network: " << new_failed_network->service_path();
    notification_connection_error_->ShowAlways(
        new_failed_network->type(),
        l10n_util::GetStringFUTF16(
            IDS_NETWORK_CONNECTION_ERROR_MESSAGE_WITH_DETAILS,
            UTF8ToUTF16(new_failed_network->name()),
            UTF8ToUTF16(new_failed_network->GetErrorString())),
        string16(), BalloonViewHost::MessageCallback(), false, false);
  }
}

void NetworkMessageObserver::OnConnectionInitiated(NetworkLibrary* cros,
                                                   const Network* network) {
  // If user initiated any network connection, we hide the error notification.
  notification_connection_error_->Hide();
}

}  // namespace chromeos
