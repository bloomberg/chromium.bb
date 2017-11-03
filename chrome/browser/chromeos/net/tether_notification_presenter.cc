// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/tether_notification_presenter.h"

#include "ash/system/system_notifier.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/proximity_auth/logging/logging.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/notifier_id.h"

namespace chromeos {

namespace tether {

namespace {

// Mean value of NetworkState's signal_strength() range.
const int kMediumSignalStrength = 50;

// Dimensions of Tether notification icon in pixels.
constexpr gfx::Size kTetherSignalIconSize(18, 18);

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

// Returns the icon to use for a network with the given signal strength, which
// should range from 0 to 100 (inclusive).
const gfx::ImageSkia GetImageForSignalStrength(int signal_strength) {
  // Convert the [0, 100] range to [0, 4], since there are 5 distinct signal
  // strength icons (0 bars to 4 bars).
  int normalized_signal_strength =
      std::min(std::max(signal_strength / 25, 0), 4);

  return gfx::CanvasImageSource::MakeImageSkia<
      ash::network_icon::SignalStrengthImageSource>(
      ash::network_icon::BARS, gfx::kGoogleBlue500, kTetherSignalIconSize,
      normalized_signal_strength);
}

std::unique_ptr<message_center::Notification> CreateNotification(
    const std::string& id,
    const base::string16& title,
    const base::string16& message,
    const gfx::ImageSkia& small_image,
    const message_center::RichNotificationData& rich_notification_data =
        message_center::RichNotificationData()) {
  auto notification = base::MakeUnique<message_center::Notification>(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE, id, title,
      message, gfx::Image() /* image */, base::string16() /* display_source */,
      GURL() /* origin_url */,
      message_center::NotifierId(
          message_center::NotifierId::NotifierType::SYSTEM_COMPONENT,
          ash::system_notifier::kNotifierTether),
      rich_notification_data, nullptr);
  notification->SetSystemPriority();
  notification->set_small_image(gfx::Image(small_image));
  return notification;
}

}  // namespace

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
constexpr const char* const
    TetherNotificationPresenter::kIdsWhichOpenTetherSettingsOnClick[] = {
        TetherNotificationPresenter::kActiveHostNotificationId,
        TetherNotificationPresenter::kPotentialHotspotNotificationId,
        TetherNotificationPresenter::kSetupRequiredNotificationId};

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

  hotspot_nearby_device_id_ =
      base::MakeUnique<std::string>(remote_device.GetDeviceId());

  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_TETHER_NOTIFICATION_WIFI_AVAILABLE_ONE_DEVICE_CONNECT)));

  ShowNotification(CreateNotification(
      kPotentialHotspotNotificationId,
      l10n_util::GetStringUTF16(
          IDS_TETHER_NOTIFICATION_WIFI_AVAILABLE_ONE_DEVICE_TITLE),
      l10n_util::GetStringFUTF16(
          IDS_TETHER_NOTIFICATION_WIFI_AVAILABLE_ONE_DEVICE_MESSAGE,
          base::ASCIIToUTF16(remote_device.name)),
      GetImageForSignalStrength(signal_strength), rich_notification_data));
}

void TetherNotificationPresenter::NotifyMultiplePotentialHotspotsNearby() {
  PA_LOG(INFO) << "Displaying \"potential hotspot nearby\" notification for "
               << "multiple devices. Notification ID = "
               << kPotentialHotspotNotificationId;

  hotspot_nearby_device_id_.reset();

  ShowNotification(CreateNotification(
      kPotentialHotspotNotificationId,
      l10n_util::GetStringUTF16(
          IDS_TETHER_NOTIFICATION_WIFI_AVAILABLE_MULTIPLE_DEVICES_TITLE),
      l10n_util::GetStringUTF16(
          IDS_TETHER_NOTIFICATION_WIFI_AVAILABLE_MULTIPLE_DEVICES_MESSAGE),
      GetImageForSignalStrength(kMediumSignalStrength)));
}

NotificationPresenter::PotentialHotspotNotificationState
TetherNotificationPresenter::GetPotentialHotspotNotificationState() {
  if (!message_center_->FindVisibleNotificationById(
          kPotentialHotspotNotificationId)) {
    return NotificationPresenter::PotentialHotspotNotificationState::
        NO_HOTSPOT_NOTIFICATION_SHOWN;
  }

  return hotspot_nearby_device_id_
             ? NotificationPresenter::PotentialHotspotNotificationState::
                   SINGLE_HOTSPOT_NEARBY_SHOWN
             : NotificationPresenter::PotentialHotspotNotificationState::
                   MULTIPLE_HOTSPOTS_NEARBY_SHOWN;
}

