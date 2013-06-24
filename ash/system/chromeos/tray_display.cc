// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/tray_display.h"

#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "ash/system/tray/actionable_view.h"
#include "ash/system/tray/fixed_sized_image_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_notification_view.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace internal {
namespace {

DisplayManager* GetDisplayManager() {
  return Shell::GetInstance()->display_manager();
}

base::string16 GetDisplayName(int64 display_id) {
  return UTF8ToUTF16(GetDisplayManager()->GetDisplayNameForId(display_id));
}

base::string16 GetDisplaySize(int64 display_id) {
  return UTF8ToUTF16(
      GetDisplayManager()->GetDisplayForId(display_id).size().ToString());
}

bool ShouldShowResolution(int64 display_id) {
  if (!GetDisplayManager()->GetDisplayForId(display_id).is_valid())
    return false;

  const DisplayInfo& display_info =
      GetDisplayManager()->GetDisplayInfo(display_id);
  return display_info.rotation() != gfx::Display::ROTATE_0 ||
      display_info.ui_scale() != 1.0f;
}

// Returns the name of the currently connected external display.
base::string16 GetExternalDisplayName() {
  DisplayManager* display_manager = GetDisplayManager();
  int64 external_id = display_manager->mirrored_display().id();

  if (external_id == gfx::Display::kInvalidDisplayID) {
    int64 internal_display_id = gfx::Display::InternalDisplayId();
    for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
      int64 id = display_manager->GetDisplayAt(i)->id();
      if (id != internal_display_id) {
        external_id = id;
        break;
      }
    }
  }

  if (external_id == gfx::Display::kInvalidDisplayID)
    return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_UNKNOWN_DISPLAY_NAME);

  // The external display name may have an annotation of "(width x height)" in
  // case that the display is rotated or its resolution is changed.
  base::string16 name = GetDisplayName(external_id);
  if (ShouldShowResolution(external_id))
    name += UTF8ToUTF16(" (") + GetDisplaySize(external_id) + UTF8ToUTF16(")");

  return name;
}

base::string16 GetTrayDisplayMessage() {
  DisplayManager* display_manager = GetDisplayManager();
  if (display_manager->GetNumDisplays() > 1) {
    if (GetDisplayManager()->HasInternalDisplay()) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_DISPLAY_EXTENDED, GetExternalDisplayName());
    }
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_DISPLAY_EXTENDED_NO_INTERNAL);
  }

  if (display_manager->IsMirrored()) {
    if (GetDisplayManager()->HasInternalDisplay()) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_DISPLAY_MIRRORING, GetExternalDisplayName());
    }
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_DISPLAY_MIRRORING_NO_INTERNAL);
  }

  int64 first_id = display_manager->first_display_id();
  if (display_manager->HasInternalDisplay() &&
      !display_manager->IsInternalDisplayId(first_id)) {
    return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_DOCKED);
  }

  return base::string16();
}

void OpenSettings(user::LoginStatus login_status) {
  if (login_status == ash::user::LOGGED_IN_USER ||
      login_status == ash::user::LOGGED_IN_OWNER ||
      login_status == ash::user::LOGGED_IN_GUEST) {
    ash::Shell::GetInstance()->system_tray_delegate()->ShowDisplaySettings();
  }
}

}  // namespace

class DisplayView : public ash::internal::ActionableView {
 public:
  explicit DisplayView(user::LoginStatus login_status)
      : login_status_(login_status) {
    SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kHorizontal,
        ash::kTrayPopupPaddingHorizontal, 0,
        ash::kTrayPopupPaddingBetweenItems));

    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    image_ =
        new ash::internal::FixedSizedImageView(0, ash::kTrayPopupItemHeight);
    image_->SetImage(
        bundle.GetImageNamed(IDR_AURA_UBER_TRAY_DISPLAY).ToImageSkia());
    AddChildView(image_);

    label_ = new views::Label();
    label_->SetMultiLine(true);
    label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    AddChildView(label_);
    Update();
  }

  virtual ~DisplayView() {}

  void Update() {
    base::string16 message = GetTrayDisplayMessage();
    if (message.empty())
      message = GetInternalDisplayInfo();
    SetVisible(!message.empty());
    label_->SetText(message);
  }

  views::Label* label() { return label_; }

  // Overridden from views::View.
  virtual bool GetTooltipText(const gfx::Point& p,
                              base::string16* tooltip) const OVERRIDE {
    base::string16 tray_message = GetTrayDisplayMessage();
    base::string16 internal_message = GetInternalDisplayInfo();
    if (tray_message.empty() && internal_message.empty())
      return false;

    *tooltip = tray_message + ASCIIToUTF16("\n") + internal_message;
    return true;
  }

 private:
  base::string16 GetInternalDisplayInfo() const {
    int64 first_id = GetDisplayManager()->first_display_id();
    if (!ShouldShowResolution(first_id))
      return base::string16();

    return l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_DISPLAY_SINGLE_DISPLAY,
        GetDisplayName(first_id),
        GetDisplaySize(first_id));
  }

  // Overridden from ActionableView.
  virtual bool PerformAction(const ui::Event& event) OVERRIDE {
    OpenSettings(login_status_);
    return true;
  }

  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE {
    int label_max_width = bounds().width() - kTrayPopupPaddingHorizontal * 2 -
        kTrayPopupPaddingBetweenItems - image_->GetPreferredSize().width();
    label_->SizeToFit(label_max_width);
    PreferredSizeChanged();
  }

  user::LoginStatus login_status_;
  views::ImageView* image_;
  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(DisplayView);
};

