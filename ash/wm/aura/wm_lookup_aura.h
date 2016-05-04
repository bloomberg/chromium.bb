// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_AURA_WM_LOOKUP_AURA_H_
#define ASH_WM_AURA_WM_LOOKUP_AURA_H_

#include "ash/wm/common/wm_lookup.h"
#include "base/macros.h"

namespace ash {
namespace wm {

// Aura implementation of WmLookup.
class WmLookupAura : public WmLookup {
 public:
  WmLookupAura();
  ~WmLookupAura() override;

  // WmLookup:
  WmRootWindowController* GetRootWindowControllerWithDisplayId(
      int64_t id) override;
  WmWindow* GetWindowForWidget(views::Widget* widget) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WmLookupAura);
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_AURA_WM_LOOKUP_AURA_H_
