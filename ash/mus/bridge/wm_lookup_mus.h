// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_BRIDGE_WM_LOOKUP_MUS_H_
#define ASH_MUS_BRIDGE_WM_LOOKUP_MUS_H_

#include <stdint.h>

#include "ash/common/wm_lookup.h"
#include "base/macros.h"

namespace ash {
namespace mus {

// WmLookup implementation for mus.
class WmLookupMus : public WmLookup {
 public:
  WmLookupMus();
  ~WmLookupMus() override;

  // WmLookup:
  RootWindowController* GetRootWindowControllerWithDisplayId(
      int64_t id) override;
  WmWindow* GetWindowForWidget(views::Widget* widget) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WmLookupMus);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_BRIDGE_WM_LOOKUP_MUS_H_
