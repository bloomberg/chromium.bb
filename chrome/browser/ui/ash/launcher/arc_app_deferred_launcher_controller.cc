// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/arc_app_deferred_launcher_controller.h"

#include "chrome/browser/chromeos/arc/arc_support_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/arc_app_deferred_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_impl.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_throbber.h"

namespace {

constexpr int kUpdateIconIntervalMs = 40;  // 40ms for 25 frames per second.
constexpr int kSpinningGapPercent = 25;

class SpinningEffectSource : public gfx::CanvasImageSource {
 public:
  SpinningEffectSource(
      const base::WeakPtr<ArcAppDeferredLauncherController>& host,
      const std::string& shelf_app_id,
      const gfx::ImageSkia& image)
      : gfx::CanvasImageSource(image.size(), false /* is opaque */),
        host_(host),
        shelf_app_id_(shelf_app_id),
        image_(image) {}

  ~SpinningEffectSource() override {}

  // gfx::CanvasImageSource override.
  void Draw(gfx::Canvas* canvas) override {
    if (!host_)
      return;

    canvas->DrawImageInt(image_, 0, 0);

    const int gap = kSpinningGapPercent * image_.width() / 100;
    gfx::PaintThrobberSpinning(
        canvas, gfx::Rect(gap, gap, image_.width() - 2 * gap,
                          image_.height() - 2 * gap),
        SK_ColorWHITE, host_->GetActiveTime(shelf_app_id_));
  }

 private:
  base::WeakPtr<ArcAppDeferredLauncherController> host_;
  const std::string shelf_app_id_;
  const gfx::ImageSkia image_;

  DISALLOW_COPY_AND_ASSIGN(SpinningEffectSource);
};
}  // namespace

ArcAppDeferredLauncherController::ArcAppDeferredLauncherController(
    ChromeLauncherControllerImpl* owner)
    : owner_(owner), weak_ptr_factory_(this) {
  if (arc::ArcSessionManager::IsAllowedForProfile(owner->profile())) {
    observed_profile_ = owner->profile();
    ArcAppListPrefs::Get(observed_profile_)->AddObserver(this);
  }
  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  // arc::ArcSessionManager might not be set in tests.
  if (arc_session_manager)
    arc_session_manager->AddObserver(this);
}

ArcAppDeferredLauncherController::~ArcAppDeferredLauncherController() {
  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  // arc::ArcSessionManager may be released first.
  if (arc_session_manager)
    arc_session_manager->RemoveObserver(this);
  if (observed_profile_)
    ArcAppListPrefs::Get(observed_profile_)->RemoveObserver(this);
}

void ArcAppDeferredLauncherController::MaybeApplySpinningEffect(
    const std::string& shelf_app_id,
    gfx::ImageSkia* image) {
  DCHECK(image);
  if (app_controller_map_.find(shelf_app_id) == app_controller_map_.end())
    return;

  const color_utils::HSL shift = {-1, 0, 0.25};
  *image = gfx::ImageSkia(
      new SpinningEffectSource(
          weak_ptr_factory_.GetWeakPtr(), shelf_app_id,
          gfx::ImageSkiaOperations::CreateTransparentImage(
              gfx::ImageSkiaOperations::CreateHSLShiftedImage(*image, shift),
              0.5)),
      image->size());
}

void ArcAppDeferredLauncherController::Remove(const std::string& app_id) {
  const std::string shelf_app_id =
      ArcAppWindowLauncherController::GetShelfAppIdFromArcAppId(app_id);
  app_controller_map_.erase(shelf_app_id);
}

void ArcAppDeferredLauncherController::Close(const std::string& app_id) {
  const std::string shelf_app_id =
      ArcAppWindowLauncherController::GetShelfAppIdFromArcAppId(app_id);
  AppControllerMap::const_iterator it = app_controller_map_.find(shelf_app_id);
  if (it == app_controller_map_.end())
    return;

  const ash::ShelfID shelf_id = owner_->GetShelfIDForAppID(shelf_app_id);
  const bool need_close_item =
      it->second == owner_->GetLauncherItemController(shelf_id);
  app_controller_map_.erase(it);
  if (need_close_item)
    owner_->CloseLauncherItem(shelf_id);
  owner_->OnAppUpdated(owner_->profile(), shelf_app_id);
}

