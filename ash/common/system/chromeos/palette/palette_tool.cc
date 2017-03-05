// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/palette/palette_tool.h"

#include "ash/common/system/chromeos/palette/palette_tool_manager.h"
#include "ash/common/system/chromeos/palette/palette_utils.h"
#include "ash/common/system/chromeos/palette/tools/capture_region_mode.h"
#include "ash/common/system/chromeos/palette/tools/capture_screen_action.h"
#include "ash/common/system/chromeos/palette/tools/create_note_action.h"
#include "ash/common/system/chromeos/palette/tools/laser_pointer_mode.h"
#include "ash/common/system/chromeos/palette/tools/magnifier_mode.h"
#include "base/memory/ptr_util.h"
#include "ui/gfx/paint_vector_icon.h"

namespace ash {

// static
void PaletteTool::RegisterToolInstances(PaletteToolManager* tool_manager) {
  tool_manager->AddTool(base::MakeUnique<CaptureRegionMode>(tool_manager));
  tool_manager->AddTool(base::MakeUnique<CaptureScreenAction>(tool_manager));
  tool_manager->AddTool(base::MakeUnique<CreateNoteAction>(tool_manager));
  tool_manager->AddTool(base::MakeUnique<LaserPointerMode>(tool_manager));
  tool_manager->AddTool(base::MakeUnique<MagnifierMode>(tool_manager));
}

PaletteTool::PaletteTool(Delegate* delegate) : delegate_(delegate) {}

PaletteTool::~PaletteTool() {}

void PaletteTool::OnEnable() {
  enabled_ = true;
}

void PaletteTool::OnDisable() {
  enabled_ = false;
}

const gfx::VectorIcon& PaletteTool::GetActiveTrayIcon() const {
  return gfx::kNoneIcon;
}

}  // namespace ash
