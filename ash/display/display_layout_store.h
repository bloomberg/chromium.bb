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

namespace ash {

class ASH_EXPORT DisplayLayoutStore {
 public:
  DisplayLayoutStore();
  ~DisplayLayoutStore();

  const DisplayLayout& default_display_layout() const {
    return default_display_layout_;
  }
  void SetDefaultDisplayLayout(const DisplayLayout& layout);

  // Registeres the display layout info for the specified display(s).
  void RegisterLayoutForDisplayIdList(const DisplayIdList& list,
                                      const DisplayLayout& layout);

  // If no layout is registered, it creatas new layout using
  // |default_display_layout_|.
  DisplayLayout GetRegisteredDisplayLayout(const DisplayIdList& list);

  // Update the multi display state in the display layout for
  // |display_list|.  This creates new display layout if no layout is
  // registered for |display_list|.
  void UpdateMultiDisplayState(const DisplayIdList& display_list,
                               bool mirrored,
                               bool default_unified);

  // Update the |primary_id| in the display layout for
  // |display_list|.  This creates new display layout if no layout is
  // registered for |display_list|.
  void UpdatePrimaryDisplayId(const DisplayIdList& display_list,
                              int64_t display_id);

 private:
  // Creates new layout for display list from |default_display_layout_|.
  DisplayLayout CreateDefaultDisplayLayout(const DisplayIdList& display_list);

  // The default display layout.
  DisplayLayout default_display_layout_;

  // Display layout per list of devices.
  std::map<DisplayIdList, DisplayLayout> layouts_;

  DISALLOW_COPY_AND_ASSIGN(DisplayLayoutStore);
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_LAYOUT_STORE_H_
