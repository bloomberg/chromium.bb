// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/resolution_notification_controller.h"

#include <utility>

#include "ash/resources/grit/ash_resources.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/system_notifier.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"

using message_center::Notification;

namespace ash {
namespace {

bool g_use_timer = true;

class ResolutionChangeNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  ResolutionChangeNotificationDelegate(
      ResolutionNotificationController* controller,
      bool has_timeout);

 protected:
  ~ResolutionChangeNotificationDelegate() override;

 private:
  // message_center::NotificationDelegate overrides:
  void Close(bool by_user) override;
  void Click() override;
  bool HasClickedListener() override;
  void ButtonClick(int button_index) override;

  ResolutionNotificationController* controller_;
  bool has_timeout_;

  DISALLOW_COPY_AND_ASSIGN(ResolutionChangeNotificationDelegate);
};

ResolutionChangeNotificationDelegate::ResolutionChangeNotificationDelegate(
    ResolutionNotificationController* controller,
    bool has_timeout)
    : controller_(controller), has_timeout_(has_timeout) {
  DCHECK(controller_);
}

ResolutionChangeNotificationDelegate::~ResolutionChangeNotificationDelegate() {}

void ResolutionChangeNotificationDelegate::Close(bool by_user) {
  if (by_user)
    controller_->AcceptResolutionChange(false);
}

void ResolutionChangeNotificationDelegate::Click() {
  controller_->AcceptResolutionChange(true);
}

bool ResolutionChangeNotificationDelegate::HasClickedListener() {
  return true;
}

void ResolutionChangeNotificationDelegate::ButtonClick(int button_index) {
  // If there's the timeout, the first button is "Accept". Otherwise the
  // button click should be "Revert".
  if (has_timeout_ && button_index == 0)
    controller_->AcceptResolutionChange(true);
  else
    controller_->RevertResolutionChange(false /* display_was_removed */);
}

}  // namespace

// static
const int ResolutionNotificationController::kTimeoutInSec = 15;

// static
const char ResolutionNotificationController::kNotificationId[] =
    "chrome://settings/display/resolution";

struct ResolutionNotificationController::ResolutionChangeInfo {
  ResolutionChangeInfo(int64_t display_id,
                       const display::ManagedDisplayMode& old_resolution,
                       const display::ManagedDisplayMode& new_resolution,
                       const base::Closure& accept_callback);
  ~ResolutionChangeInfo();

  // The id of the display where the resolution change happens.
  const int64_t display_id;

  // The resolution before the change.
  display::ManagedDisplayMode old_resolution;

  // The requested resolution. Note that this may be different from
  // |current_resolution| which is the actual resolution set.
  display::ManagedDisplayMode new_resolution;

  // The actual resolution after the change.
  display::ManagedDisplayMode current_resolution;

  // The callback when accept is chosen.
  const base::Closure accept_callback;

  // The remaining timeout in seconds. 0 if the change does not time out.
  uint8_t timeout_count;

  // The timer to invoke OnTimerTick() every second. This cannot be
  // OneShotTimer since the message contains text "automatically closed in xx
  // seconds..." which has to be updated every second.
  base::RepeatingTimer timer;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResolutionChangeInfo);
};

ResolutionNotificationController::ResolutionChangeInfo::ResolutionChangeInfo(
    int64_t display_id,
    const display::ManagedDisplayMode& old_resolution,
    const display::ManagedDisplayMode& new_resolution,
    const base::Closure& accept_callback)
    : display_id(display_id),
      old_resolution(old_resolution),
      new_resolution(new_resolution),
      accept_callback(accept_callback),
      timeout_count(0) {
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  if (!display::Display::HasInternalDisplay() &&
      display_manager->num_connected_displays() == 1u) {
    timeout_count = kTimeoutInSec;
  }
}

ResolutionNotificationController::ResolutionChangeInfo::
    ~ResolutionChangeInfo() {}

ResolutionNotificationController::ResolutionNotificationController() {
  Shell::Get()->window_tree_host_manager()->AddObserver(this);
  display::Screen::GetScreen()->AddObserver(this);
}

ResolutionNotificationController::~ResolutionNotificationController() {
  Shell::Get()->window_tree_host_manager()->RemoveObserver(this);
  display::Screen::GetScreen()->RemoveObserver(this);
}

bool ResolutionNotificationController::PrepareNotificationAndSetDisplayMode(
    int64_t display_id,
    const display::ManagedDisplayMode& old_resolution,
    const display::ManagedDisplayMode& new_resolution,
    const base::Closure& accept_callback) {
  display::DisplayManager* const display_manager =
      Shell::Get()->display_manager();
  if (display::Display::IsInternalDisplayId(display_id)) {
    // We don't show notifications to confirm/revert the resolution change in
    // the case of an internal display.
    return display_manager->SetDisplayMode(display_id, new_resolution);
  }

  // If multiple resolution changes are invoked for the same display,
  // the original resolution for the first resolution change has to be used
  // instead of the specified |old_resolution|.
  display::ManagedDisplayMode original_resolution;
  if (change_info_ && change_info_->display_id == display_id) {
    DCHECK(change_info_->new_resolution.size() == old_resolution.size());
    original_resolution = change_info_->old_resolution;
  }

  if (change_info_ && change_info_->display_id != display_id) {
    // Preparing the notification for a new resolution change of another display
    // before the previous one was accepted. We decided that it's safer to
    // revert the previous resolution change since the user didn't explicitly
    // accept it, and we have no way of knowing for sure that it worked.
    RevertResolutionChange(false /* display_was_removed */);
  }

  change_info_.reset(new ResolutionChangeInfo(display_id, old_resolution,
                                              new_resolution, accept_callback));
  if (!original_resolution.size().IsEmpty())
    change_info_->old_resolution = original_resolution;

  if (!display_manager->SetDisplayMode(display_id, new_resolution)) {
    // Discard the prepared notification data since we failed to set the new
    // resolution.
    change_info_.reset();
    return false;
  }

  return true;
}

