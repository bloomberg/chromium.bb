// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/tray_utils.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_item_view.h"
#include "ash/common/wm_shell.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"

namespace ash {

void SetupLabelForTray(views::Label* label) {
  if (MaterialDesignController::IsShelfMaterial()) {
    // The text is drawn on an transparent bg, so we must disable subpixel
    // rendering.
    label->SetSubpixelRenderingEnabled(false);
    label->SetFontList(gfx::FontList().Derive(2, gfx::Font::NORMAL,
                                              gfx::Font::Weight::MEDIUM));
  } else {
    label->SetFontList(
        gfx::FontList().Derive(1, gfx::Font::NORMAL, gfx::Font::Weight::BOLD));
    label->SetShadows(gfx::ShadowValues(
        1,
        gfx::ShadowValue(gfx::Vector2d(0, 1), 0, SkColorSetARGB(64, 0, 0, 0))));
    label->SetAutoColorReadabilityEnabled(false);
    label->SetEnabledColor(SK_ColorWHITE);
    label->SetBackgroundColor(SkColorSetARGB(0, 255, 255, 255));
  }
}

void SetTrayImageItemBorder(views::View* tray_view, ShelfAlignment alignment) {
  if (MaterialDesignController::IsShelfMaterial())
    return;

  const int tray_image_item_padding = GetTrayConstant(TRAY_IMAGE_ITEM_PADDING);
  if (IsHorizontalAlignment(alignment)) {
    tray_view->SetBorder(views::Border::CreateEmptyBorder(
        0, tray_image_item_padding, 0, tray_image_item_padding));
  } else {
    tray_view->SetBorder(views::Border::CreateEmptyBorder(
        tray_image_item_padding,
        kTrayImageItemHorizontalPaddingVerticalAlignment,
        tray_image_item_padding,
        kTrayImageItemHorizontalPaddingVerticalAlignment));
  }
}

void SetTrayLabelItemBorder(TrayItemView* tray_view, ShelfAlignment alignment) {
  if (MaterialDesignController::IsShelfMaterial())
    return;

  if (IsHorizontalAlignment(alignment)) {
    tray_view->SetBorder(views::Border::CreateEmptyBorder(
        0, kTrayLabelItemHorizontalPaddingBottomAlignment, 0,
        kTrayLabelItemHorizontalPaddingBottomAlignment));
  } else {
    // Center the label for vertical launcher alignment.
    int horizontal_padding =
        std::max(0, (tray_view->GetPreferredSize().width() -
                     tray_view->label()->GetPreferredSize().width()) /
                        2);
    tray_view->SetBorder(views::Border::CreateEmptyBorder(
        kTrayLabelItemVerticalPaddingVerticalAlignment, horizontal_padding,
        kTrayLabelItemVerticalPaddingVerticalAlignment, horizontal_padding));
  }
}

void GetAccessibleLabelFromDescendantViews(
    views::View* view,
    std::vector<base::string16>& out_labels) {
  ui::AXViewState temp_state;
  view->GetAccessibleState(&temp_state);
  if (!temp_state.name.empty())
    out_labels.push_back(temp_state.name);

  // Do not descend into static text labels which may compute their own labels
  // recursively.
  if (temp_state.role == ui::AX_ROLE_STATIC_TEXT)
    return;

  for (int i = 0; i < view->child_count(); ++i)
    GetAccessibleLabelFromDescendantViews(view->child_at(i), out_labels);
}

bool CanOpenWebUISettings(LoginStatus status) {
  // TODO(tdanderson): Consider moving this into WmShell, or introduce a
  // CanShowSettings() method in each delegate type that has a
  // ShowSettings() method.
  return status != LoginStatus::NOT_LOGGED_IN &&
         status != LoginStatus::LOCKED &&
         !WmShell::Get()->GetSessionStateDelegate()->IsInSecondaryLoginScreen();
}

}  // namespace ash
