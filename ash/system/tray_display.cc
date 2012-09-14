// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray_display.h"

#include "ash/display/display_controller.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_views.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/aura/display_manager.h"
#include "ui/aura/env.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

#if defined(USE_X11)
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
    views::ImageView* image =
        new ash::internal::FixedSizedImageView(0, ash::kTrayPopupItemHeight);
    image->SetImage(
        bundle.GetImageNamed(IDR_AURA_UBER_TRAY_DISPLAY).ToImageSkia());
    AddChildView(image);
    label_ = new views::Label();
    AddChildView(label_);
    Update();
  }

  virtual ~DisplayView() {}

  void Update() {
#if defined(OS_CHROMEOS)
    switch (Shell::GetInstance()->output_configurator()->output_state()) {
      case chromeos::STATE_INVALID:
      case chromeos::STATE_HEADLESS:
      case chromeos::STATE_SINGLE:
        SetVisible(false);
        return;
      case chromeos::STATE_DUAL_MIRROR: {
        // Simply assumes that the primary display appears first and the
        // secondary display appears next in the list.
        std::vector<std::string> display_names;
#if defined(USE_X11)
        std::vector<XID> output_ids;
        ui::GetOutputDeviceHandles(&output_ids);
        display_names = ui::GetDisplayNames(output_ids);
#endif
        if (display_names.size() > 1) {
          label_->SetText(l10n_util::GetStringFUTF16(
              IDS_ASH_STATUS_TRAY_DISPLAY_MIRRORING,
              UTF8ToUTF16(display_names[1])));
          SetVisible(true);
        } else {
          SetVisible(false);
        }
        return;
      }
      case chromeos::STATE_DUAL_PRIMARY_ONLY:
      case chromeos::STATE_DUAL_SECONDARY_ONLY: {
        aura::DisplayManager* display_manager =
            aura::Env::GetInstance()->display_manager();
        if (display_manager->GetNumDisplays() > 1) {
          label_->SetText(l10n_util::GetStringFUTF16(
              IDS_ASH_STATUS_TRAY_DISPLAY_EXTENDED,
              UTF8ToUTF16(display_manager->GetDisplayNameAt(1))));
          SetVisible(true);
        } else {
          SetVisible(false);
        }
        return;
      }
      default:
        NOTREACHED();
    }
#endif  // OS_CHROMEOS
  }

 private:
  // Overridden from ActionableView.
  virtual bool PerformAction(const ui::Event& event) OVERRIDE {
    if (login_status_ == ash::user::LOGGED_IN_USER ||
        login_status_ == ash::user::LOGGED_IN_OWNER ||
        login_status_ == ash::user::LOGGED_IN_GUEST) {
      ash::Shell::GetInstance()->tray_delegate()->ShowDisplaySettings();
    }

    return true;
  }

  user::LoginStatus login_status_;
  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(DisplayView);
};

TrayDisplay::TrayDisplay()
    : default_(NULL) {
  aura::Env::GetInstance()->display_manager()->AddObserver(this);
#if defined(OS_CHROMEOS)
  ash::Shell::GetInstance()->output_configurator()->AddObserver(this);
#endif
}

TrayDisplay::~TrayDisplay() {
  aura::Env::GetInstance()->display_manager()->RemoveObserver(this);
#if defined(OS_CHROMEOS)
  ash::Shell::GetInstance()->output_configurator()->RemoveObserver(this);
#endif
}

views::View* TrayDisplay::CreateDefaultView(user::LoginStatus status) {
#if defined(OS_CHROMEOS)
  default_ = new DisplayView(status);
#endif
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
