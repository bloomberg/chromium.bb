// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/arc_app_deferred_launcher_controller.h"

#include "chrome/browser/chromeos/arc/arc_support_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/arc_app_deferred_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
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
      const std::string& app_id,
      const gfx::ImageSkia& image)
      : gfx::CanvasImageSource(image.size(), false /* is opaque */),
        host_(host),
        app_id_(app_id),
        image_(image) {}

  ~SpinningEffectSource() override {}

  // gfx::CanvasImageSource override.
  void Draw(gfx::Canvas* canvas) override {
    if (!host_)
      return;

    canvas->DrawImageInt(image_, 0, 0);

    const int gap = kSpinningGapPercent * image_.width() / 100;
    gfx::PaintThrobberSpinning(canvas,
                               gfx::Rect(gap, gap, image_.width() - 2 * gap,
                                         image_.height() - 2 * gap),
                               SK_ColorWHITE, host_->GetActiveTime(app_id_));
  }

 private:
  base::WeakPtr<ArcAppDeferredLauncherController> host_;
  const std::string app_id_;
  const gfx::ImageSkia image_;

  DISALLOW_COPY_AND_ASSIGN(SpinningEffectSource);
};
}  // namespace

ArcAppDeferredLauncherController::ArcAppDeferredLauncherController(
    ChromeLauncherController* owner)
    : owner_(owner), weak_ptr_factory_(this) {
  ArcAppListPrefs::Get(owner_->profile())->AddObserver(this);
}

ArcAppDeferredLauncherController::~ArcAppDeferredLauncherController() {
  ArcAppListPrefs::Get(owner_->profile())->RemoveObserver(this);
}

void ArcAppDeferredLauncherController::Apply(const std::string& app_id,
                                             gfx::ImageSkia* image) {
  DCHECK(image);
  if (app_controller_map_.find(app_id) == app_controller_map_.end())
    return;

  const color_utils::HSL shift = {-1, 0, 0.25};
  *image = gfx::ImageSkia(
      new SpinningEffectSource(
          weak_ptr_factory_.GetWeakPtr(), app_id,
          gfx::ImageSkiaOperations::CreateTransparentImage(
              gfx::ImageSkiaOperations::CreateHSLShiftedImage(*image, shift),
              0.5)),
      image->size());
}

void ArcAppDeferredLauncherController::Close(const std::string& app_id) {
  AppControllerMap::const_iterator it = app_controller_map_.find(app_id);
  if (it == app_controller_map_.end())
    return;

  const ash::ShelfID shelf_id = owner_->GetShelfIDForAppID(app_id);
  const bool need_close_item =
      it->second == owner_->GetLauncherItemController(shelf_id);
  app_controller_map_.erase(it);
  if (need_close_item)
    owner_->CloseLauncherItem(shelf_id);
}

void ArcAppDeferredLauncherController::OnAppReadyChanged(
    const std::string& app_id,
    bool ready) {
  if (!ready)
    return;

  AppControllerMap::const_iterator it = app_controller_map_.find(app_id);
  if (it == app_controller_map_.end())
    return;

  Close(app_id);

  arc::LaunchApp(owner_->profile(), app_id);
}

void ArcAppDeferredLauncherController::OnAppRemoved(const std::string& app_id) {
  Close(app_id);
}

base::TimeDelta ArcAppDeferredLauncherController::GetActiveTime(
    const std::string& app_id) const {
  AppControllerMap::const_iterator it = app_controller_map_.find(app_id);
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
    const std::string& app_id) {
  const std::string shelf_app_id =
      app_id == arc::kPlayStoreAppId ? ArcSupportHost::kHostAppId : app_id;
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
      new ArcAppDeferredLauncherItemController(app_id, owner_,
                                               weak_ptr_factory_.GetWeakPtr());
  if (shelf_id == 0) {
    owner_->CreateAppLauncherItem(controller, shelf_app_id,
                                  ash::STATUS_RUNNING);
  } else {
    owner_->SetItemController(shelf_id, controller);
    owner_->SetItemStatus(shelf_id, ash::STATUS_RUNNING);
  }

  if (app_controller_map_.empty())
    RegisterNextUpdate();

  app_controller_map_[app_id] = controller;
}
