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
      case chromeos::STATE_DUAL_EXTENDED:
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
#if defined(USE_X11)
    DisplayManager* display_manager = Shell::GetInstance()->display_manager();
    int64 internal_display_id = gfx::Display::InternalDisplayId();
    int64 primary_display_id =
        gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().id();

    // Use xrandr features rather than DisplayManager to find out the external
    // display's name. DisplayManager's API doesn't work well in mirroring mode
    // since it's based on gfx::Display but in mirroring mode there's only one
    // gfx::Display instance which represents both displays.
    std::vector<XID> outputs;
    ui::GetOutputDeviceHandles(&outputs);
    for (size_t i = 0; i < outputs.size(); ++i) {
      std::string name;
      uint16 manufacturer_id = 0;
      uint16 product_code = 0;
      if (ui::GetOutputDeviceData(
              outputs[i], &manufacturer_id, &product_code, &name)) {
        int64 display_id = gfx::Display::GetID(
            manufacturer_id, product_code, i);
        if (display_id == internal_display_id)
          continue;
        // Some systems like stumpy don't have the internal display at all. It
        // means both of the displays are external but we need to choose either
        // one. Currently we adopt simple heuristics which just avoids the
        // primary display.
        if (!display_manager->HasInternalDisplay() &&
            display_id == primary_display_id) {
          continue;
        }

        return UTF8ToUTF16(name);
      }
    }
#endif
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
