// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/network_message_observer.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/network/network_observer.h"
#include "ash/system/tray/system_tray.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/notifications/balloon_view_host_chromeos.h"
#include "chrome/browser/chromeos/notifications/system_notification.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/time_format.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Returns prefs::kShowPlanNotifications in the profile of the last active
// browser. If there is no active browser, returns true.
bool ShouldShowMobilePlanNotifications() {
  Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
  PrefService* prefs = profile->GetPrefs();
  return prefs->GetBoolean(prefs::kShowPlanNotifications);
}

}  // namespace

namespace chromeos {

class NetworkMessageNotification : public ash::NetworkTrayDelegate {
 public:
  NetworkMessageNotification(Profile* profile,
                             ash::NetworkObserver::MessageType error_type)
      : error_type_(error_type) {
    std::string id;
    int icon_id = 0;
    switch (error_type) {
      case ash::NetworkObserver::ERROR_CONNECT_FAILED:
        id = "network_connection.chromeos";
        icon_id = IDR_NOTIFICATION_NETWORK_FAILED;
        title_ = l10n_util::GetStringUTF16(IDS_NETWORK_CONNECTION_ERROR_TITLE);
        break;
      case ash::NetworkObserver::MESSAGE_DATA_LOW:
        id = "network_low_data.chromeos";
        icon_id = IDR_NOTIFICATION_BARS_CRITICAL;
        title_ = l10n_util::GetStringUTF16(IDS_NETWORK_LOW_DATA_TITLE);
        break;
      case ash::NetworkObserver::MESSAGE_DATA_NONE:
        id = "network_no_data.chromeos";
        icon_id = IDR_NOTIFICATION_BARS_EMPTY;
        title_ = l10n_util::GetStringUTF16(IDS_NETWORK_OUT_OF_DATA_TITLE);
        break;
      case ash::NetworkObserver::MESSAGE_DATA_PROMO:
        NOTREACHED();
        break;
    }
    DCHECK(!id.empty());
    if (CommandLine::ForCurrentProcess()->HasSwitch(
            ash::switches::kAshNotifyDisabled)) {
      system_notification_.reset(
          new SystemNotification(profile, id, icon_id, title_));
    }
  }

  // Overridden from ash::NetworkTrayDelegate:
  virtual void NotificationLinkClicked(size_t index) OVERRIDE {
    base::ListValue empty_value;
    if (!callback_.is_null())
      callback_.Run(&empty_value);
  }

  void Hide() {
    if (system_notification_.get()) {
      system_notification_->Hide();
    } else {
      ash::Shell::GetInstance()->system_tray()->network_observer()->
          ClearNetworkMessage(error_type_);
    }
  }

  void SetTitle(const string16& title) {
    title_ = title;
    if (system_notification_.get()) {
      system_notification_->set_title(title);
    }
  }

  void Show(const string16& message,
            const string16& link_text,
            const BalloonViewHost::MessageCallback& callback,
            bool urgent, bool sticky) {
    if (system_notification_.get()) {
      system_notification_->Show(message, link_text, callback, urgent, sticky);
    } else {
      callback_ = callback;
      std::vector<string16> links;
      links.push_back(link_text);
      ash::Shell::GetInstance()->system_tray()->network_observer()->
          SetNetworkMessage(this, error_type_, title_, message, links);
    }
  }

  void ShowAlways(const string16& message,
                  const string16& link_text,
                  const BalloonViewHost::MessageCallback& callback,
                  bool urgent, bool sticky) {
    if (system_notification_.get()) {
      // Hide if already shown to force show it in case user has closed it.
      if (system_notification_->visible())
        system_notification_->Hide();
    }
    Show(message, link_text, callback, urgent, sticky);
  }

