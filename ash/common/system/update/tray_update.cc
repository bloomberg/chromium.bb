// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/update/tray_update.h"

#include "ash/common/metrics/user_metrics_action.h"
#include "ash/common/system/tray/fixed_sized_image_view.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/wm_shell.h"
#include "ash/system/tray/system_tray.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace {

// TODO(tdanderson): The material design update icon needs to be colored
// programmatically. See crbug.com/625692.
int DecideResource(UpdateInfo::UpdateSeverity severity, bool dark) {
  switch (severity) {
    case UpdateInfo::UPDATE_NORMAL:
      return dark ? IDR_AURA_UBER_TRAY_UPDATE_DARK : IDR_AURA_UBER_TRAY_UPDATE;

    case UpdateInfo::UPDATE_LOW_GREEN:
      return dark ? IDR_AURA_UBER_TRAY_UPDATE_DARK_GREEN
                  : IDR_AURA_UBER_TRAY_UPDATE_GREEN;

    case UpdateInfo::UPDATE_HIGH_ORANGE:
      return dark ? IDR_AURA_UBER_TRAY_UPDATE_DARK_ORANGE
                  : IDR_AURA_UBER_TRAY_UPDATE_ORANGE;

    case UpdateInfo::UPDATE_SEVERE_RED:
      return dark ? IDR_AURA_UBER_TRAY_UPDATE_DARK_RED
                  : IDR_AURA_UBER_TRAY_UPDATE_RED;
  }

  NOTREACHED() << "Unknown update severity level.";
  return 0;
}

class UpdateView : public ActionableView {
 public:
  explicit UpdateView(const UpdateInfo& info) {
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
                                          kTrayPopupPaddingHorizontal, 0,
                                          kTrayPopupPaddingBetweenItems));

    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    views::ImageView* image = new FixedSizedImageView(0, kTrayPopupItemHeight);
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
    WmShell::Get()->system_tray_delegate()->RequestRestartForUpdate();
    WmShell::Get()->RecordUserMetricsAction(
        UMA_STATUS_AREA_OS_UPDATE_DEFAULT_SELECTED);
    return true;
  }

  DISALLOW_COPY_AND_ASSIGN(UpdateView);
};

}  // namespace

TrayUpdate::TrayUpdate(SystemTray* system_tray)
    : TrayImageItem(system_tray, IDR_AURA_UBER_TRAY_UPDATE) {
  WmShell::Get()->system_tray_notifier()->AddUpdateObserver(this);
}

TrayUpdate::~TrayUpdate() {
  WmShell::Get()->system_tray_notifier()->RemoveUpdateObserver(this);
}

bool TrayUpdate::GetInitialVisibility() {
  UpdateInfo info;
  WmShell::Get()->system_tray_delegate()->GetSystemUpdateInfo(&info);
  return info.update_required;
}

views::View* TrayUpdate::CreateDefaultView(LoginStatus status) {
  UpdateInfo info;
  WmShell::Get()->system_tray_delegate()->GetSystemUpdateInfo(&info);
  return info.update_required ? new UpdateView(info) : nullptr;
}

void TrayUpdate::OnUpdateRecommended(const UpdateInfo& info) {
  SetImageFromResourceId(DecideResource(info.severity, false));
  tray_view()->SetVisible(true);
}

}  // namespace ash
