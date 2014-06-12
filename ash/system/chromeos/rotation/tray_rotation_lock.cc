// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/rotation/tray_rotation_lock.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/tray_item_more.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/display.h"

namespace ash {

namespace tray {

// Extends TrayItemMore, however does not make use of the chevron, nor of the
// DetailedView. This was chosen over ActionableView in order to reuse the
// layout and styling of labels and images. This allows RotationLockDefaultView
// to maintain the look of other system tray items without code duplication.
class RotationLockDefaultView : public TrayItemMore,
                                public ShellObserver {
 public:
  explicit RotationLockDefaultView(SystemTrayItem* owner);
  virtual ~RotationLockDefaultView();

  // ActionableView:
  virtual bool PerformAction(const ui::Event& event) OVERRIDE;

  // ShellObserver:
  virtual void OnMaximizeModeStarted() OVERRIDE;
  virtual void OnMaximizeModeEnded() OVERRIDE;

 private:
  void UpdateImage();

  DISALLOW_COPY_AND_ASSIGN(RotationLockDefaultView);
};

RotationLockDefaultView::RotationLockDefaultView(SystemTrayItem* owner)
    : TrayItemMore(owner, false) {
  UpdateImage();
  SetVisible(Shell::GetInstance()->maximize_mode_controller()->
                 IsMaximizeModeWindowManagerEnabled());
  Shell::GetInstance()->AddShellObserver(this);
}

RotationLockDefaultView::~RotationLockDefaultView() {
  Shell::GetInstance()->RemoveShellObserver(this);
}

bool RotationLockDefaultView::PerformAction(const ui::Event& event) {
  MaximizeModeController* maximize_mode_controller = Shell::GetInstance()->
      maximize_mode_controller();
  maximize_mode_controller->SetRotationLocked(
      !maximize_mode_controller->rotation_locked());
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
  base::string16 label;
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  if (Shell::GetInstance()->maximize_mode_controller()->rotation_locked()) {
    SetImage(bundle.GetImageNamed(
        IDR_AURA_UBER_TRAY_AUTO_ROTATION_LOCKED_DARK).ToImageSkia());
    label = l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ROTATION_LOCK_LOCKED);
    SetLabel(label);
    SetAccessibleName(label);
  } else {
    SetImage(bundle.GetImageNamed(IDR_AURA_UBER_TRAY_AUTO_ROTATION_DARK).
        ToImageSkia());
    label = l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ROTATION_LOCK_AUTO);
    SetLabel(label);
    SetAccessibleName(label);
  }
}

}  // namespace tray

TrayRotationLock::TrayRotationLock(SystemTray* system_tray)
    : TrayImageItem(system_tray, IDR_AURA_UBER_TRAY_AUTO_ROTATION_LOCKED),
      on_primary_display_(false) {
  gfx::NativeView native_view = system_tray->GetWidget()->GetNativeView();
  gfx::Display parent_display = Shell::GetScreen()->
      GetDisplayNearestWindow(native_view);

  on_primary_display_ = parent_display.IsInternal();

  if (on_primary_display_)
    Shell::GetInstance()->AddShellObserver(this);
}

TrayRotationLock::~TrayRotationLock() {
  if (on_primary_display_)
    Shell::GetInstance()->RemoveShellObserver(this);
}

void TrayRotationLock::OnRotationLockChanged(bool rotation_locked) {
  tray_view()->SetVisible(ShouldBeVisible());
}

views::View* TrayRotationLock::CreateDefaultView(user::LoginStatus status) {
  if (on_primary_display_)
    return new tray::RotationLockDefaultView(this);
  return NULL;
}

void TrayRotationLock::OnMaximizeModeStarted() {
  tray_view()->SetVisible(
      Shell::GetInstance()->maximize_mode_controller()->rotation_locked());
  Shell::GetInstance()->maximize_mode_controller()->AddObserver(this);
}

void TrayRotationLock::OnMaximizeModeEnded() {
  tray_view()->SetVisible(false);
  Shell::GetInstance()->maximize_mode_controller()->RemoveObserver(this);
}

bool TrayRotationLock::GetInitialVisibility() {
  return ShouldBeVisible();
}

bool TrayRotationLock::ShouldBeVisible() {
  MaximizeModeController* controller = Shell::GetInstance()->
      maximize_mode_controller();
  return on_primary_display_ &&
         controller->IsMaximizeModeWindowManagerEnabled() &&
         controller->rotation_locked();
}

}  // namespace ash