 private:
  string16 title_;
  scoped_ptr<SystemNotification> system_notification_;
  ash::NetworkObserver::MessageType error_type_;
  BalloonViewHost::MessageCallback callback_;
};

NetworkMessageObserver::NetworkMessageObserver(Profile* profile) {
  notification_connection_error_.reset(
      new NetworkMessageNotification(
          profile, ash::NetworkObserver::ERROR_CONNECT_FAILED));
  notification_low_data_.reset(
      new NetworkMessageNotification(
          profile,
          ash::NetworkObserver::MESSAGE_DATA_LOW));
  notification_no_data_.reset(
      new NetworkMessageNotification(
          profile,
          ash::NetworkObserver::MESSAGE_DATA_NONE));
  NetworkLibrary* netlib = CrosLibrary::Get()->GetNetworkLibrary();
  OnNetworkManagerChanged(netlib);
  // Note that this gets added as a NetworkManagerObserver,
  // CellularDataPlanObserver, and UserActionObserver in
  // startup_browser_creator.cc
}

NetworkMessageObserver::~NetworkMessageObserver() {
  NetworkLibrary* netlib = CrosLibrary::Get()->GetNetworkLibrary();
  netlib->RemoveNetworkManagerObserver(this);
  netlib->RemoveCellularDataPlanObserver(this);
  netlib->RemoveUserActionObserver(this);
  notification_connection_error_->Hide();
  notification_low_data_->Hide();
  notification_no_data_->Hide();
}

// static
bool NetworkMessageObserver::IsApplicableBackupPlan(
    const CellularDataPlan* plan, const CellularDataPlan* other_plan) {
  // By applicable plan, we mean that the other plan has data AND the timeframe
  // will apply: (unlimited OR used bytes < max bytes) AND
  //   ((start time - 1 sec) <= end time of currently active plan).
  // In other words, there is data available and there is no gap of more than a
  // second in time between the old plan and the new plan.
  bool has_data = other_plan->plan_type == CELLULAR_DATA_PLAN_UNLIMITED ||
      other_plan->remaining_data() > 0;
  bool will_apply =
      (other_plan->plan_start_time - plan->plan_end_time).InSeconds() <= 1;
  return has_data && will_apply;
}

void NetworkMessageObserver::OpenMobileSetupPage(
    const std::string& service_path, const ListValue* args) {
  ash::Shell::GetInstance()->delegate()->OpenMobileSetup(service_path);
}

void NetworkMessageObserver::OpenMoreInfoPage(const ListValue* args) {
  Browser* browser = browser::FindOrCreateTabbedBrowser(
      ProfileManager::GetDefaultProfileOrOffTheRecord());
  chromeos::NetworkLibrary* lib =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  const chromeos::CellularNetwork* cellular = lib->cellular_network();
  if (!cellular)
    return;
  chrome::ShowSingletonTab(browser, GURL(cellular->payment_url()));
}

void NetworkMessageObserver::InitNewPlan(const CellularDataPlan* plan) {
  notification_low_data_->Hide();
  notification_no_data_->Hide();
  if (plan->plan_type == CELLULAR_DATA_PLAN_UNLIMITED) {
    notification_no_data_->SetTitle(
        l10n_util::GetStringFUTF16(IDS_NETWORK_DATA_EXPIRED_TITLE,
                                   ASCIIToUTF16(plan->plan_name)));
    notification_low_data_->SetTitle(
        l10n_util::GetStringFUTF16(IDS_NETWORK_NEARING_EXPIRATION_TITLE,
                                   ASCIIToUTF16(plan->plan_name)));
  } else {
    notification_no_data_->SetTitle(
        l10n_util::GetStringFUTF16(IDS_NETWORK_OUT_OF_DATA_TITLE,
                                   ASCIIToUTF16(plan->plan_name)));
    notification_low_data_->SetTitle(
        l10n_util::GetStringFUTF16(IDS_NETWORK_LOW_DATA_TITLE,
                                   ASCIIToUTF16(plan->plan_name)));
  }
}

void NetworkMessageObserver::ShowNeedsPlanNotification(
    const CellularNetwork* cellular) {
  notification_no_data_->SetTitle(
      l10n_util::GetStringFUTF16(IDS_NETWORK_NO_DATA_PLAN_TITLE,
                                 UTF8ToUTF16(cellular->name())));
  notification_no_data_->Show(
      l10n_util::GetStringFUTF16(
          IDS_NETWORK_NO_DATA_PLAN_MESSAGE,
          UTF8ToUTF16(cellular->name())),
      l10n_util::GetStringUTF16(IDS_NETWORK_PURCHASE_MORE_MESSAGE),
      base::Bind(&NetworkMessageObserver::OpenMobileSetupPage,
                 AsWeakPtr(),
                 cellular->service_path()),
      false, false);
}

void NetworkMessageObserver::ShowNoDataNotification(
    const CellularNetwork* cellular,
    CellularDataPlanType plan_type) {
  notification_low_data_->Hide();  // Hide previous low data notification.
  string16 message = plan_type == CELLULAR_DATA_PLAN_UNLIMITED ?
      TimeFormat::TimeRemaining(base::TimeDelta()) :
      l10n_util::GetStringFUTF16(IDS_NETWORK_DATA_REMAINING_MESSAGE,
                                 ASCIIToUTF16("0"));
  notification_no_data_->Show(message,
      l10n_util::GetStringUTF16(IDS_NETWORK_PURCHASE_MORE_MESSAGE),
      base::Bind(&NetworkMessageObserver::OpenMobileSetupPage,
                 AsWeakPtr(),
                 cellular->service_path()),
      false, false);
}

void NetworkMessageObserver::ShowLowDataNotification(
    const CellularDataPlan* plan) {
  string16 message;
  if (plan->plan_type == CELLULAR_DATA_PLAN_UNLIMITED) {
    message = plan->GetPlanExpiration();
  } else {
    int64 remaining_mbytes = plan->remaining_data() / (1024 * 1024);
    message = l10n_util::GetStringFUTF16(IDS_NETWORK_DATA_REMAINING_MESSAGE,
        UTF8ToUTF16(base::Int64ToString(remaining_mbytes)));
  }
  notification_low_data_->Show(message,
      l10n_util::GetStringUTF16(IDS_NETWORK_MORE_INFO_MESSAGE),
      base::Bind(&NetworkMessageObserver::OpenMoreInfoPage, AsWeakPtr()),
      false, false);
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
    notification_connection_error_->ShowAlways(
        l10n_util::GetStringFUTF16(
            IDS_NETWORK_CONNECTION_ERROR_MESSAGE_WITH_DETAILS,
            UTF8ToUTF16(new_failed_network->name()),
            UTF8ToUTF16(new_failed_network->GetErrorString())),
        string16(), BalloonViewHost::MessageCallback(), false, false);
  }
}

