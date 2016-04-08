// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_LAYOUT_STORE_H_
#define ASH_DISPLAY_DISPLAY_LAYOUT_STORE_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/display/manager/display_layout.h"

namespace ash {

class ASH_EXPORT DisplayLayoutStore {
 public:
  DisplayLayoutStore();
  ~DisplayLayoutStore();

  void SetDefaultDisplayPlacement(const display::DisplayPlacement& placement);

  // Registeres the display layout info for the specified display(s).
  void RegisterLayoutForDisplayIdList(
      const display::DisplayIdList& list,
      std::unique_ptr<display::DisplayLayout> layout);

  // If no layout is registered, it creatas new layout using
  // |default_display_layout_|.
  const display::DisplayLayout& GetRegisteredDisplayLayout(
      const display::DisplayIdList& list);

  // Update the multi display state in the display layout for
  // |display_list|.  This creates new display layout if no layout is
  // registered for |display_list|.
  void UpdateMultiDisplayState(const display::DisplayIdList& display_list,
                               bool mirrored,
                               bool default_unified);

 private:
  // Creates new layout for display list from |default_display_layout_|.
  display::DisplayLayout* CreateDefaultDisplayLayout(
      const display::DisplayIdList& display_list);

  // The default display placement.
  display::DisplayPlacement default_display_placement_;

  // Display layout per list of devices.
  std::map<display::DisplayIdList, std::unique_ptr<display::DisplayLayout>>
      layouts_;

  DISALLOW_COPY_AND_ASSIGN(DisplayLayoutStore);
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_LAYOUT_STORE_H_