bool ResolutionNotificationController::DoesNotificationTimeout() {
  return change_info_ && change_info_->timeout_count > 0;
}

void ResolutionNotificationController::CreateOrUpdateNotification(
    bool enable_spoken_feedback) {
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  if (!change_info_) {
    message_center->RemoveNotification(kNotificationId, false /* by_user */);
    return;
  }

  base::string16 timeout_message;
  message_center::RichNotificationData data;
  if (change_info_->timeout_count > 0) {
    data.buttons.push_back(message_center::ButtonInfo(
        l10n_util::GetStringUTF16(IDS_ASH_DISPLAY_RESOLUTION_CHANGE_ACCEPT)));
    timeout_message = l10n_util::GetStringFUTF16(
        IDS_ASH_DISPLAY_RESOLUTION_TIMEOUT,
        ui::TimeFormat::Simple(
            ui::TimeFormat::FORMAT_DURATION, ui::TimeFormat::LENGTH_LONG,
            base::TimeDelta::FromSeconds(change_info_->timeout_count)));
  }
  data.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_ASH_DISPLAY_RESOLUTION_CHANGE_REVERT)));

  data.should_make_spoken_feedback_for_popup_updates = enable_spoken_feedback;

  const base::string16 display_name =
      base::UTF8ToUTF16(Shell::Get()->display_manager()->GetDisplayNameForId(
          change_info_->display_id));
  const base::string16 message =
      (change_info_->new_resolution.size() ==
       change_info_->current_resolution.size())
          ? l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_DISPLAY_RESOLUTION_CHANGED, display_name,
                base::UTF8ToUTF16(
                    change_info_->new_resolution.size().ToString()))
          : l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_DISPLAY_RESOLUTION_CHANGED_TO_UNSUPPORTED,
                display_name,
                base::UTF8ToUTF16(
                    change_info_->new_resolution.size().ToString()),
                base::UTF8ToUTF16(
                    change_info_->current_resolution.size().ToString()));

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  std::unique_ptr<Notification> notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId, message,
      timeout_message, bundle.GetImageNamed(IDR_AURA_NOTIFICATION_DISPLAY),
      base::string16() /* display_source */, GURL(),
      message_center::NotifierId(
          message_center::NotifierId::SYSTEM_COMPONENT,
          system_notifier::kNotifierDisplayResolutionChange),
      data, new ResolutionChangeNotificationDelegate(
                this, change_info_->timeout_count > 0)));
  notification->SetSystemPriority();
  message_center->AddNotification(std::move(notification));
}

void ResolutionNotificationController::OnTimerTick() {
  if (!change_info_)
    return;

  --change_info_->timeout_count;
  if (change_info_->timeout_count == 0)
    RevertResolutionChange(false /* display_was_removed */);
  else
    CreateOrUpdateNotification(false);
}

void ResolutionNotificationController::AcceptResolutionChange(
    bool close_notification) {
  if (close_notification) {
    message_center::MessageCenter::Get()->RemoveNotification(
        kNotificationId, false /* by_user */);
  }
  if (!change_info_)
    return;
  base::Closure callback = change_info_->accept_callback;
  change_info_.reset();
  callback.Run();
}

void ResolutionNotificationController::RevertResolutionChange(
    bool display_was_removed) {
  message_center::MessageCenter::Get()->RemoveNotification(kNotificationId,
                                                           false /* by_user */);
  if (!change_info_)
    return;
  int64_t display_id = change_info_->display_id;
  display::ManagedDisplayMode old_resolution = change_info_->old_resolution;
  change_info_.reset();
  if (display_was_removed) {
    // If display was removed then we are inside the stack of
    // DisplayManager::UpdateDisplaysWith(), and we need to update the selected
    // mode of this removed display without reentering again into
    // UpdateDisplaysWith() because this can cause a crash. crbug.com/709722.
    Shell::Get()->display_manager()->SetSelectedModeForDisplayId(
        display_id, old_resolution);
  } else {
    Shell::Get()->display_manager()->SetDisplayMode(display_id, old_resolution);
  }
}

void ResolutionNotificationController::OnDisplayAdded(
    const display::Display& new_display) {}

void ResolutionNotificationController::OnDisplayRemoved(
    const display::Display& old_display) {
  if (change_info_ && change_info_->display_id == old_display.id())
    RevertResolutionChange(true /* display_was_removed */);
}

void ResolutionNotificationController::OnDisplayMetricsChanged(
    const display::Display&,
    uint32_t) {}

void ResolutionNotificationController::OnDisplayConfigurationChanged() {
  if (!change_info_)
    return;

  display::ManagedDisplayMode mode;
  if (Shell::Get()->display_manager()->GetActiveModeForDisplayId(
          change_info_->display_id, &mode)) {
    change_info_->current_resolution = mode;
  }

  CreateOrUpdateNotification(true);
  if (g_use_timer && change_info_->timeout_count > 0) {
    change_info_->timer.Start(FROM_HERE, base::TimeDelta::FromSeconds(1), this,
                              &ResolutionNotificationController::OnTimerTick);
  }
}

void ResolutionNotificationController::SuppressTimerForTest() {
  g_use_timer = false;
}

}  // namespace ash
