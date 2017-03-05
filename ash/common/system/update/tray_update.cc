// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/update/tray_update.h"

#include "ash/common/metrics/user_metrics_action.h"
#include "ash/common/system/tray/fixed_sized_image_view.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/system_tray_controller.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_item_style.h"
#include "ash/common/system/tray/tray_popup_utils.h"
#include "ash/common/wm_shell.h"
#include "ash/public/interfaces/update.mojom.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace {

// Returns the color to use for the update icon when the update severity is
// |severity|. If |for_menu| is true, the icon color for the system menu is
// given, otherwise the icon color for the system tray is given.
SkColor IconColorForUpdateSeverity(mojom::UpdateSeverity severity,
                                   bool for_menu) {
  const SkColor default_color = for_menu ? kMenuIconColor : kTrayIconColor;
  switch (severity) {
    case mojom::UpdateSeverity::NONE:
      return default_color;
    case mojom::UpdateSeverity::LOW:
      return for_menu ? gfx::kGoogleGreen700 : kTrayIconColor;
    case mojom::UpdateSeverity::ELEVATED:
      return for_menu ? gfx::kGoogleYellow700 : gfx::kGoogleYellow300;
    case mojom::UpdateSeverity::HIGH:
    case mojom::UpdateSeverity::SEVERE:
    case mojom::UpdateSeverity::CRITICAL:
      return for_menu ? gfx::kGoogleRed700 : gfx::kGoogleRed300;
    default:
      NOTREACHED();
      break;
  }
  return default_color;
}

}  // namespace

// static
bool TrayUpdate::update_required_ = false;
// static
mojom::UpdateSeverity TrayUpdate::severity_ = mojom::UpdateSeverity::NONE;
// static
bool TrayUpdate::factory_reset_required_ = false;

// The "restart to update" item in the system tray menu.
class TrayUpdate::UpdateView : public ActionableView {
 public:
  explicit UpdateView(TrayUpdate* owner)
      : ActionableView(owner, TrayPopupInkDropStyle::FILL_BOUNDS) {
    SetLayoutManager(new views::FillLayout);

    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    TriView* tri_view = TrayPopupUtils::CreateDefaultRowView();
    AddChildView(tri_view);
    views::ImageView* image = TrayPopupUtils::CreateMainImageView();
    image->SetImage(gfx::CreateVectorIcon(
        kSystemMenuUpdateIcon,
        IconColorForUpdateSeverity(owner->severity_, true)));
    tri_view->AddView(TriView::Container::START, image);

    base::string16 label_text =
        owner->factory_reset_required_
            ? bundle.GetLocalizedString(
                  IDS_ASH_STATUS_TRAY_RESTART_AND_POWERWASH_TO_UPDATE)
            : bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_UPDATE);
    SetAccessibleName(label_text);
    auto* label = TrayPopupUtils::CreateDefaultLabel();
    label->SetText(label_text);
    TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::DEFAULT_VIEW_LABEL);
    style.SetupLabel(label);
    tri_view->AddView(TriView::Container::CENTER, label);

    SetInkDropMode(InkDropHostView::InkDropMode::ON);
  }

  ~UpdateView() override {}

 private:
  // Overridden from ActionableView.
  bool PerformAction(const ui::Event& event) override {
    WmShell::Get()->system_tray_controller()->RequestRestartForUpdate();
    WmShell::Get()->RecordUserMetricsAction(
        UMA_STATUS_AREA_OS_UPDATE_DEFAULT_SELECTED);
    CloseSystemBubble();
    return true;
  }

  DISALLOW_COPY_AND_ASSIGN(UpdateView);
};

TrayUpdate::TrayUpdate(SystemTray* system_tray)
    : TrayImageItem(system_tray, kSystemTrayUpdateIcon, UMA_UPDATE) {}

TrayUpdate::~TrayUpdate() {}

bool TrayUpdate::GetInitialVisibility() {
  // If chrome tells ash there is an update available before this item's system
  // tray is constructed then show the icon.
  return update_required_;
}

views::View* TrayUpdate::CreateDefaultView(LoginStatus status) {
  return update_required_ ? new UpdateView(this) : nullptr;
}

void TrayUpdate::ShowUpdateIcon(mojom::UpdateSeverity severity,
                                bool factory_reset_required) {
  // Cache update info so we can create the default view when the menu opens.
  update_required_ = true;
  severity_ = severity;
  factory_reset_required_ = factory_reset_required;

  // Show the icon in the tray.
  SetIconColor(IconColorForUpdateSeverity(severity_, false));
  tray_view()->SetVisible(true);
}

}  // namespace ash
