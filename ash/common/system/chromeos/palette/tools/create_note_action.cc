// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/palette/tools/create_note_action.h"

#include "ash/common/palette_delegate.h"
#include "ash/common/system/chromeos/palette/palette_ids.h"
#include "ash/common/wm_shell.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

CreateNoteAction::CreateNoteAction(Delegate* delegate)
    : CommonPaletteTool(delegate) {}

CreateNoteAction::~CreateNoteAction() {}

PaletteGroup CreateNoteAction::GetGroup() const {
  return PaletteGroup::ACTION;
}

PaletteToolId CreateNoteAction::GetToolId() const {
  return PaletteToolId::CREATE_NOTE;
}

void CreateNoteAction::OnEnable() {
  CommonPaletteTool::OnEnable();

  WmShell::Get()->palette_delegate()->CreateNote();

  delegate()->DisableTool(GetToolId());
  delegate()->HidePalette();
}

views::View* CreateNoteAction::CreateView() {
  if (!WmShell::Get()->palette_delegate()->HasNoteApp())
    return nullptr;

  return CreateDefaultView(
      l10n_util::GetStringUTF16(IDS_ASH_STYLUS_TOOLS_CREATE_NOTE_ACTION));
}

const gfx::VectorIcon& CreateNoteAction::GetPaletteIcon() const {
  return kPaletteActionCreateNoteIcon;
}

}  // namespace ash
