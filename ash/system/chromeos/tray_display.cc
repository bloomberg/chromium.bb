// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/tray_display.h"

#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/system/tray/fixed_sized_image_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "base/chromeos/chromeos_version.h"
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

#if defined(USE_X11)
#include "chromeos/display/output_configurator.h"
#include "ui/base/x/x11_util.h"
#endif

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
#if !defined(USE_X11)
     SetVisible(false);
#else
    chromeos::OutputState state =
        base::chromeos::IsRunningOnChromeOS() ?
        Shell::GetInstance()->output_configurator()->output_state() :
        InferOutputState();
    switch (state) {
      case chromeos::STATE_INVALID:
      case chromeos::STATE_HEADLESS:
      case chromeos::STATE_SINGLE:
        SetVisible(false);
        return;
      case chromeos::STATE_DUAL_MIRROR:
        label_->SetText(l10n_util::GetStringFUTF16(
            IDS_ASH_STATUS_TRAY_DISPLAY_MIRRORING, GetExternalDisplayName()));
        SetVisible(true);
        return;
      case chromeos::STATE_DUAL_EXTENDED:
        label_->SetText(l10n_util::GetStringFUTF16(
            IDS_ASH_STATUS_TRAY_DISPLAY_EXTENDED, GetExternalDisplayName()));
        SetVisible(true);
        return;
    }
    NOTREACHED() << "Unhandled state " << state;
#endif
  }

  chromeos::OutputState InferOutputState() const {
    return Shell::GetScreen()->GetNumDisplays() == 1 ?
        chromeos::STATE_SINGLE : chromeos::STATE_DUAL_EXTENDED;
  }

 private:
  // Returns the name of the currently connected external display.
  base::string16 GetExternalDisplayName() const {
    DisplayManager* display_manager = Shell::GetInstance()->display_manager();
    int64 external_id = display_manager->mirrored_display_id();

    if (external_id == gfx::Display::kInvalidDisplayID) {
      int64 internal_display_id = gfx::Display::InternalDisplayId();
      int64 first_display_id = display_manager->first_display_id();
      for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
        int64 id = display_manager->GetDisplayAt(i)->id();
        if (id != internal_display_id && id != first_display_id) {
          external_id = id;
          break;
        }
      }
    }
    if (external_id != gfx::Display::kInvalidDisplayID)
      return UTF8ToUTF16(display_manager->GetDisplayNameForId(external_id));
    return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_UNKNOWN_DISPLAY_NAME);
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

TrayDisplay::TrayDisplay(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      default_(NULL) {
  Shell::GetScreen()->AddObserver(this);
#if defined(USE_X11)
  Shell::GetInstance()->output_configurator()->AddObserver(this);
#endif
}

TrayDisplay::~TrayDisplay() {
  Shell::GetScreen()->RemoveObserver(this);
#if defined(USE_X11)
  Shell::GetInstance()->output_configurator()->RemoveObserver(this);
#endif
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

#if defined(OS_CHROMEOS)
void TrayDisplay::OnDisplayModeChanged() {
  if (default_)
    default_->Update();
}
#endif

}  // namespace internal
}  // namespace ash
