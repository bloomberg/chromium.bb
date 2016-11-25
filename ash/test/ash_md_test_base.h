// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_ASH_MD_TEST_BASE_H_
#define ASH_TEST_ASH_MD_TEST_BASE_H_

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/test/material_design_controller_test_api.h"
#include "ash/test/ash_test_base.h"

namespace ash {
namespace test {

class AshMDTestBase
    : public AshTestBase,
      public testing::WithParamInterface<ash::MaterialDesignController::Mode> {
 public:
  AshMDTestBase();
  ~AshMDTestBase() override;

  // AshTestBase:
  void SetUp() override;

  int GetMdMaximizedWindowHeightOffset();
  int GetMdAutoHiddenShelfHeightOffset();

 private:
  // The material design shelf is taller (by 1px) so use this offset to
  // adjust the expected height of a maximized window.
  int md_maximized_window_height_offset_ = 0;

  // The material design auto hidden shelf is shorter (by 3) so use this offset
  // to adjust the display/root window size.
  int md_auto_hidden_shelf_height_offset_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AshMDTestBase);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_ASH_MD_TEST_BASE_H_
