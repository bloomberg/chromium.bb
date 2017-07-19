// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/tether_notification_presenter.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/grit/generated_resources.h"
#include "components/proximity_auth/logging/logging.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/notifier_settings.h"

namespace chromeos {

namespace tether {

namespace {

// Mean value of NetworkStat's signal_strength() range.
const int kMediumSignalStrength = 50;

// Dimensions of Tether notification icon in pixels.
constexpr gfx::Size kTetherSignalIconSize(40, 40);

const char kTetherSettingsSubpage[] = "networks?type=Tether";

class SettingsUiDelegateImpl
    : public TetherNotificationPresenter::SettingsUiDelegate {
 public:
  SettingsUiDelegateImpl() {}
  ~SettingsUiDelegateImpl() override {}

  void ShowSettingsSubPageForProfile(Profile* profile,
                                     const std::string& sub_page) override {
    chrome::ShowSettingsSubPageForProfile(profile, sub_page);
  }
};

// Gets the normalized signal strength that can range from 0 to 4. This return
// value is then used to get an appropriate image to display on the
// notification.  Defaults to full signal strength (4) if |signal_strength|
// is out of bounds.
int GetNormalizedSignalStrength(int signal_strength) {
  int normalized_signal_strength = signal_strength / 25;
  return std::min(std::max(normalized_signal_strength, 0), 4);
}

}  // namespace

// static
constexpr const char TetherNotificationPresenter::kTetherNotifierId[] =
    "cros_tether_notification_ids.notifier_id";

// static
constexpr const char TetherNotificationPresenter::kActiveHostNotificationId[] =
    "cros_tether_notification_ids.active_host";

// static
constexpr const char
    TetherNotificationPresenter::kPotentialHotspotNotificationId[] =
        "cros_tether_notification_ids.potential_hotspot";

// static
constexpr const char
    TetherNotificationPresenter::kSetupRequiredNotificationId[] =
        "cros_tether_notification_ids.setup_required";

// static
constexpr const std::array<const char* const, 4>
    TetherNotificationPresenter::kIdsWhichOpenSettingsOnClick{
        {TetherNotificationPresenter::kTetherNotifierId,
         TetherNotificationPresenter::kActiveHostNotificationId,
         TetherNotificationPresenter::kPotentialHotspotNotificationId,
         TetherNotificationPresenter::kSetupRequiredNotificationId}};

// static
std::unique_ptr<message_center::Notification>
TetherNotificationPresenter::CreateNotificationWithMediumSignalStrengthIcon(
    const std::string& id,
    const base::string16& title,
    const base::string16& message) {
  return CreateNotification(id, title, message,
                            message_center::RichNotificationData(),
                            kMediumSignalStrength);
}

// static
std::unique_ptr<message_center::Notification>
TetherNotificationPresenter::CreateNotification(
    const std::string& id,
    const base::string16& title,
    const base::string16& message,
    const message_center::RichNotificationData rich_notification_data,
    int signal_strength) {
  auto source = base::MakeUnique<ash::network_icon::SignalStrengthImageSource>(
      ash::network_icon::BARS, gfx::kGoogleBlue500, kTetherSignalIconSize,
      GetNormalizedSignalStrength(signal_strength));
  return base::MakeUnique<message_center::Notification>(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE, id, title,
      message,
      gfx::Image(gfx::ImageSkia(std::move(source), kTetherSignalIconSize)),
      base::string16() /* display_source */, GURL() /* origin_url */,
      message_center::NotifierId(
          message_center::NotifierId::NotifierType::SYSTEM_COMPONENT,
          kTetherNotifierId),
      rich_notification_data, nullptr);
}

TetherNotificationPresenter::TetherNotificationPresenter(
    Profile* profile,
    message_center::MessageCenter* message_center,
    NetworkConnect* network_connect)
    : profile_(profile),
      message_center_(message_center),
      network_connect_(network_connect),
      settings_ui_delegate_(base::WrapUnique(new SettingsUiDelegateImpl())),
      weak_ptr_factory_(this) {
  message_center_->AddObserver(this);
}

TetherNotificationPresenter::~TetherNotificationPresenter() {
  message_center_->RemoveObserver(this);
}

void TetherNotificationPresenter::NotifyPotentialHotspotNearby(
    const cryptauth::RemoteDevice& remote_device,
    int signal_strength) {
  PA_LOG(INFO) << "Displaying \"potential hotspot nearby\" notification for "
               << "device with name \"" << remote_device.name << "\". "
               << "Notification ID = " << kPotentialHotspotNotificationId;

  hotspot_nearby_device_ = remote_device;

  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_TETHER_NOTIFICATION_WIFI_AVAILABLE_ONE_DEVICE_CONNECT)));
  ShowNotification(CreateNotification(
      std::string(kPotentialHotspotNotificationId),
      l10n_util::GetStringUTF16(
          IDS_TETHER_NOTIFICATION_WIFI_AVAILABLE_ONE_DEVICE_TITLE),
      l10n_util::GetStringFUTF16(
          IDS_TETHER_NOTIFICATION_WIFI_AVAILABLE_ONE_DEVICE_MESSAGE,
          base::ASCIIToUTF16(hotspot_nearby_device_.name)),
      rich_notification_data, signal_strength));
}

