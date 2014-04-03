// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "ash/ash_switches.h"
#include "ash/display/display_layout_store.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "ui/gfx/display.h"

namespace ash {

DisplayLayoutStore::DisplayLayoutStore() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
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
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kAshSecondaryDisplayLayout))
    default_display_layout_ = layout;
}

void DisplayLayoutStore::RegisterLayoutForDisplayIdPair(
    int64 id1,
    int64 id2,
    const DisplayLayout& layout) {
  paired_layouts_[std::make_pair(id1, id2)] = layout;
}

DisplayLayout DisplayLayoutStore::GetRegisteredDisplayLayout(
    const DisplayIdPair& pair) {
  std::map<DisplayIdPair, DisplayLayout>::const_iterator iter =
      paired_layouts_.find(pair);
  return
      iter != paired_layouts_.end() ? iter->second : CreateDisplayLayout(pair);
}

DisplayLayout DisplayLayoutStore::ComputeDisplayLayoutForDisplayIdPair(
    const DisplayIdPair& pair) {
  DisplayLayout layout = GetRegisteredDisplayLayout(pair);
  DCHECK_NE(layout.primary_id, gfx::Display::kInvalidDisplayID);
  // Invert if the primary was swapped. If mirrored, first is always
  // primary.
  return (layout.primary_id == gfx::Display::kInvalidDisplayID ||
          pair.first == layout.primary_id) ? layout : layout.Invert();
}

void DisplayLayoutStore::UpdateMirrorStatus(const DisplayIdPair& pair,
                                            bool mirrored) {
  if (paired_layouts_.find(pair) == paired_layouts_.end())
    CreateDisplayLayout(pair);
  paired_layouts_[pair].mirrored = mirrored;
}

void DisplayLayoutStore::UpdatePrimaryDisplayId(const DisplayIdPair& pair,
                                                int64 display_id) {
  if (paired_layouts_.find(pair) == paired_layouts_.end())
    CreateDisplayLayout(pair);
  paired_layouts_[pair].primary_id = display_id;
}

DisplayLayout DisplayLayoutStore::CreateDisplayLayout(
    const DisplayIdPair& pair) {
  DisplayLayout layout = default_display_layout_;
  layout.primary_id = pair.first;
  paired_layouts_[pair] = layout;
  return layout;
}

}  // namespace ash