void ArcAppDeferredLauncherController::OnAppReadyChanged(
    const std::string& app_id,
    bool ready) {
  if (!ready || app_controller_map_.empty())
    return;

  const std::string shelf_app_id =
      ArcAppWindowLauncherController::GetShelfAppIdFromArcAppId(app_id);
  AppControllerMap::const_iterator it = app_controller_map_.find(shelf_app_id);
  if (it == app_controller_map_.end())
    return;

  // Preserve the event flags before |it| is invalidated in Close().
  int event_flags = it->second->event_flags();
  Close(app_id);

  arc::LaunchApp(observed_profile_, app_id, event_flags);
}

void ArcAppDeferredLauncherController::OnAppRemoved(const std::string& app_id) {
  Close(app_id);
}

void ArcAppDeferredLauncherController::OnOptInEnabled(bool enabled) {
  if (enabled)
    return;

  // If Arc was disabled, remove all deferred launch requests.
  while (!app_controller_map_.empty())
    Close(app_controller_map_.begin()->first);
}

bool ArcAppDeferredLauncherController::HasApp(const std::string& app_id) const {
  const std::string shelf_app_id =
      ArcAppWindowLauncherController::GetShelfAppIdFromArcAppId(app_id);
  return app_controller_map_.count(shelf_app_id);
}

base::TimeDelta ArcAppDeferredLauncherController::GetActiveTime(
    const std::string& shelf_app_id) const {
  AppControllerMap::const_iterator it = app_controller_map_.find(shelf_app_id);
  if (it == app_controller_map_.end())
    return base::TimeDelta();

  return it->second->GetActiveTime();
}

void ArcAppDeferredLauncherController::UpdateApps() {
  if (app_controller_map_.empty())
    return;

  RegisterNextUpdate();
  for (const auto pair : app_controller_map_)
    owner_->OnAppUpdated(owner_->profile(), pair.first);
}

void ArcAppDeferredLauncherController::RegisterNextUpdate() {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&ArcAppDeferredLauncherController::UpdateApps,
                            weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kUpdateIconIntervalMs));
}

void ArcAppDeferredLauncherController::RegisterDeferredLaunch(
    const std::string& app_id,
    int event_flags) {
  const arc::ArcSessionManager* arc_session_manager =
      arc::ArcSessionManager::Get();
  DCHECK(arc_session_manager);
  DCHECK(arc_session_manager->state() !=
         arc::ArcSessionManager::State::STOPPED);
  DCHECK(arc_session_manager->state() !=
         arc::ArcSessionManager::State::NOT_INITIALIZED);

  const std::string shelf_app_id =
      ArcAppWindowLauncherController::GetShelfAppIdFromArcAppId(app_id);
  const ash::ShelfID shelf_id = owner_->GetShelfIDForAppID(shelf_app_id);

  if (shelf_id) {
    LauncherItemController* controller =
        owner_->GetLauncherItemController(shelf_id);
    if (controller &&
        controller->type() != LauncherItemController::TYPE_SHORTCUT) {
      // We are allowed to apply new deferred controller only over shortcut.
      return;
    }
  }

  ArcAppDeferredLauncherItemController* controller =
      new ArcAppDeferredLauncherItemController(
          shelf_app_id, owner_, event_flags, weak_ptr_factory_.GetWeakPtr());
  if (shelf_id == 0) {
    owner_->CreateAppLauncherItem(controller, shelf_app_id,
                                  ash::STATUS_RUNNING);
  } else {
    owner_->SetItemController(shelf_id, controller);
    owner_->SetItemStatus(shelf_id, ash::STATUS_RUNNING);
  }

  if (app_controller_map_.empty())
    RegisterNextUpdate();

  app_controller_map_[shelf_app_id] = controller;
}
