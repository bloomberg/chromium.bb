// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_LAYOUT_STORE_H_
#define ASH_DISPLAY_DISPLAY_LAYOUT_STORE_H_

#include <stdint.h>

#include <map>

#include "ash/ash_export.h"
#include "ash/display/display_layout.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace ash {

class ASH_EXPORT DisplayLayoutStore {
 public:
  DisplayLayoutStore();
  ~DisplayLayoutStore();

  void SetDefaultDisplayPlacement(const DisplayPlacement& placement);

  // Registeres the display layout info for the specified display(s).
  void RegisterLayoutForDisplayIdList(const DisplayIdList& list,
                                      scoped_ptr<DisplayLayout> layout);

  // If no layout is registered, it creatas new layout using
  // |default_display_layout_|.
  const DisplayLayout& GetRegisteredDisplayLayout(const DisplayIdList& list);

  // Update the multi display state in the display layout for
  // |display_list|.  This creates new display layout if no layout is
  // registered for |display_list|.
  void UpdateMultiDisplayState(const DisplayIdList& display_list,
                               bool mirrored,
                               bool default_unified);

 private:
  // Creates new layout for display list from |default_display_layout_|.
  DisplayLayout* CreateDefaultDisplayLayout(const DisplayIdList& display_list);

  // The default display placement.
  DisplayPlacement default_display_placement_;

  // Display layout per list of devices.
  std::map<DisplayIdList, scoped_ptr<DisplayLayout>> layouts_;

  DISALLOW_COPY_AND_ASSIGN(DisplayLayoutStore);
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_LAYOUT_STORE_H_
