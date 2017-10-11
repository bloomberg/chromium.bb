// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_tray_test_api.h"

#include "ash/shell.h"
#include "ash/system/enterprise/tray_enterprise.h"
#include "ash/system/network/tray_network.h"
#include "ash/system/tray/system_tray.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/view.h"

namespace ash {
namespace {

// Returns true if this view or any child view has the given tooltip.
bool HasChildWithTooltip(views::View* view,
                         const base::string16& given_tooltip) {
  base::string16 tooltip;
  view->GetTooltipText(gfx::Point(), &tooltip);
  if (tooltip == given_tooltip)
    return true;

  for (int i = 0; i < view->child_count(); ++i) {
    if (HasChildWithTooltip(view->child_at(i), given_tooltip))
      return true;
  }

  return false;
}

}  // namespace

SystemTrayTestApi::SystemTrayTestApi(SystemTray* tray) : tray_(tray) {}

SystemTrayTestApi::~SystemTrayTestApi() {}

// static
void SystemTrayTestApi::BindRequest(mojom::SystemTrayTestApiRequest request) {
  SystemTray* tray = Shell::Get()->GetPrimarySystemTray();
  mojo::MakeStrongBinding(std::make_unique<SystemTrayTestApi>(tray),
                          std::move(request));
}

void SystemTrayTestApi::ShowBubble(ShowBubbleCallback cb) {
  SystemTray* tray = Shell::Get()->GetPrimarySystemTray();
  tray->ShowDefaultView(ash::BUBBLE_CREATE_NEW, false /* show_by_click */);
  base::RunLoop().RunUntilIdle();  // May animate.
  std::move(cb).Run();
}

void SystemTrayTestApi::ShowDetailedView(mojom::TrayItem item,
                                         ShowDetailedViewCallback cb) {
  SystemTray* tray = Shell::Get()->GetPrimarySystemTray();
  SystemTrayItem* tray_item;
  switch (item) {
    case mojom::TrayItem::kEnterprise:
      tray_item = tray_->tray_enterprise_;
      break;
    case mojom::TrayItem::kNetwork:
      tray_item = tray_->tray_network_;
      break;
  }
  tray->ShowDetailedView(tray_item, 0 /* delay */, BUBBLE_CREATE_NEW);
  base::RunLoop().RunUntilIdle();  // May animate.
  std::move(cb).Run();
}

void SystemTrayTestApi::IsBubbleViewVisible(int view_id,
                                            IsBubbleViewVisibleCallback cb) {
  SystemTray* tray = Shell::Get()->GetPrimarySystemTray();

  // Find the view in the bubble (not the tray itself).
  views::View* view =
      tray->GetSystemBubble()->bubble_view()->GetViewByID(view_id);
  std::move(cb).Run(view && view->visible());
}

void SystemTrayTestApi::GetBubbleViewTooltip(int view_id,
                                             GetBubbleViewTooltipCallback cb) {
  SystemTray* tray = Shell::Get()->GetPrimarySystemTray();

  // Find the view in the bubble (not the tray itself).
  views::View* view =
      tray->GetSystemBubble()->bubble_view()->GetViewByID(view_id);
  if (!view) {
    std::move(cb).Run(base::string16());
    return;
  }
  base::string16 tooltip;
  view->GetTooltipText(gfx::Point(), &tooltip);
  std::move(cb).Run(tooltip);
}

}  // namespace ash
