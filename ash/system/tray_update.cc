// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray_update.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_views.h"
#include "ash/wm/shelf_layout_manager.h"
#include "base/time.h"
#include "base/timer.h"
#include "grit/ash_strings.h"
#include "grit/ui_resources.h"
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

int DecideResource(ash::UpdateObserver::UpdateSeverity severity, bool dark) {
  switch (severity) {
    case ash::UpdateObserver::UPDATE_NORMAL:
      return dark ? IDR_AURA_UBER_TRAY_UPDATE_DARK:
                    IDR_AURA_UBER_TRAY_UPDATE;

    case ash::UpdateObserver::UPDATE_LOW_GREEN:
      return dark ? IDR_AURA_UBER_TRAY_UPDATE_DARK_GREEN :
                    IDR_AURA_UBER_TRAY_UPDATE_GREEN;

    case ash::UpdateObserver::UPDATE_HIGH_ORANGE:
      return dark ? IDR_AURA_UBER_TRAY_UPDATE_DARK_ORANGE :
                    IDR_AURA_UBER_TRAY_UPDATE_ORANGE;

    case ash::UpdateObserver::UPDATE_SEVERE_RED:
      return dark ? IDR_AURA_UBER_TRAY_UPDATE_DARK_RED :
                    IDR_AURA_UBER_TRAY_UPDATE_RED;
  }

  NOTREACHED() << "Unknown update severity level.";
  return 0;
}

class UpdateView : public ash::internal::ActionableView {
 public:
  explicit UpdateView(ash::UpdateObserver::UpdateSeverity severity) {
    SetLayoutManager(new
        views::BoxLayout(views::BoxLayout::kHorizontal,
        ash::kTrayPopupPaddingHorizontal, 0,
        ash::kTrayPopupPaddingBetweenItems));

    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    views::ImageView* image =
        new ash::internal::FixedSizedImageView(0, ash::kTrayPopupItemHeight);
    image->SetImage(bundle.GetImageNamed(DecideResource(severity, true)).
        ToImageSkia());

    AddChildView(image);
    AddChildView(new views::Label(
        bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_UPDATE)));
    SetAccessibleName(bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_UPDATE));
  }

  virtual ~UpdateView() {}

 private:
  // Overridden from ActionableView.
  virtual bool PerformAction(const ui::Event& event) OVERRIDE {
    ash::Shell::GetInstance()->tray_delegate()->RequestRestart();
    return true;
  }

  DISALLOW_COPY_AND_ASSIGN(UpdateView);
};

}

namespace ash {
namespace internal {

namespace tray {

class UpdateNagger : public ui::LayerAnimationObserver {
 public:
  explicit UpdateNagger(SystemTrayItem* owner)
      : owner_(owner) {
    RestartTimer();
    Shell::GetInstance()->system_tray()->GetWidget()->GetNativeView()->layer()->
        GetAnimator()->AddObserver(this);
  }

  virtual ~UpdateNagger() {
    Shell::GetInstance()->system_tray()->GetWidget()->GetNativeView()->layer()->
        GetAnimator()->RemoveObserver(this);
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
  virtual void OnLayerAnimationEnded(
      ui::LayerAnimationSequence* sequence) OVERRIDE {
    if (Shell::GetInstance()->shelf()->IsVisible())
      timer_.Stop();
    else if (!timer_.IsRunning())
      RestartTimer();
  }

  virtual void OnLayerAnimationAborted(
      ui::LayerAnimationSequence* sequence) OVERRIDE {}

  virtual void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) OVERRIDE {}

  SystemTrayItem* owner_;
  base::OneShotTimer<UpdateNagger> timer_;

  DISALLOW_COPY_AND_ASSIGN(UpdateNagger);
};

}  // namespace tray

TrayUpdate::TrayUpdate()
    : TrayImageItem(IDR_AURA_UBER_TRAY_UPDATE),
      severity_(UpdateObserver::UPDATE_NORMAL) {
}

TrayUpdate::~TrayUpdate() {}

bool TrayUpdate::GetInitialVisibility() {
  return Shell::GetInstance()->tray_delegate()->SystemShouldUpgrade();
}

views::View* TrayUpdate::CreateDefaultView(user::LoginStatus status) {
  if (!Shell::GetInstance()->tray_delegate()->SystemShouldUpgrade())
    return NULL;
  return new UpdateView(severity_);
}

views::View* TrayUpdate::CreateDetailedView(user::LoginStatus status) {
  return CreateDefaultView(status);
}

void TrayUpdate::DestroyDetailedView() {
  if (nagger_.get()) {
    // The nagger was being displayed. Now that the detailed view is being
    // closed, that means either the user clicks on it to restart, or the user
    // didn't click on it to restart. In either case, start the timer to show
    // the nag reminder again after the specified time.
    nagger_->RestartTimer();
  }
}

void TrayUpdate::OnUpdateRecommended(UpdateObserver::UpdateSeverity severity) {
  severity_ = severity;
  SetImageFromResourceId(DecideResource(severity_, false));
  tray_view()->SetVisible(true);
  if (!Shell::GetInstance()->shelf()->IsVisible() && !nagger_.get()) {
    // The shelf is not visible, and there is no nagger scheduled.
    nagger_.reset(new tray::UpdateNagger(this));
  }
}

}  // namespace internal
}  // namespace ash
