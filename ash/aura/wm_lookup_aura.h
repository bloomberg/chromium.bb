// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AURA_WM_LOOKUP_AURA_H_
#define ASH_AURA_WM_LOOKUP_AURA_H_

#include "ash/common/wm_lookup.h"
#include "base/macros.h"

namespace ash {

// Aura implementation of WmLookup.
class WmLookupAura : public WmLookup {
 public:
  WmLookupAura();
  ~WmLookupAura() override;

  // WmLookup:
  RootWindowController* GetRootWindowControllerWithDisplayId(
      int64_t id) override;
  WmWindow* GetWindowForWidget(views::Widget* widget) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WmLookupAura);
};

}  // namespace ash

#endif  // ASH_AURA_WM_LOOKUP_AURA_H_
