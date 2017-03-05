// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_TEST_TEST_PALETTE_DELEGATE_H_
#define ASH_COMMON_TEST_TEST_PALETTE_DELEGATE_H_

#include "ash/common/palette_delegate.h"
#include "base/macros.h"

namespace ash {

// A simple test double for a PaletteDelegate.
class TestPaletteDelegate : public PaletteDelegate {
 public:
  TestPaletteDelegate();
  ~TestPaletteDelegate() override;

  int create_note_count() const { return create_note_count_; }

  int has_note_app_count() const { return has_note_app_count_; }

  int take_screenshot_count() const { return take_screenshot_count_; }

  int take_partial_screenshot_count() const {
    return take_partial_screenshot_count_;
  }

  base::Closure partial_screenshot_done() const {
    return partial_screenshot_done_;
  }

  void set_has_note_app(bool has_note_app) { has_note_app_ = has_note_app; }

  void set_should_auto_open_palette(bool should_auto_open_palette) {
    should_auto_open_palette_ = should_auto_open_palette;
  }

  void set_should_show_palette(bool should_show_palette) {
    should_show_palette_ = should_show_palette;
  }

 private:
  // PaletteDelegate:
  std::unique_ptr<EnableListenerSubscription> AddPaletteEnableListener(
      const EnableListener& on_state_changed) override;
  void CreateNote() override;
  bool HasNoteApp() override;
  bool ShouldAutoOpenPalette() override;
  bool ShouldShowPalette() override;
  void TakeScreenshot() override;
  void TakePartialScreenshot(const base::Closure& done) override;
  void CancelPartialScreenshot() override;

  int create_note_count_ = 0;
  int has_note_app_count_ = 0;
  int take_screenshot_count_ = 0;
  int take_partial_screenshot_count_ = 0;
  base::Closure partial_screenshot_done_;
  bool has_note_app_ = false;
  bool should_auto_open_palette_ = false;
  bool should_show_palette_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestPaletteDelegate);
};

}  // namespace ash

#endif  // ASH_COMMON_TEST_TEST_PALETTE_DELEGATE_H_
