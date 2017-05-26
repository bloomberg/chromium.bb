// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/rotation/tray_rotation_lock.h"

#include "ash/display/screen_orientation_controller_chromeos.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/actionable_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

bool IsMaximizeModeWindowManagerEnabled() {
  return Shell::Get()
      ->maximize_mode_controller()
      ->IsMaximizeModeWindowManagerEnabled();
}

bool IsUserRotationLocked() {
  return Shell::Get()->screen_orientation_controller()->user_rotation_locked();
}

bool IsCurrentRotationPortrait() {
  display::Display::Rotation current_rotation =
      ShellPort::Get()
          ->GetDisplayInfo(display::Display::InternalDisplayId())
          .GetActiveRotation();
  return current_rotation == display::Display::ROTATE_90 ||
         current_rotation == display::Display::ROTATE_270;
}

}  // namespace

namespace tray {

class RotationLockDefaultView : public ActionableView,
                                public ShellObserver,
                                public ScreenOrientationController::Observer {
 public:
  explicit RotationLockDefaultView(SystemTrayItem* owner);
  ~RotationLockDefaultView() override;

 private:
  // Updates icon and label according to current rotation lock status.
  void Update();

  // Stop observing rotation lock status.
  void StopObservingRotation();

  // ActionableView:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool PerformAction(const ui::Event& event) override;

  // ShellObserver:
  void OnMaximizeModeStarted() override;
  void OnMaximizeModeEnded() override;

  // ScreenOrientationController::Obsever:
  void OnUserRotationLockChanged() override;

  views::ImageView* icon_;
  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(RotationLockDefaultView);
};

RotationLockDefaultView::RotationLockDefaultView(SystemTrayItem* owner)
    : ActionableView(owner, TrayPopupInkDropStyle::FILL_BOUNDS),
      icon_(TrayPopupUtils::CreateMainImageView()),
      label_(TrayPopupUtils::CreateDefaultLabel()) {
  SetLayoutManager(new views::FillLayout);

  TriView* tri_view = TrayPopupUtils::CreateDefaultRowView();
  AddChildView(tri_view);

  tri_view->AddView(TriView::Container::START, icon_);
  tri_view->AddView(TriView::Container::CENTER, label_);
  tri_view->SetContainerVisible(TriView::Container::END, false);

  Update();

  SetInkDropMode(InkDropHostView::InkDropMode::ON);

  SetVisible(IsMaximizeModeWindowManagerEnabled());
  Shell::Get()->AddShellObserver(this);
  if (IsMaximizeModeWindowManagerEnabled())
    Shell::Get()->screen_orientation_controller()->AddObserver(this);
}

RotationLockDefaultView::~RotationLockDefaultView() {
  StopObservingRotation();
  Shell::Get()->RemoveShellObserver(this);
}

void RotationLockDefaultView::Update() {
  TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::DEFAULT_VIEW_LABEL);
  base::string16 label;
  if (IsUserRotationLocked()) {
    icon_->SetImage(gfx::CreateVectorIcon(
        IsCurrentRotationPortrait() ? kSystemMenuRotationLockPortraitIcon
                                    : kSystemMenuRotationLockLandscapeIcon,
        kMenuIconSize, style.GetIconColor()));
    label = l10n_util::GetStringUTF16(
        IsCurrentRotationPortrait()
            ? IDS_ASH_STATUS_TRAY_ROTATION_LOCK_PORTRAIT
            : IDS_ASH_STATUS_TRAY_ROTATION_LOCK_LANDSCAPE);
  } else {
    icon_->SetImage(gfx::CreateVectorIcon(kSystemMenuRotationLockAutoIcon,
                                          kMenuIconSize, style.GetIconColor()));
    label = l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ROTATION_LOCK_AUTO);
  }
  label_->SetText(label);
  style.SetupLabel(label_);

  Layout();
  SchedulePaint();
}

void RotationLockDefaultView::StopObservingRotation() {
  ScreenOrientationController* controller =
      Shell::Get()->screen_orientation_controller();
  if (controller)
    controller->RemoveObserver(this);
}

void RotationLockDefaultView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  ActionableView::GetAccessibleNodeData(node_data);
  if (!label_->text().empty())
    node_data->SetName(label_->text());
}

bool RotationLockDefaultView::PerformAction(const ui::Event& event) {
  Shell::Get()->screen_orientation_controller()->ToggleUserRotationLock();
  return true;
}

void RotationLockDefaultView::OnMaximizeModeStarted() {
  Update();
  SetVisible(true);
  Shell::Get()->screen_orientation_controller()->AddObserver(this);
}

void RotationLockDefaultView::OnMaximizeModeEnded() {
  SetVisible(false);
  StopObservingRotation();
}

void RotationLockDefaultView::OnUserRotationLockChanged() {
  Update();
}

}  // namespace tray

TrayRotationLock::TrayRotationLock(SystemTray* system_tray)
    : TrayImageItem(system_tray,
                    kSystemTrayRotationLockLockedIcon,
                    UMA_ROTATION_LOCK) {
  Shell::Get()->AddShellObserver(this);
}

TrayRotationLock::~TrayRotationLock() {
  Shell::Get()->RemoveShellObserver(this);
}

void TrayRotationLock::OnUserRotationLockChanged() {
  UpdateTrayImage();
}

views::View* TrayRotationLock::CreateDefaultView(LoginStatus status) {
  if (OnPrimaryDisplay())
    return new tray::RotationLockDefaultView(this);
  return nullptr;
}

void TrayRotationLock::OnMaximizeModeStarted() {
  tray_view()->SetVisible(ShouldBeVisible());
  UpdateTrayImage();
  Shell::Get()->screen_orientation_controller()->AddObserver(this);
}

void TrayRotationLock::OnMaximizeModeEnded() {
  tray_view()->SetVisible(false);
  StopObservingRotation();
}

void TrayRotationLock::DestroyTrayView() {
  StopObservingRotation();
  Shell::Get()->RemoveShellObserver(this);
  TrayImageItem::DestroyTrayView();
}

bool TrayRotationLock::GetInitialVisibility() {
  return ShouldBeVisible();
}

void TrayRotationLock::UpdateTrayImage() {
  TrayImageItem::SetImageIcon(IsUserRotationLocked()
                                  ? kSystemTrayRotationLockLockedIcon
                                  : kSystemTrayRotationLockAutoIcon);
}

bool TrayRotationLock::ShouldBeVisible() {
  return OnPrimaryDisplay() && IsMaximizeModeWindowManagerEnabled();
}

bool TrayRotationLock::OnPrimaryDisplay() const {
  gfx::NativeView native_window = system_tray()->GetWidget()->GetNativeWindow();
  display::Display parent_display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(native_window);
  return parent_display.IsInternal();
}

void TrayRotationLock::StopObservingRotation() {
  ScreenOrientationController* controller =
      Shell::Get()->screen_orientation_controller();
  if (controller)
    controller->RemoveObserver(this);
}

}  // namespace ash
