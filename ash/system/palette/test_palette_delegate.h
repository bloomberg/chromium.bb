// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PALETTE_TEST_PALETTE_DELEGATE_H_
#define ASH_SYSTEM_PALETTE_TEST_PALETTE_DELEGATE_H_

#include "ash/palette_delegate.h"
#include "base/macros.h"

namespace ash {

// A simple test double for a PaletteDelegate.
class TestPaletteDelegate : public PaletteDelegate {
 public:
  TestPaletteDelegate();
  ~TestPaletteDelegate() override;

  void set_should_auto_open_palette(bool should_auto_open_palette) {
    should_auto_open_palette_ = should_auto_open_palette;
  }

  void set_should_show_palette(bool should_show_palette) {
    should_show_palette_ = should_show_palette;
  }

 protected:
  // PaletteDelegate:
  std::unique_ptr<EnableListenerSubscription> AddPaletteEnableListener(
      const EnableListener& on_state_changed) override;
  bool ShouldAutoOpenPalette() override;
  bool ShouldShowPalette() override;

  bool should_auto_open_palette_ = false;
  bool should_show_palette_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestPaletteDelegate);
};

}  // namespace ash

#endif  // ASH_SYSTEM_PALETTE_TEST_PALETTE_DELEGATE_H_
