// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/palette/common_palette_tool.h"

#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/system/chromeos/palette/palette_ids.h"
#include "ash/common/system/chromeos/palette/palette_tool_manager.h"
#include "ash/common/system/tray/hover_highlight_view.h"
#include "ash/common/system/tray/view_click_listener.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"

namespace ash {

CommonPaletteTool::CommonPaletteTool(Delegate* delegate)
    : PaletteTool(delegate) {}

CommonPaletteTool::~CommonPaletteTool() {}

views::View* CommonPaletteTool::CreateView() {
  // TODO(jdufault): Use real strings.
  return CreateDefaultView(
      base::ASCIIToUTF16("[TODO] " + PaletteToolIdToString(GetToolId())));
}

void CommonPaletteTool::OnViewDestroyed() {
  highlight_view_ = nullptr;
}

void CommonPaletteTool::OnEnable() {
  PaletteTool::OnEnable();

  if (highlight_view_) {
    highlight_view_->SetHighlight(true);
    highlight_view_->SetRightIconVisible(true);
  }
}

void CommonPaletteTool::OnDisable() {
  PaletteTool::OnDisable();

  if (highlight_view_) {
    highlight_view_->SetHighlight(false);
    highlight_view_->SetRightIconVisible(false);
  }
}

void CommonPaletteTool::OnViewClicked(views::View* sender) {
  if (enabled())
    delegate()->DisableTool(GetToolId());
  else
    delegate()->EnableTool(GetToolId());
}

views::View* CommonPaletteTool::CreateDefaultView(const base::string16& name) {
  highlight_view_ = new HoverHighlightView(this);

  // TODO(jdufault): Use real colors (SK_ColorBLACK?)
  gfx::ImageSkia image = CreateVectorIcon(GetPaletteIconId(), SK_ColorBLACK);
  gfx::ImageSkia checkbox =
      CreateVectorIcon(gfx::VectorIconId::CHECK_CIRCLE, gfx::kGoogleGreen700);

  highlight_view_->AddIndentedIconAndLabel(image, name, false);
  highlight_view_->AddRightIcon(checkbox);

  if (enabled())
    highlight_view_->SetHighlight(true);
  else
    highlight_view_->SetRightIconVisible(false);

  return highlight_view_;
}

}  // namespace ash