class DisplayNotificationView : public TrayNotificationView {
 public:
  DisplayNotificationView(user::LoginStatus login_status,
                          TrayDisplay* tray_item,
                          const base::string16& message)
      : TrayNotificationView(tray_item, IDR_AURA_UBER_TRAY_DISPLAY),
        login_status_(login_status) {
    StartAutoCloseTimer(kTrayPopupAutoCloseDelayForTextInSeconds);
    Update(message);
  }

  virtual ~DisplayNotificationView() {}

  void Update(const base::string16& message) {
    if (message.empty()) {
      owner()->HideNotificationView();
    } else {
      views::Label* label = new views::Label(message);
      label->SetMultiLine(true);
      label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      UpdateView(label);
      RestartAutoCloseTimer();
    }
  }

  // Overridden from TrayNotificationView:
  virtual void OnClickAction() OVERRIDE {
    OpenSettings(login_status_);
  }

 private:
  user::LoginStatus login_status_;

  DISALLOW_COPY_AND_ASSIGN(DisplayNotificationView);
};

TrayDisplay::TrayDisplay(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      default_(NULL),
      notification_(NULL) {
  current_message_ = GetDisplayMessageForNotification();
  Shell::GetInstance()->display_controller()->AddObserver(this);
}

TrayDisplay::~TrayDisplay() {
  Shell::GetInstance()->display_controller()->RemoveObserver(this);
}

base::string16 TrayDisplay::GetDisplayMessageForNotification() {
  DisplayManager* display_manager = GetDisplayManager();
  DisplayInfoMap old_info;
  old_info.swap(display_info_);
  for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
    int64 id = display_manager->GetDisplayAt(i)->id();
    display_info_[id] = display_manager->GetDisplayInfo(id);
  }

  // Display is added or removed. Use the same message as the one in
  // the system tray.
  if (display_info_.size() != old_info.size())
    return GetTrayDisplayMessage();

  for (DisplayInfoMap::const_iterator iter = display_info_.begin();
       iter != display_info_.end(); ++iter) {
    DisplayInfoMap::const_iterator old_iter = old_info.find(iter->first);
    // A display is removed and added at the same time. It won't happen
    // in the actual environment, but falls back to the system tray's
    // message just in case.
    if (old_iter == old_info.end())
      return GetTrayDisplayMessage();

    if (iter->second.ui_scale() != old_iter->second.ui_scale()) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_DISPLAY_RESOLUTION_CHANGED,
          GetDisplayName(iter->first),
          GetDisplaySize(iter->first));
    }
    if (iter->second.rotation() != old_iter->second.rotation()) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_DISPLAY_ROTATED, GetDisplayName(iter->first));
    }
  }

  // Found nothing special
  return base::string16();
}

views::View* TrayDisplay::CreateDefaultView(user::LoginStatus status) {
  DCHECK(default_ == NULL);
  default_ = new DisplayView(status);
  return default_;
}

views::View* TrayDisplay::CreateNotificationView(user::LoginStatus status) {
  DCHECK(notification_ == NULL);
  notification_ = new DisplayNotificationView(status, this, current_message_);
  return notification_;
}

void TrayDisplay::DestroyDefaultView() {
  default_ = NULL;
}

void TrayDisplay::DestroyNotificationView() {
  notification_ = NULL;
}

bool TrayDisplay::ShouldShowLauncher() const {
  return false;
}

void TrayDisplay::OnDisplayConfigurationChanged() {
  if (!Shell::GetInstance()->system_tray_delegate()->
          ShouldShowDisplayNotification()) {
    return;
  }

  // TODO(mukai): do not show the notification when the configuration changed
  // due to the user operation on display settings page.
  current_message_ = GetDisplayMessageForNotification();
  if (notification_)
    notification_->Update(current_message_);
  else if (!current_message_.empty())
    ShowNotificationView();
}

base::string16 TrayDisplay::GetDefaultViewMessage() {
  if (!default_ || !default_->visible())
    return base::string16();

  return static_cast<DisplayView*>(default_)->label()->text();
}

}  // namespace internal
}  // namespace ash
