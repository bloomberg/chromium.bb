// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_TEST_ASH_TEST_IMPL_AURA_H_
#define ASH_MUS_TEST_ASH_TEST_IMPL_AURA_H_

#include "ash/common/test/ash_test_impl.h"

namespace ash {
namespace test {
class AshTestBase;
}

// Implementation of AshTestImpl on top of aura. Implementation of AshTestImpl
// calls through to AshTestBase.
class AshTestImplAura : public AshTestImpl {
 public:
  AshTestImplAura();
  ~AshTestImplAura() override;

  // AshTestImpl:
  void SetUp() override;
  void TearDown() override;
  void UpdateDisplay(const std::string& display_spec) override;
  std::unique_ptr<WindowOwner> CreateTestWindow(
      const gfx::Rect& bounds_in_screen,
      ui::wm::WindowType type,
      int shell_window_id) override;
  std::unique_ptr<WindowOwner> CreateToplevelTestWindow(
      const gfx::Rect& bounds_in_screen,
      int shell_window_id) override;
  display::Display GetSecondaryDisplay() override;
  bool SetSecondaryDisplayPlacement(
      display::DisplayPlacement::Position position,
      int offset) override;
  void ConfigureWidgetInitParamsForDisplay(
      WmWindow* window,
      views::Widget::InitParams* init_params) override;
  void AddTransientChild(WmWindow* parent, WmWindow* window) override;

 private:
  std::unique_ptr<test::AshTestBase> ash_test_base_;

  DISALLOW_COPY_AND_ASSIGN(AshTestImplAura);
};

}  // namespace ash

#endif  // ASH_MUS_TEST_ASH_TEST_IMPL_AURA_H_
