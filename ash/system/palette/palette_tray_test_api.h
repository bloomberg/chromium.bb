// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PALETTE_PALETTE_TRAY_TEST_API_H_
#define ASH_SYSTEM_PALETTE_PALETTE_TRAY_TEST_API_H_

#include "ash/system/palette/palette_tray.h"
#include "base/macros.h"

namespace ash {

class PaletteToolManager;
class PaletteWelcomeBubble;
class TrayBubbleWrapper;

// Use the api in this class to test PaletteTray.
class PaletteTrayTestApi {
 public:
  explicit PaletteTrayTestApi(PaletteTray* palette_tray);
  ~PaletteTrayTestApi();

  PaletteToolManager* palette_tool_manager() {
    return palette_tray_->palette_tool_manager_.get();
  }

  PaletteWelcomeBubble* welcome_bubble() {
    return palette_tray_->welcome_bubble_.get();
  }

  TrayBubbleWrapper* tray_bubble_wrapper() {
    return palette_tray_->bubble_.get();
  }

  void OnStylusStateChanged(ui::StylusState state) {
    palette_tray_->OnStylusStateChanged(state);
  }

 private:
  PaletteTray* palette_tray_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PaletteTrayTestApi);
};

}  // namespace ash

#endif  // ASH_SYSTEM_PALETTE_PALETTE_TRAY_TEST_API_H_
