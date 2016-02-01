// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "ash/ash_switches.h"
#include "ash/display/display_layout_store.h"
#include "ash/display/display_manager.h"
#include "ash/display/display_util.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "ui/gfx/display.h"

namespace ash {

DisplayLayoutStore::DisplayLayoutStore() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAshSecondaryDisplayLayout)) {
    std::string value = command_line->GetSwitchValueASCII(
        switches::kAshSecondaryDisplayLayout);
    char layout;
    int offset = 0;
    if (sscanf(value.c_str(), "%c,%d", &layout, &offset) == 2) {
      if (layout == 't')
        default_display_layout_.position = DisplayLayout::TOP;
      else if (layout == 'b')
        default_display_layout_.position = DisplayLayout::BOTTOM;
      else if (layout == 'r')
        default_display_layout_.position = DisplayLayout::RIGHT;
      else if (layout == 'l')
        default_display_layout_.position = DisplayLayout::LEFT;
      default_display_layout_.offset = offset;
    }
  }
}

DisplayLayoutStore::~DisplayLayoutStore() {
}

void DisplayLayoutStore::SetDefaultDisplayLayout(const DisplayLayout& layout) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kAshSecondaryDisplayLayout))
    default_display_layout_ = layout;
}

void DisplayLayoutStore::RegisterLayoutForDisplayIdList(
    int64_t id1,
    int64_t id2,
    const DisplayLayout& layout) {
  auto key = CreateDisplayIdList(id1, id2);

  // Do not overwrite the valid data with old invalid date.
  if (layouts_.count(key) && !CompareDisplayIds(id1, id2))
    return;

  layouts_[key] = layout;
}

DisplayLayout DisplayLayoutStore::GetRegisteredDisplayLayout(
    const DisplayIdList& list) {
  std::map<DisplayIdList, DisplayLayout>::const_iterator iter =
      layouts_.find(list);
  return iter != layouts_.end() ? iter->second : CreateDisplayLayout(list);
}

DisplayLayout DisplayLayoutStore::ComputeDisplayLayoutForDisplayIdList(
    const DisplayIdList& list) {
  DisplayLayout layout = GetRegisteredDisplayLayout(list);
  DCHECK_NE(layout.primary_id, gfx::Display::kInvalidDisplayID);
  // Invert if the primary was swapped. If mirrored, first is always
  // primary.
  return (layout.primary_id == gfx::Display::kInvalidDisplayID ||
          list[0] == layout.primary_id)
             ? layout
             : layout.Invert();
}

void DisplayLayoutStore::UpdateMultiDisplayState(const DisplayIdList& list,
                                                 bool mirrored,
                                                 bool default_unified) {
  if (layouts_.find(list) == layouts_.end())
    CreateDisplayLayout(list);
  layouts_[list].mirrored = mirrored;
  layouts_[list].default_unified = default_unified;
}

void DisplayLayoutStore::UpdatePrimaryDisplayId(const DisplayIdList& list,
                                                int64_t display_id) {
  if (layouts_.find(list) == layouts_.end())
    CreateDisplayLayout(list);
  layouts_[list].primary_id = display_id;
}

DisplayLayout DisplayLayoutStore::CreateDisplayLayout(
    const DisplayIdList& list) {
  DisplayLayout layout = default_display_layout_;
  layout.primary_id = list[0];
  layouts_[list] = layout;
  return layout;
}

}  // namespace ash
