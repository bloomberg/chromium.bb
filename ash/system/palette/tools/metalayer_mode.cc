// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/tools/metalayer_mode.h"

#include "ash/palette_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/palette/palette_ids.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

MetalayerMode::MetalayerMode(Delegate* delegate)
    : CommonPaletteTool(delegate), weak_factory_(this) {}

MetalayerMode::~MetalayerMode() {}

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
  // TODO(kaznacheev) replace with the correct icon when it is available
  return kPaletteTrayIconCaptureRegionIcon;
}

const gfx::VectorIcon& MetalayerMode::GetPaletteIcon() const {
  // TODO(kaznacheev) replace with the correct icon when it is available
  return kPaletteActionCaptureRegionIcon;
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

}  // namespace ash
