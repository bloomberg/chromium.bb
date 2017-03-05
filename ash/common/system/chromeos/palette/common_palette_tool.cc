// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/palette/common_palette_tool.h"

#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/system/chromeos/palette/palette_ids.h"
#include "ash/common/system/chromeos/palette/palette_tool_manager.h"
#include "ash/common/system/tray/hover_highlight_view.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/view_click_listener.h"
#include "ash/resources/grit/ash_resources.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"

namespace ash {
namespace {

void AddHistogramTimes(PaletteToolId id, base::TimeDelta duration) {
  if (id == PaletteToolId::LASER_POINTER) {
    UMA_HISTOGRAM_CUSTOM_TIMES("Ash.Shelf.Palette.InLaserPointerMode", duration,
                               base::TimeDelta::FromMilliseconds(100),
                               base::TimeDelta::FromHours(1), 50);
  } else if (id == PaletteToolId::MAGNIFY) {
    UMA_HISTOGRAM_CUSTOM_TIMES("Ash.Shelf.Palette.InMagnifyMode", duration,
                               base::TimeDelta::FromMilliseconds(100),
                               base::TimeDelta::FromHours(1), 50);
  }
}

}  // namespace

CommonPaletteTool::CommonPaletteTool(Delegate* delegate)
    : PaletteTool(delegate) {}

CommonPaletteTool::~CommonPaletteTool() {}

void CommonPaletteTool::OnViewDestroyed() {
  highlight_view_ = nullptr;
}

void CommonPaletteTool::OnEnable() {
  PaletteTool::OnEnable();
  start_time_ = base::TimeTicks::Now();

  if (highlight_view_) {
    highlight_view_->SetRightViewVisible(true);
    highlight_view_->SetAccessiblityState(
        HoverHighlightView::AccessibilityState::CHECKED_CHECKBOX);
  }
}

void CommonPaletteTool::OnDisable() {
  PaletteTool::OnDisable();
  AddHistogramTimes(GetToolId(), base::TimeTicks::Now() - start_time_);

  if (highlight_view_) {
    highlight_view_->SetRightViewVisible(false);
    highlight_view_->SetAccessiblityState(
        HoverHighlightView::AccessibilityState::UNCHECKED_CHECKBOX);
  }
}

void CommonPaletteTool::OnViewClicked(views::View* sender) {
  delegate()->RecordPaletteOptionsUsage(
      PaletteToolIdToPaletteTrayOptions(GetToolId()));
  if (enabled()) {
    delegate()->DisableTool(GetToolId());
    delegate()->RecordPaletteModeCancellation(
        PaletteToolIdToPaletteModeCancelType(GetToolId(),
                                             false /*is_switched*/));
  } else {
    delegate()->EnableTool(GetToolId());
  }
}

views::View* CommonPaletteTool::CreateDefaultView(const base::string16& name) {
  gfx::ImageSkia icon =
      CreateVectorIcon(GetPaletteIcon(), kMenuIconSize, gfx::kChromeIconGrey);
  gfx::ImageSkia check = CreateVectorIcon(gfx::VectorIconId::CHECK_CIRCLE,
                                          kMenuIconSize, gfx::kGoogleGreen700);

  highlight_view_ = new HoverHighlightView(this);
  highlight_view_->SetBorder(views::CreateEmptyBorder(0, 0, 0, 0));
  const int interior_button_padding = (kMenuButtonSize - kMenuIconSize) / 2;
  highlight_view_->AddIconAndLabelCustomSize(icon, name, false, kMenuIconSize,
                                             interior_button_padding,
                                             kTrayPopupPaddingHorizontal);
  highlight_view_->AddRightIcon(check, kMenuIconSize);
  highlight_view_->set_custom_height(kMenuButtonSize);

  if (enabled()) {
    highlight_view_->SetAccessiblityState(
        HoverHighlightView::AccessibilityState::CHECKED_CHECKBOX);
  } else {
    highlight_view_->SetRightViewVisible(false);
    highlight_view_->SetAccessiblityState(
        HoverHighlightView::AccessibilityState::UNCHECKED_CHECKBOX);
  }

  return highlight_view_;
}

}  // namespace ash