void NetworkMessageObserver::OnCellularDataPlanChanged(NetworkLibrary* cros) {
  if (!ShouldShowMobilePlanNotifications())
    return;
  const CellularNetwork* cellular = cros->cellular_network();
  if (!cellular || !cellular->SupportsDataPlan())
    return;

  const CellularDataPlanVector* plans =
      cros->GetDataPlans(cellular->service_path());
  // If no plans available, check to see if we need a new plan.
  if (!plans || plans->empty()) {
    // If previously, we had low data, we know that a plan was near expiring.
    // In that case, because the plan disappeared, we assume that it expired.
    if (cellular_data_left_ == CellularNetwork::DATA_LOW) {
      ShowNoDataNotification(cellular, cellular_data_plan_type_);
    } else if (cellular->needs_new_plan()) {
      ShowNeedsPlanNotification(cellular);
    }
    SaveLastCellularInfo(cellular, NULL);
    return;
  }

  CellularDataPlanVector::const_iterator iter = plans->begin();
  const CellularDataPlan* current_plan = *iter;

  // If current plan is not the last plan (there is another backup plan),
  // then we do not show notifications for this plan.
  // For example, if there is another data plan available when this runs out.
  for (++iter; iter != plans->end(); ++iter) {
    if (IsApplicableBackupPlan(current_plan, *iter)) {
      SaveLastCellularInfo(cellular, current_plan);
      return;
    }
  }

  // If connected cellular network changed, or data plan is different, then
  // it's a new network. Then hide all previous notifications.
  bool new_plan = cellular->service_path() != cellular_service_path_ ||
      current_plan->GetUniqueIdentifier() != cellular_data_plan_unique_id_;

  if (new_plan) {
    InitNewPlan(current_plan);
  }

  if (cellular->data_left() == CellularNetwork::DATA_NONE) {
    ShowNoDataNotification(cellular, current_plan->plan_type);
  } else if (cellular->data_left() == CellularNetwork::DATA_VERY_LOW) {
    // Only show low data notification if we transition to very low data
    // and we are on the same plan. This is so that users don't get a
    // notification whenever they connect to a low data 3g network.
    if (!new_plan && (cellular_data_left_ != CellularNetwork::DATA_VERY_LOW))
      ShowLowDataNotification(current_plan);
  }

  SaveLastCellularInfo(cellular, current_plan);
}

void NetworkMessageObserver::OnConnectionInitiated(NetworkLibrary* cros,
                                                   const Network* network) {
  // If user initiated any network connection, we hide the error notification.
  notification_connection_error_->Hide();
}

void NetworkMessageObserver::SaveLastCellularInfo(
    const CellularNetwork* cellular, const CellularDataPlan* plan) {
  DCHECK(cellular);
  cellular_service_path_ = cellular->service_path();
  cellular_data_left_ = cellular->data_left();
  if (plan) {
    cellular_data_plan_unique_id_ = plan->GetUniqueIdentifier();
    cellular_data_plan_type_ = plan->plan_type;
  } else {
    cellular_data_plan_unique_id_ = std::string();
    cellular_data_plan_type_ = CELLULAR_DATA_PLAN_UNKNOWN;
  }
}

}  // namespace chromeos
