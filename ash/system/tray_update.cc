// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray_update.h"

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/fixed_sized_image_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace {

// How many seconds should we wait before showing the nag reminder?
const int kUpdateNaggingTimeSeconds = 24 * 60 * 60;

// How long should the nag reminder be displayed?
const int kShowUpdateNaggerForSeconds = 15;

int DecideResource(ash::UpdateInfo::UpdateSeverity severity, bool dark) {
  switch (severity) {
    case ash::UpdateInfo::UPDATE_NORMAL:
      return dark ? IDR_AURA_UBER_TRAY_UPDATE_DARK:
                    IDR_AURA_UBER_TRAY_UPDATE;

    case ash::UpdateInfo::UPDATE_LOW_GREEN:
      return dark ? IDR_AURA_UBER_TRAY_UPDATE_DARK_GREEN :
                    IDR_AURA_UBER_TRAY_UPDATE_GREEN;

    case ash::UpdateInfo::UPDATE_HIGH_ORANGE:
      return dark ? IDR_AURA_UBER_TRAY_UPDATE_DARK_ORANGE :
                    IDR_AURA_UBER_TRAY_UPDATE_ORANGE;

    case ash::UpdateInfo::UPDATE_SEVERE_RED:
      return dark ? IDR_AURA_UBER_TRAY_UPDATE_DARK_RED :
                    IDR_AURA_UBER_TRAY_UPDATE_RED;
  }

  NOTREACHED() << "Unknown update severity level.";
  return 0;
}

class UpdateView : public ash::ActionableView {
 public:
  explicit UpdateView(const ash::UpdateInfo& info) {
    SetLayoutManager(new
        views::BoxLayout(views::BoxLayout::kHorizontal,
        ash::kTrayPopupPaddingHorizontal, 0,
        ash::kTrayPopupPaddingBetweenItems));

    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    views::ImageView* image =
        new ash::FixedSizedImageView(0, ash::kTrayPopupItemHeight);
    image->SetImage(bundle.GetImageNamed(DecideResource(info.severity, true))
                        .ToImageSkia());

    AddChildView(image);

    base::string16 label =
        info.factory_reset_required
            ? bundle.GetLocalizedString(
                  IDS_ASH_STATUS_TRAY_RESTART_AND_POWERWASH_TO_UPDATE)
            : bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_UPDATE);
    AddChildView(new views::Label(label));
    SetAccessibleName(label);
  }

  ~UpdateView() override {}

 private:
  // Overridden from ActionableView.
  bool PerformAction(const ui::Event& event) override {
    ash::Shell::GetInstance()->
        system_tray_delegate()->RequestRestartForUpdate();
    return true;
  }

  DISALLOW_COPY_AND_ASSIGN(UpdateView);
};

}

namespace ash {
namespace tray {

class UpdateNagger : public ui::LayerAnimationObserver {
 public:
  explicit UpdateNagger(SystemTrayItem* owner)
      : owner_(owner) {
    RestartTimer();
    owner_->system_tray()->GetWidget()->GetNativeView()->layer()->
        GetAnimator()->AddObserver(this);
  }

  ~UpdateNagger() override {
    StatusAreaWidget* status_area =
        Shell::GetPrimaryRootWindowController()->shelf()->status_area_widget();
    if (status_area) {
      status_area->system_tray()->GetWidget()->GetNativeView()->layer()->
          GetAnimator()->RemoveObserver(this);
    }
  }

  void RestartTimer() {
    timer_.Stop();
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromSeconds(kUpdateNaggingTimeSeconds),
                 this,
                 &UpdateNagger::Nag);
  }

 private:
  void Nag() {
    owner_->PopupDetailedView(kShowUpdateNaggerForSeconds, false);
  }

  // Overridden from ui::LayerAnimationObserver.
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override {
    // TODO(oshima): Find out if the updator will be shown on non
    // primary display.
    if (Shell::GetPrimaryRootWindowController()->shelf()->IsVisible())
      timer_.Stop();
    else if (!timer_.IsRunning())
      RestartTimer();
  }

  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {}

  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}

  SystemTrayItem* owner_;
  base::OneShotTimer<UpdateNagger> timer_;

  DISALLOW_COPY_AND_ASSIGN(UpdateNagger);
};

}  // namespace tray

TrayUpdate::TrayUpdate(SystemTray* system_tray)
    : TrayImageItem(system_tray, IDR_AURA_UBER_TRAY_UPDATE) {
  Shell::GetInstance()->system_tray_notifier()->AddUpdateObserver(this);
}

TrayUpdate::~TrayUpdate() {
  Shell::GetInstance()->system_tray_notifier()->RemoveUpdateObserver(this);
}

bool TrayUpdate::GetInitialVisibility() {
  UpdateInfo info;
  Shell::GetInstance()->system_tray_delegate()->GetSystemUpdateInfo(&info);
  return info.update_required;
}

views::View* TrayUpdate::CreateDefaultView(user::LoginStatus status) {
  UpdateInfo info;
  Shell::GetInstance()->system_tray_delegate()->GetSystemUpdateInfo(&info);
  return info.update_required ? new UpdateView(info) : nullptr;
}

views::View* TrayUpdate::CreateDetailedView(user::LoginStatus status) {
  return CreateDefaultView(status);
}

void TrayUpdate::DestroyDetailedView() {
  if (nagger_) {
    // The nagger was being displayed. Now that the detailed view is being
    // closed, that means either the user clicks on it to restart, or the user
    // didn't click on it to restart. In either case, start the timer to show
    // the nag reminder again after the specified time.
    nagger_->RestartTimer();
  }
}

void TrayUpdate::OnUpdateRecommended(const UpdateInfo& info) {
  SetImageFromResourceId(DecideResource(info.severity, false));
  tray_view()->SetVisible(true);
  if (!Shell::GetPrimaryRootWindowController()->shelf()->IsVisible() &&
      !nagger_.get()) {
    // The shelf is not visible, and there is no nagger scheduled.
    nagger_.reset(new tray::UpdateNagger(this));
  }
}

}  // namespace ash
