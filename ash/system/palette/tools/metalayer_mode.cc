// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/tools/metalayer_mode.h"

#include "ash/palette_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/palette/palette_ids.h"
#include "ash/system/palette/palette_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"

namespace ash {

MetalayerMode::MetalayerMode(Delegate* delegate)
    : CommonPaletteTool(delegate), weak_factory_(this) {
  Shell::Get()->AddPreTargetHandler(this);
}

MetalayerMode::~MetalayerMode() {
  Shell::Get()->RemovePreTargetHandler(this);
}

PaletteGroup MetalayerMode::GetGroup() const {
  return PaletteGroup::MODE;
}

PaletteToolId MetalayerMode::GetToolId() const {
  return PaletteToolId::METALAYER;
}

void MetalayerMode::OnEnable() {
  CommonPaletteTool::OnEnable();

  Shell::Get()->palette_delegate()->ShowMetalayer(
      base::Bind(&MetalayerMode::OnMetalayerDone, weak_factory_.GetWeakPtr()));
  delegate()->HidePalette();
}

void MetalayerMode::OnDisable() {
  CommonPaletteTool::OnDisable();

  Shell::Get()->palette_delegate()->HideMetalayer();
}

const gfx::VectorIcon& MetalayerMode::GetActiveTrayIcon() const {
  return kPaletteTrayIconMetalayerIcon;
}

const gfx::VectorIcon& MetalayerMode::GetPaletteIcon() const {
  return kPaletteModeMetalayerIcon;
}

views::View* MetalayerMode::CreateView() {
  if (!Shell::Get()->palette_delegate()->IsMetalayerSupported())
    return nullptr;

  return CreateDefaultView(
      l10n_util::GetStringUTF16(IDS_ASH_STYLUS_TOOLS_METALAYER_MODE));
}

void MetalayerMode::OnMetalayerDone() {
  delegate()->DisableTool(GetToolId());
}

void MetalayerMode::OnTouchEvent(ui::TouchEvent* event) {
  if (event->pointer_details().pointer_type !=
      ui::EventPointerType::POINTER_TYPE_PEN)
    return;

  if (event->type() != ui::ET_TOUCH_PRESSED)
    return;

  // The stylus "barrel" button press is encoded as ui::EF_LEFT_MOUSE_BUTTON
  if (!(event->flags() & ui::EF_LEFT_MOUSE_BUTTON))
    return;

  if (palette_utils::PaletteContainsPointInScreen(event->root_location()))
    return;

  if (enabled())
    return;

  // Shell::palette_delegate() might return null in some tests that are not
  // concerned with palette but generate touch events.
  if (!Shell::Get()->palette_delegate() ||
      !Shell::Get()->palette_delegate()->IsMetalayerSupported())
    return;

  delegate()->RecordPaletteOptionsUsage(
      PaletteToolIdToPaletteTrayOptions(GetToolId()),
      PaletteInvocationMethod::SHORTCUT);
  delegate()->EnableTool(GetToolId());
  event->StopPropagation();
}

}  // namespace ash
