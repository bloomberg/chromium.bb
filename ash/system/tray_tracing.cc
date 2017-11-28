// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray_tracing.h"

#include "ash/metrics/user_metrics_action.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/actionable_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_controller.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace tray {

class DefaultTracingView : public ActionableView {
 public:
  explicit DefaultTracingView(SystemTrayItem* owner)
      : ActionableView(owner, TrayPopupInkDropStyle::FILL_BOUNDS) {
    SetLayoutManager(new views::FillLayout);
    TriView* tri_view = TrayPopupUtils::CreateDefaultRowView();
    AddChildView(tri_view);

    auto* image = TrayPopupUtils::CreateMainImageView();
    tri_view->AddView(TriView::Container::START, image);

    auto* label = TrayPopupUtils::CreateDefaultLabel();
    label->SetMultiLine(true);
    label->SetText(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_TRACING));
    tri_view->AddView(TriView::Container::CENTER, label);

    TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::DEFAULT_VIEW_LABEL);
    style.SetupLabel(label);
    image->SetImage(
        gfx::CreateVectorIcon(kSystemMenuTracingIcon, style.GetIconColor()));

    SetInkDropMode(InkDropHostView::InkDropMode::ON);
  }

  ~DefaultTracingView() override = default;

 private:
  bool PerformAction(const ui::Event& event) override {
    Shell::Get()->metrics()->RecordUserMetricsAction(
        UMA_STATUS_AREA_TRACING_DEFAULT_SELECTED);
    Shell::Get()->system_tray_controller()->ShowChromeSlow();
    CloseSystemBubble();
    return true;
  }

  DISALLOW_COPY_AND_ASSIGN(DefaultTracingView);
};

}  // namespace tray

////////////////////////////////////////////////////////////////////////////////
// ash::TrayTracing

TrayTracing::TrayTracing(SystemTray* system_tray)
    : TrayImageItem(system_tray, kSystemTrayTracingIcon, UMA_TRACING),
      default_(nullptr) {
  DCHECK(system_tray);
  Shell::Get()->system_tray_notifier()->AddTracingObserver(this);
}

TrayTracing::~TrayTracing() {
  Shell::Get()->system_tray_notifier()->RemoveTracingObserver(this);
}

void TrayTracing::SetTrayIconVisible(bool visible) {
  if (tray_view())
    tray_view()->SetVisible(visible);
}

bool TrayTracing::GetInitialVisibility() {
  return false;
}

views::View* TrayTracing::CreateDefaultView(LoginStatus status) {
  CHECK(default_ == nullptr);
  if (tray_view() && tray_view()->visible())
    default_ = new tray::DefaultTracingView(this);
  return default_;
}

views::View* TrayTracing::CreateDetailedView(LoginStatus status) {
  return nullptr;
}

void TrayTracing::OnDefaultViewDestroyed() {
  default_ = nullptr;
}

void TrayTracing::OnDetailedViewDestroyed() {}

void TrayTracing::OnTracingModeChanged(bool value) {
  SetTrayIconVisible(value);
}

}  // namespace ash