void TetherNotificationPresenter::NotifyMultiplePotentialHotspotsNearby() {
  PA_LOG(INFO) << "Displaying \"potential hotspot nearby\" notification for "
               << "multiple devices. Notification ID = "
               << kPotentialHotspotNotificationId;

  ShowNotification(CreateNotificationWithMediumSignalStrengthIcon(
      std::string(kPotentialHotspotNotificationId),
      l10n_util::GetStringUTF16(
          IDS_TETHER_NOTIFICATION_WIFI_AVAILABLE_MULTIPLE_DEVICES_TITLE),
      l10n_util::GetStringUTF16(
          IDS_TETHER_NOTIFICATION_WIFI_AVAILABLE_MULTIPLE_DEVICES_MESSAGE)));
}

void TetherNotificationPresenter::RemovePotentialHotspotNotification() {
  PA_LOG(INFO) << "Removing \"potential hotspot nearby\" dialog. "
               << "Notification ID = " << kPotentialHotspotNotificationId;

  message_center_->RemoveNotification(
      std::string(kPotentialHotspotNotificationId), false /* by_user */);
}

void TetherNotificationPresenter::NotifySetupRequired(
    const std::string& device_name) {
  PA_LOG(INFO) << "Displaying \"setup required\" notification. Notification "
               << "ID = " << kSetupRequiredNotificationId;

  ShowNotification(CreateNotificationWithMediumSignalStrengthIcon(
      std::string(kSetupRequiredNotificationId),
      l10n_util::GetStringFUTF16(IDS_TETHER_NOTIFICATION_SETUP_REQUIRED_TITLE,
                                 base::ASCIIToUTF16(device_name)),
      l10n_util::GetStringFUTF16(IDS_TETHER_NOTIFICATION_SETUP_REQUIRED_MESSAGE,
                                 base::ASCIIToUTF16(device_name))));
}

void TetherNotificationPresenter::RemoveSetupRequiredNotification() {
  PA_LOG(INFO) << "Removing \"setup required\" dialog. "
               << "Notification ID = " << kSetupRequiredNotificationId;

  message_center_->RemoveNotification(std::string(kSetupRequiredNotificationId),
                                      false /* by_user */);
}

void TetherNotificationPresenter::NotifyConnectionToHostFailed() {
  PA_LOG(INFO) << "Displaying \"connection attempt failed\" notification. "
               << "Notification ID = " << kActiveHostNotificationId;

  ShowNotification(CreateNotificationWithMediumSignalStrengthIcon(
      std::string(kActiveHostNotificationId),
      l10n_util::GetStringUTF16(
          IDS_TETHER_NOTIFICATION_CONNECTION_FAILED_TITLE),
      l10n_util::GetStringUTF16(
          IDS_TETHER_NOTIFICATION_CONNECTION_FAILED_MESSAGE)));
}

void TetherNotificationPresenter::RemoveConnectionToHostFailedNotification() {
  PA_LOG(INFO) << "Removing \"connection attempt failed\" dialog. "
               << "Notification ID = " << kActiveHostNotificationId;

  message_center_->RemoveNotification(std::string(kActiveHostNotificationId),
                                      false /* by_user */);
}

void TetherNotificationPresenter::OnNotificationClicked(
    const std::string& notification_id) {
  auto* const* const it = std::find_if(
      kIdsWhichOpenSettingsOnClick.begin(), kIdsWhichOpenSettingsOnClick.end(),
      [notification_id](const char* id) { return notification_id == id; });
  if (it == kIdsWhichOpenSettingsOnClick.end()) {
    return;
  }

  settings_ui_delegate_->ShowSettingsSubPageForProfile(profile_,
                                                       kTetherSettingsSubpage);
  message_center_->RemoveNotification(notification_id, true /* by_user */);
}

void TetherNotificationPresenter::OnNotificationButtonClicked(
    const std::string& notification_id,
    int button_index) {
  PA_LOG(INFO) << "Button at index " << button_index
               << " of notification with ID " << notification_id
               << " was clicked.";

  if (notification_id == kPotentialHotspotNotificationId && button_index == 0) {
    network_connect_->ConnectToNetworkId(hotspot_nearby_device_.GetDeviceId());
  }
  message_center_->RemoveNotification(notification_id, true /* by_user */);
}

void TetherNotificationPresenter::SetSettingsUiDelegateForTesting(
    std::unique_ptr<SettingsUiDelegate> settings_ui_delegate) {
  settings_ui_delegate_ = std::move(settings_ui_delegate);
}

void TetherNotificationPresenter::ShowNotification(
    std::unique_ptr<message_center::Notification> notification) {
  std::string notification_id = notification->id();
  if (message_center_->FindVisibleNotificationById(notification_id)) {
    message_center_->UpdateNotification(notification_id,
                                        std::move(notification));
  } else {
    message_center_->AddNotification(std::move(notification));
  }
}

}  // namespace tether

}  // namespace chromeos
