// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/rotation/tray_rotation_lock.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_item_more.h"
#include "ash/common/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/display/screen_orientation_controller_chromeos.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/display.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"

namespace ash {

namespace tray {

// Extends TrayItemMore, however does not make use of the chevron, nor of the
// DetailedView. This was chosen over ActionableView in order to reuse the
// layout and styling of labels and images. This allows RotationLockDefaultView
// to maintain the look of other system tray items without code duplication.
class RotationLockDefaultView : public TrayItemMore, public ShellObserver {
 public:
  explicit RotationLockDefaultView(SystemTrayItem* owner);
  ~RotationLockDefaultView() override;

  // ActionableView:
  bool PerformAction(const ui::Event& event) override;

  // ShellObserver:
  void OnMaximizeModeStarted() override;
  void OnMaximizeModeEnded() override;

 private:
  void UpdateImage();

  DISALLOW_COPY_AND_ASSIGN(RotationLockDefaultView);
};

RotationLockDefaultView::RotationLockDefaultView(SystemTrayItem* owner)
    : TrayItemMore(owner, false) {
  UpdateImage();
  SetVisible(WmShell::Get()
                 ->maximize_mode_controller()
                 ->IsMaximizeModeWindowManagerEnabled());
  WmShell::Get()->AddShellObserver(this);
}

RotationLockDefaultView::~RotationLockDefaultView() {
  WmShell::Get()->RemoveShellObserver(this);
}

bool RotationLockDefaultView::PerformAction(const ui::Event& event) {
  ScreenOrientationController* screen_orientation_controller =
      Shell::GetInstance()->screen_orientation_controller();
  screen_orientation_controller->SetRotationLocked(
      !screen_orientation_controller->rotation_locked());
  UpdateImage();
  return true;
}

void RotationLockDefaultView::OnMaximizeModeStarted() {
  UpdateImage();
  SetVisible(true);
}

void RotationLockDefaultView::OnMaximizeModeEnded() {
  SetVisible(false);
}

void RotationLockDefaultView::UpdateImage() {
  const bool rotation_locked =
      Shell::GetInstance()->screen_orientation_controller()->rotation_locked();
  if (MaterialDesignController::UseMaterialDesignSystemIcons()) {
    const gfx::VectorIconId icon_id =
        rotation_locked ? gfx::VectorIconId::SYSTEM_MENU_ROTATION_LOCK_LOCKED
                        : gfx::VectorIconId::SYSTEM_MENU_ROTATION_LOCK_AUTO;
    gfx::ImageSkia image_md =
        CreateVectorIcon(icon_id, kMenuIconSize, kMenuIconColor);
    SetImage(&image_md);
  } else {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    const int resource_id = rotation_locked
                                ? IDR_AURA_UBER_TRAY_AUTO_ROTATION_LOCKED_DARK
                                : IDR_AURA_UBER_TRAY_AUTO_ROTATION_DARK;
    SetImage(bundle.GetImageNamed(resource_id).ToImageSkia());
  }

  base::string16 label = l10n_util::GetStringUTF16(
      rotation_locked ? IDS_ASH_STATUS_TRAY_ROTATION_LOCK_LOCKED
                      : IDS_ASH_STATUS_TRAY_ROTATION_LOCK_AUTO);
  SetLabel(label);
  SetAccessibleName(label);
}

}  // namespace tray

TrayRotationLock::TrayRotationLock(SystemTray* system_tray)
    : TrayImageItem(system_tray, IDR_AURA_UBER_TRAY_AUTO_ROTATION_LOCKED),
      observing_rotation_(false),
      observing_shell_(true) {
  WmShell::Get()->AddShellObserver(this);
}

TrayRotationLock::~TrayRotationLock() {
  StopObservingShell();
}

void TrayRotationLock::OnRotationLockChanged(bool rotation_locked) {
  tray_view()->SetVisible(ShouldBeVisible());
}

views::View* TrayRotationLock::CreateDefaultView(LoginStatus status) {
  if (OnPrimaryDisplay())
    return new tray::RotationLockDefaultView(this);
  return NULL;
}

void TrayRotationLock::OnMaximizeModeStarted() {
  tray_view()->SetVisible(
      Shell::GetInstance()->screen_orientation_controller()->rotation_locked());
  Shell::GetInstance()->screen_orientation_controller()->AddObserver(this);
  observing_rotation_ = true;
}

void TrayRotationLock::OnMaximizeModeEnded() {
  tray_view()->SetVisible(false);
  StopObservingRotation();
}

void TrayRotationLock::DestroyTrayView() {
  StopObservingRotation();
  StopObservingShell();
  TrayImageItem::DestroyTrayView();
}

bool TrayRotationLock::GetInitialVisibility() {
  return ShouldBeVisible();
}

bool TrayRotationLock::ShouldBeVisible() {
  return OnPrimaryDisplay() &&
         WmShell::Get()
             ->maximize_mode_controller()
             ->IsMaximizeModeWindowManagerEnabled() &&
         Shell::GetInstance()
             ->screen_orientation_controller()
             ->rotation_locked();
}

bool TrayRotationLock::OnPrimaryDisplay() const {
  gfx::NativeView native_view = system_tray()->GetWidget()->GetNativeView();
  display::Display parent_display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(native_view);
  return parent_display.IsInternal();
}

void TrayRotationLock::StopObservingRotation() {
  if (!observing_rotation_)
    return;
  ScreenOrientationController* controller =
      Shell::GetInstance()->screen_orientation_controller();
  if (controller)
    controller->RemoveObserver(this);
  observing_rotation_ = false;
}

void TrayRotationLock::StopObservingShell() {
  if (!observing_shell_)
    return;
  WmShell::Get()->RemoveShellObserver(this);
  observing_shell_ = false;
}

}  // namespace ash
