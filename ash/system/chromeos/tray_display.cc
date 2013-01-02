// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/tray_display.h"

#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_views.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/aura/env.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace internal {

class DisplayView : public ash::internal::ActionableView {
 public:
  explicit DisplayView(user::LoginStatus login_status)
      : login_status_(login_status) {
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
    label_ = new views::Label();
    label_->SetMultiLine(true);
    label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    AddChildView(label_);
    Update();
  }

  virtual ~DisplayView() {}

  void Update() {
    switch (Shell::GetInstance()->output_configurator()->output_state()) {
      case chromeos::STATE_INVALID:
      case chromeos::STATE_HEADLESS:
      case chromeos::STATE_SINGLE:
        SetVisible(false);
        return;
      case chromeos::STATE_DUAL_MIRROR: {
        label_->SetText(l10n_util::GetStringFUTF16(
            IDS_ASH_STATUS_TRAY_DISPLAY_MIRRORING, GetExternalDisplayName()));
        SetVisible(true);
        return;
      }
      case chromeos::STATE_DUAL_PRIMARY_ONLY:
      case chromeos::STATE_DUAL_SECONDARY_ONLY:
      case chromeos::STATE_DUAL_UNKNOWN: {
        label_->SetText(l10n_util::GetStringFUTF16(
            IDS_ASH_STATUS_TRAY_DISPLAY_EXTENDED, GetExternalDisplayName()));
        SetVisible(true);
        return;
      }
      default:
        NOTREACHED();
    }
  }

 private:
  // Returns the name of the currently connected external display.
  string16 GetExternalDisplayName() {
    DisplayManager* display_manager = Shell::GetInstance()->display_manager();

    gfx::Display external_display(gfx::Display::kInvalidDisplayID);
    if (display_manager->HasInternalDisplay()) {
      for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
        gfx::Display* display = display_manager->GetDisplayAt(i);
        if (!display_manager->IsInternalDisplayId(display->id())) {
          external_display = *display;
          break;
        }
      }
    } else {
      // Falls back to the secondary display since the system doesn't
      // distinguish the displays.
      external_display = ScreenAsh::GetSecondaryDisplay();
    }

    return UTF8ToUTF16(display_manager->GetDisplayNameFor(external_display));
  }

  // Overridden from ActionableView.
  virtual bool PerformAction(const ui::Event& event) OVERRIDE {
    if (login_status_ == ash::user::LOGGED_IN_USER ||
        login_status_ == ash::user::LOGGED_IN_OWNER ||
        login_status_ == ash::user::LOGGED_IN_GUEST) {
      ash::Shell::GetInstance()->system_tray_delegate()->ShowDisplaySettings();
    }

    return true;
  }

  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) {
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

TrayDisplay::TrayDisplay(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      default_(NULL) {
  Shell::GetScreen()->AddObserver(this);
  Shell::GetInstance()->output_configurator()->AddObserver(this);
}

TrayDisplay::~TrayDisplay() {
  Shell::GetScreen()->RemoveObserver(this);
  Shell::GetInstance()->output_configurator()->RemoveObserver(this);
}

views::View* TrayDisplay::CreateDefaultView(user::LoginStatus status) {
  default_ = new DisplayView(status);
  return default_;
}

void TrayDisplay::DestroyDefaultView() {
  default_ = NULL;
}

void TrayDisplay::OnDisplayBoundsChanged(const gfx::Display& display) {
  if (default_)
    default_->Update();
}

void TrayDisplay::OnDisplayAdded(const gfx::Display& new_display) {
  if (default_)
    default_->Update();
}

void TrayDisplay::OnDisplayRemoved(const gfx::Display& old_display) {
  if (default_)
    default_->Update();
}

void TrayDisplay::OnDisplayModeChanged() {
  if (default_)
    default_->Update();
}

}  // namespace internal
}  // namespace ash
