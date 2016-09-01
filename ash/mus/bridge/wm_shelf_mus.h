// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_BRIDGE_WM_SHELF_MUS_H_
#define ASH_MUS_BRIDGE_WM_SHELF_MUS_H_

#include "ash/common/shelf/wm_shelf.h"
#include "base/macros.h"

namespace ash {

class WmWindow;

namespace mus {

// WmShelf implementation for mus.
class WmShelfMus : public WmShelf {
 public:
  explicit WmShelfMus(WmWindow* root_window);
  ~WmShelfMus() override;

  // WmShelf:
  void WillDeleteShelfLayoutManager() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WmShelfMus);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_BRIDGE_WM_SHELF_MUS_H_