void TetherNotificationPresenter::RemovePotentialHotspotNotification() {
  RemoveNotificationIfVisible(kPotentialHotspotNotificationId);
}

void TetherNotificationPresenter::NotifySetupRequired(
    const std::string& device_name,
    int signal_strength) {
  PA_LOG(INFO) << "Displaying \"setup required\" notification. Notification "
               << "ID = " << kSetupRequiredNotificationId;

  ShowNotification(CreateNotification(
      kSetupRequiredNotificationId,
      l10n_util::GetStringFUTF16(IDS_TETHER_NOTIFICATION_SETUP_REQUIRED_TITLE,
                                 base::ASCIIToUTF16(device_name)),
      l10n_util::GetStringFUTF16(IDS_TETHER_NOTIFICATION_SETUP_REQUIRED_MESSAGE,
                                 base::ASCIIToUTF16(device_name)),
      GetImageForSignalStrength(signal_strength)));
}

void TetherNotificationPresenter::RemoveSetupRequiredNotification() {
  RemoveNotificationIfVisible(kSetupRequiredNotificationId);
}

void TetherNotificationPresenter::NotifyConnectionToHostFailed() {
  PA_LOG(INFO) << "Displaying \"connection attempt failed\" notification. "
               << "Notification ID = " << kActiveHostNotificationId;

  ShowNotification(message_center::Notification::CreateSystemNotification(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE,
      kActiveHostNotificationId,
      l10n_util::GetStringUTF16(
          IDS_TETHER_NOTIFICATION_CONNECTION_FAILED_TITLE),
      l10n_util::GetStringUTF16(
          IDS_TETHER_NOTIFICATION_CONNECTION_FAILED_MESSAGE),
      gfx::Image() /* icon */, base::string16() /* display_source */,
      GURL() /* origin_url */,
      message_center::NotifierId(
          message_center::NotifierId::NotifierType::SYSTEM_COMPONENT,
          ash::system_notifier::kNotifierTether),
      {} /* rich_notification_data */, nullptr /* delegate */,
      kNotificationCellularAlertIcon,
      message_center::SystemNotificationWarningLevel::WARNING));
}

void TetherNotificationPresenter::RemoveConnectionToHostFailedNotification() {
  RemoveNotificationIfVisible(kActiveHostNotificationId);
}

void TetherNotificationPresenter::OnNotificationClicked(
    const std::string& notification_id) {
  // Iterate through all IDs corresponding to notifications which should open
  // the Tether settings page when clicked. If the notification ID provided
  // exists in |kIdsWhichOpenTetherSettingsOnClick|, open settings.
  for (size_t i = 0; i < arraysize(kIdsWhichOpenTetherSettingsOnClick); ++i) {
    const char* const curr_clickable_id = kIdsWhichOpenTetherSettingsOnClick[i];
    if (notification_id == curr_clickable_id) {
      OpenSettingsAndRemoveNotification(kTetherSettingsSubpage,
                                        notification_id);
      return;
    }
  }

  // Otherwise, ignore this click since it is not in the list of clickable IDs
  // (e.g., it could have been created by another feature/application).
}

void TetherNotificationPresenter::OnNotificationButtonClicked(
    const std::string& notification_id,
    int button_index) {
  if (notification_id != kPotentialHotspotNotificationId)
    return;

  DCHECK(button_index == 0);
  DCHECK(hotspot_nearby_device_id_);
  PA_LOG(INFO) << "\"Potential hotspot nearby\" notification button was "
               << "clicked.";
  network_connect_->ConnectToNetworkId(*hotspot_nearby_device_id_);
  RemoveNotificationIfVisible(kPotentialHotspotNotificationId);
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

void TetherNotificationPresenter::OpenSettingsAndRemoveNotification(
    const std::string& settings_subpage,
    const std::string& notification_id) {
  PA_LOG(INFO) << "Notification with ID " << notification_id << " was clicked. "
               << "Opening settings subpage: " << settings_subpage;

  settings_ui_delegate_->ShowSettingsSubPageForProfile(profile_,
                                                       settings_subpage);
  RemoveNotificationIfVisible(notification_id);
}

void TetherNotificationPresenter::RemoveNotificationIfVisible(
    const std::string& notification_id) {
  if (notification_id == kPotentialHotspotNotificationId)
    hotspot_nearby_device_id_.reset();

  if (!message_center_->FindVisibleNotificationById(notification_id))
    return;

  PA_LOG(INFO) << "Removing notification with ID \"" << notification_id
               << "\".";
  message_center_->RemoveNotification(notification_id, false /* by_user */);
}

}  // namespace tether

}  // namespace chromeos
