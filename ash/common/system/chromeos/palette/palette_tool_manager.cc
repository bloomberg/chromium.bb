// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/palette/palette_tool_manager.h"

#include <algorithm>

#include "ash/common/system/chromeos/palette/palette_tool.h"
#include "base/bind.h"
#include "ui/gfx/vector_icons_public.h"

namespace ash {

PaletteToolManager::PaletteToolManager(Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

PaletteToolManager::~PaletteToolManager() {}

void PaletteToolManager::AddTool(std::unique_ptr<PaletteTool> tool) {
  // The same PaletteToolId cannot be registered twice.
  DCHECK_EQ(0, std::count_if(tools_.begin(), tools_.end(),
                             [&tool](const std::unique_ptr<PaletteTool>& t) {
                               return t->GetToolId() == tool->GetToolId();
                             }));

  tools_.emplace_back(std::move(tool));
}

void PaletteToolManager::ActivateTool(PaletteToolId tool_id) {
  PaletteTool* new_tool = FindToolById(tool_id);
  DCHECK(new_tool);

  PaletteTool* previous_tool = active_tools_[new_tool->GetGroup()];

  if (new_tool == previous_tool)
    return;

  if (previous_tool)
    previous_tool->OnDisable();

  active_tools_[new_tool->GetGroup()] = new_tool;
  new_tool->OnEnable();

  delegate_->OnActiveToolChanged();
}

void PaletteToolManager::DeactivateTool(PaletteToolId tool_id) {
  PaletteTool* tool = FindToolById(tool_id);
  DCHECK(tool);

  active_tools_[tool->GetGroup()] = nullptr;
  tool->OnDisable();

  delegate_->OnActiveToolChanged();
}

bool PaletteToolManager::IsToolActive(PaletteToolId tool_id) {
  PaletteTool* tool = FindToolById(tool_id);
  DCHECK(tool);

  return active_tools_[tool->GetGroup()] == tool;
}

PaletteToolId PaletteToolManager::GetActiveTool(PaletteGroup group) {
  PaletteTool* active_tool = active_tools_[group];
  return active_tool ? active_tool->GetToolId() : PaletteToolId::NONE;
}

gfx::VectorIconId PaletteToolManager::GetActiveTrayIcon(PaletteToolId tool_id) {
  PaletteTool* tool = FindToolById(tool_id);
  if (!tool)
    return gfx::VectorIconId::PALETTE_TRAY_ICON_DEFAULT;

  return tool->GetActiveTrayIcon();
}

std::vector<PaletteToolView> PaletteToolManager::CreateViews() {
  std::vector<PaletteToolView> views;
  views.reserve(tools_.size());

  for (size_t i = 0; i < tools_.size(); ++i) {
    views::View* tool_view = tools_[i]->CreateView();
    if (!tool_view)
      continue;

    PaletteToolView view;
    view.group = tools_[i]->GetGroup();
    view.tool_id = tools_[i]->GetToolId();
    view.view = tool_view;
    views.push_back(view);
  }

  return views;
}

void PaletteToolManager::NotifyViewsDestroyed() {
  for (std::unique_ptr<PaletteTool>& tool : tools_)
    tool->OnViewDestroyed();
}

void PaletteToolManager::EnableTool(PaletteToolId tool_id) {
  ActivateTool(tool_id);
}

void PaletteToolManager::DisableTool(PaletteToolId tool_id) {
  DeactivateTool(tool_id);
}

void PaletteToolManager::HidePalette() {
  delegate_->HidePalette();
}

WmWindow* PaletteToolManager::GetWindow() {
  return delegate_->GetWindow();
}

PaletteTool* PaletteToolManager::FindToolById(PaletteToolId tool_id) {
  for (std::unique_ptr<PaletteTool>& tool : tools_) {
    if (tool->GetToolId() == tool_id)
      return tool.get();
  }

  return nullptr;
}

}  // namespace ash
