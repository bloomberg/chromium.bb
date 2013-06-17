// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/tray_display.h"

#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
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
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace internal {
namespace {

TrayDisplayMode GetCurrentTrayDisplayMode() {
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  if (display_manager->GetNumDisplays() > 1)
    return TRAY_DISPLAY_EXTENDED;

  if (display_manager->IsMirrored())
    return TRAY_DISPLAY_MIRRORED;

  int64 first_id = display_manager->first_display_id();
  if (display_manager->HasInternalDisplay() &&
      !display_manager->IsInternalDisplayId(first_id)) {
    return TRAY_DISPLAY_DOCKED;
  }

  return TRAY_DISPLAY_SINGLE;
}

// Returns the name of the currently connected external display.
base::string16 GetExternalDisplayName() {
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
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
  if (external_id != gfx::Display::kInvalidDisplayID)
    return UTF8ToUTF16(display_manager->GetDisplayNameForId(external_id));
  return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_UNKNOWN_DISPLAY_NAME);
}

class DisplayViewBase {
 public:
  DisplayViewBase(user::LoginStatus login_status)
      : login_status_(login_status) {
    label_ = new views::Label();
    label_->SetMultiLine(true);
    label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  }

  virtual ~DisplayViewBase() {
  }

 protected:
  void OpenSettings() {
    if (login_status_ == ash::user::LOGGED_IN_USER ||
        login_status_ == ash::user::LOGGED_IN_OWNER ||
        login_status_ == ash::user::LOGGED_IN_GUEST) {
      ash::Shell::GetInstance()->system_tray_delegate()->ShowDisplaySettings();
    }
  }

  bool UpdateLabelText() {
    switch (GetCurrentTrayDisplayMode()) {
      case TRAY_DISPLAY_SINGLE:
        // TODO(oshima|mukai): Support single display mode for overscan
        // alignment.
        return false;
      case TRAY_DISPLAY_EXTENDED:
        if (Shell::GetInstance()->display_manager()->HasInternalDisplay()) {
          label_->SetText(l10n_util::GetStringFUTF16(
              IDS_ASH_STATUS_TRAY_DISPLAY_EXTENDED, GetExternalDisplayName()));
        } else {
          label_->SetText(l10n_util::GetStringUTF16(
              IDS_ASH_STATUS_TRAY_DISPLAY_EXTENDED_NO_INTERNAL));
        }
        break;
      case TRAY_DISPLAY_MIRRORED:
        if (Shell::GetInstance()->display_manager()->HasInternalDisplay()) {
          label_->SetText(l10n_util::GetStringFUTF16(
              IDS_ASH_STATUS_TRAY_DISPLAY_MIRRORING, GetExternalDisplayName()));
        } else {
          label_->SetText(l10n_util::GetStringUTF16(
              IDS_ASH_STATUS_TRAY_DISPLAY_MIRRORING_NO_INTERNAL));
        }
        break;
      case TRAY_DISPLAY_DOCKED:
        label_->SetText(l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_DISPLAY_DOCKED));
        break;
    }
    return true;
  }

  views::Label* label() { return label_; }

 private:
  user::LoginStatus login_status_;
  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(DisplayViewBase);
};

}  // namespace

class DisplayView : public DisplayViewBase,
                    public ash::internal::ActionableView {
 public:
  explicit DisplayView(user::LoginStatus login_status)
      : DisplayViewBase(login_status) {
    SetLayoutManager(new
        views::BoxLayout(views::BoxLayout::kHorizontal,
        ash::kTrayPopupPaddingHorizontal, 0,
        ash::kTrayPopupPaddingBetweenItems));

    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    image_ =
        new ash::internal::FixedSizedImageView(0, ash::kTrayPopupItemHeight);
    image_->SetImage(
        bundle.GetImageNamed(IDR_AURA_UBER_TRAY_DISPLAY).ToImageSkia());
    AddChildView(image_);
    AddChildView(label());
    Update();
  }

  virtual ~DisplayView() {}

  void Update() {
    SetVisible(UpdateLabelText());
  }

 private:
  // Overridden from ActionableView.
  virtual bool PerformAction(const ui::Event& event) OVERRIDE {
    OpenSettings();
    return true;
  }

  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE {
    int label_max_width = bounds().width() - kTrayPopupPaddingHorizontal * 2 -
        kTrayPopupPaddingBetweenItems - image_->GetPreferredSize().width();
    label()->SizeToFit(label_max_width);
    PreferredSizeChanged();
  }

  views::ImageView* image_;

  DISALLOW_COPY_AND_ASSIGN(DisplayView);
};

class DisplayNotificationView : public DisplayViewBase,
                                public TrayNotificationView {
 public:
  DisplayNotificationView(user::LoginStatus login_status,
                          TrayDisplay* tray_item)
      : DisplayViewBase(login_status),
        TrayNotificationView(tray_item, IDR_AURA_UBER_TRAY_DISPLAY) {
    InitView(label());
    StartAutoCloseTimer(kTrayPopupAutoCloseDelayForTextInSeconds);
    Update();
  }

  virtual ~DisplayNotificationView() {}

  void Update() {
    if (UpdateLabelText())
      RestartAutoCloseTimer();
    else
      owner()->HideNotificationView();
  }

  // Overridden from TrayNotificationView:
  virtual void OnClickAction() OVERRIDE {
    OpenSettings();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayNotificationView);
};

TrayDisplay::TrayDisplay(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      default_(NULL),
      notification_(NULL),
      current_mode_(GetCurrentTrayDisplayMode()) {
  Shell::GetInstance()->display_controller()->AddObserver(this);
}

TrayDisplay::~TrayDisplay() {
  Shell::GetInstance()->display_controller()->RemoveObserver(this);
}

views::View* TrayDisplay::CreateDefaultView(user::LoginStatus status) {
  DCHECK(default_ == NULL);
  default_ = new DisplayView(status);
  return default_;
}

views::View* TrayDisplay::CreateNotificationView(user::LoginStatus status) {
  DCHECK(notification_ == NULL);
  notification_ = new DisplayNotificationView(status, this);
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
  TrayDisplayMode new_mode = GetCurrentTrayDisplayMode();
  if (current_mode_ != new_mode && new_mode != TRAY_DISPLAY_SINGLE) {
    if (notification_)
      notification_->Update();
    else
      ShowNotificationView();
  }
  current_mode_ = new_mode;
}

}  // namespace internal
}  // namespace ash
