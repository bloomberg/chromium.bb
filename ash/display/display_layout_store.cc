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

DisplayLayoutStore::DisplayLayoutStore()
    : default_display_layout_(new DisplayLayout) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAshSecondaryDisplayLayout)) {
    std::string value = command_line->GetSwitchValueASCII(
        switches::kAshSecondaryDisplayLayout);
    char layout;
    int offset = 0;
    if (sscanf(value.c_str(), "%c,%d", &layout, &offset) == 2) {
      if (layout == 't')
        default_display_layout_->placement.position = DisplayPlacement::TOP;
      else if (layout == 'b')
        default_display_layout_->placement.position = DisplayPlacement::BOTTOM;
      else if (layout == 'r')
        default_display_layout_->placement.position = DisplayPlacement::RIGHT;
      else if (layout == 'l')
        default_display_layout_->placement.position = DisplayPlacement::LEFT;
      default_display_layout_->placement.offset = offset;
    }
  }
}

DisplayLayoutStore::~DisplayLayoutStore() {
}

void DisplayLayoutStore::SetDefaultDisplayLayout(
    scoped_ptr<DisplayLayout> layout) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kAshSecondaryDisplayLayout))
    default_display_layout_ = std::move(layout);
}

void DisplayLayoutStore::RegisterLayoutForDisplayIdList(
    const DisplayIdList& list,
    scoped_ptr<DisplayLayout> layout) {
  DCHECK_EQ(2u, list.size());

  // Do not overwrite the valid data with old invalid date.
  if (layouts_.count(list) && !CompareDisplayIds(list[0], list[1]))
    return;

  // Old data may not have the display_id/parent_display_id.
  // Guess these values based on the saved primary_id.
  if (layout->placement.display_id == gfx::Display::kInvalidDisplayID) {
    if (layout->primary_id == list[1]) {
      layout->placement.display_id = list[0];
      layout->placement.parent_display_id = list[1];
    } else {
      layout->placement.display_id = list[1];
      layout->placement.parent_display_id = list[0];
    }
  }
  layouts_[list] = std::move(layout);
}

const DisplayLayout& DisplayLayoutStore::GetRegisteredDisplayLayout(
    const DisplayIdList& list) {
  DCHECK_NE(1u, list.size());
  const auto iter = layouts_.find(list);
  const DisplayLayout* layout = iter != layouts_.end()
                                    ? iter->second.get()
                                    : CreateDefaultDisplayLayout(list);
  DCHECK_EQ(layout->primary_id, layout->placement.parent_display_id);
  DCHECK((layout->placement.display_id == list[0] &&
          layout->placement.parent_display_id == list[1]) ||
         (layout->placement.display_id == list[1] &&
          layout->placement.parent_display_id == list[0]));
  DCHECK_NE(layout->primary_id, gfx::Display::kInvalidDisplayID);
  return *layout;
}

void DisplayLayoutStore::UpdateMultiDisplayState(const DisplayIdList& list,
                                                 bool mirrored,
                                                 bool default_unified) {
  DCHECK(layouts_.find(list) != layouts_.end());
  if (layouts_.find(list) == layouts_.end())
    CreateDefaultDisplayLayout(list);

  layouts_[list]->mirrored = mirrored;
  layouts_[list]->default_unified = default_unified;
}

DisplayLayout* DisplayLayoutStore::CreateDefaultDisplayLayout(
    const DisplayIdList& list) {
  scoped_ptr<DisplayLayout> layout(default_display_layout_->Copy());
  layout->primary_id = list[0];
  layout->placement.display_id = list[1];
  layout->placement.parent_display_id = list[0];
  layouts_[list] = std::move(layout);
  auto iter = layouts_.find(list);
  return iter->second.get();
}

}  // namespace ash
