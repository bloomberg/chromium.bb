// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/update/tray_update.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/metrics/user_metrics_action.h"
#include "ash/common/system/tray/fixed_sized_image_view.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/system_tray_controller.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_item_style.h"
#include "ash/common/system/tray/tray_popup_utils.h"
#include "ash/common/wm_shell.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace {

// Decides the non-material design image resource to use for a given update
// severity.
// TODO(tdanderson): This is only used for non-material design, so remove it
// when material design is the default. See crbug.com/625692.
int DecideResource(UpdateInfo::UpdateSeverity severity, bool dark) {
  switch (severity) {
    case UpdateInfo::UPDATE_NONE:
    case UpdateInfo::UPDATE_LOW:
      return dark ? IDR_AURA_UBER_TRAY_UPDATE_DARK : IDR_AURA_UBER_TRAY_UPDATE;

    case UpdateInfo::UPDATE_ELEVATED:
      return dark ? IDR_AURA_UBER_TRAY_UPDATE_DARK_GREEN
                  : IDR_AURA_UBER_TRAY_UPDATE_GREEN;

    case UpdateInfo::UPDATE_HIGH:
      return dark ? IDR_AURA_UBER_TRAY_UPDATE_DARK_ORANGE
                  : IDR_AURA_UBER_TRAY_UPDATE_ORANGE;

    case UpdateInfo::UPDATE_SEVERE:
    case UpdateInfo::UPDATE_CRITICAL:
      return dark ? IDR_AURA_UBER_TRAY_UPDATE_DARK_RED
                  : IDR_AURA_UBER_TRAY_UPDATE_RED;
  }

  NOTREACHED() << "Unknown update severity level.";
  return 0;
}

// Returns the color to use for the material design update icon when the update
// severity is |severity|. If |for_menu| is true, the icon color for the system
// menu is given, otherwise the icon color for the system tray is given.
SkColor IconColorForUpdateSeverity(UpdateInfo::UpdateSeverity severity,
                                   bool for_menu) {
  const SkColor default_color = for_menu ? kMenuIconColor : kTrayIconColor;
  switch (severity) {
    case UpdateInfo::UPDATE_NONE:
      return default_color;
    case UpdateInfo::UPDATE_LOW:
      return for_menu ? gfx::kGoogleGreen700 : kTrayIconColor;
    case UpdateInfo::UPDATE_ELEVATED:
      return for_menu ? gfx::kGoogleYellow700 : gfx::kGoogleYellow300;
    case UpdateInfo::UPDATE_HIGH:
    case UpdateInfo::UPDATE_SEVERE:
    case UpdateInfo::UPDATE_CRITICAL:
      return for_menu ? gfx::kGoogleRed700 : gfx::kGoogleRed300;
    default:
      NOTREACHED();
      break;
  }
  return default_color;
}

class UpdateView : public ActionableView {
 public:
  UpdateView(SystemTrayItem* owner, const UpdateInfo& info)
      : ActionableView(owner, TrayPopupInkDropStyle::FILL_BOUNDS),
        label_(nullptr) {
    SetLayoutManager(new views::FillLayout);

    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    TriView* tri_view = TrayPopupUtils::CreateDefaultRowView();
    AddChildView(tri_view);
    views::ImageView* image = TrayPopupUtils::CreateMainImageView();
    if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
      image->SetImage(gfx::CreateVectorIcon(
          kSystemMenuUpdateIcon,
          IconColorForUpdateSeverity(info.severity, true)));
    } else {
      image->SetImage(bundle.GetImageNamed(DecideResource(info.severity, true))
                          .ToImageSkia());
    }
    tri_view->AddView(TriView::Container::START, image);

    base::string16 label_text =
        info.factory_reset_required
            ? bundle.GetLocalizedString(
                  IDS_ASH_STATUS_TRAY_RESTART_AND_POWERWASH_TO_UPDATE)
            : bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_UPDATE);
    label_ = TrayPopupUtils::CreateDefaultLabel();
    label_->SetText(label_text);
    SetAccessibleName(label_text);
    tri_view->AddView(TriView::Container::CENTER, label_);

    if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
      UpdateStyle();
      SetInkDropMode(InkDropHostView::InkDropMode::ON);
    }
  }

  ~UpdateView() override {}

 private:
  void UpdateStyle() {
    TrayPopupItemStyle style(GetNativeTheme(),
                             TrayPopupItemStyle::FontStyle::DEFAULT_VIEW_LABEL);
    style.SetupLabel(label_);
  }

  // Overridden from ActionableView.
  bool PerformAction(const ui::Event& event) override {
    WmShell::Get()->system_tray_controller()->RequestRestartForUpdate();
    WmShell::Get()->RecordUserMetricsAction(
        UMA_STATUS_AREA_OS_UPDATE_DEFAULT_SELECTED);
    CloseSystemBubble();
    return true;
  }

  void OnNativeThemeChanged(const ui::NativeTheme* theme) override {
    ActionableView::OnNativeThemeChanged(theme);

    if (!MaterialDesignController::IsSystemTrayMenuMaterial())
      return;
    UpdateStyle();
  }

  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(UpdateView);
};

}  // namespace

TrayUpdate::TrayUpdate(SystemTray* system_tray)
    : TrayImageItem(system_tray, IDR_AURA_UBER_TRAY_UPDATE, UMA_UPDATE) {
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
  return info.update_required ? new UpdateView(this, info) : nullptr;
}

void TrayUpdate::OnUpdateRecommended(const UpdateInfo& info) {
  if (MaterialDesignController::UseMaterialDesignSystemIcons())
    SetIconColor(IconColorForUpdateSeverity(info.severity, false));
  else
    SetImageFromResourceId(DecideResource(info.severity, false));
  tray_view()->SetVisible(true);
}

}  // namespace ash
