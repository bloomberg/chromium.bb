// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_md_test_base.h"

#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/test/material_design_controller_test_api.h"

namespace ash {
namespace test {

AshMDTestBase::AshMDTestBase() {}

AshMDTestBase::~AshMDTestBase() {}

void AshMDTestBase::SetUp() {
  int non_md_shelf_size = 0;
  int non_md_auto_hide_shelf_size = 0;
  int md_shelf_size = 0;
  int md_auto_hide_shelf_size = 0;

  {
    test::MaterialDesignControllerTestAPI md_state(
        MaterialDesignController::Mode::NON_MATERIAL);
    non_md_shelf_size = GetShelfConstant(SHELF_SIZE);
    non_md_auto_hide_shelf_size = GetShelfConstant(SHELF_INSETS_FOR_AUTO_HIDE);
  }

  {
    test::MaterialDesignControllerTestAPI md_state(GetParam());
    md_shelf_size = GetShelfConstant(SHELF_SIZE);
    md_auto_hide_shelf_size = GetShelfConstant(SHELF_INSETS_FOR_AUTO_HIDE);
  }

  md_maximized_window_height_offset_ = non_md_shelf_size - md_shelf_size;
  md_auto_hidden_shelf_height_offset_ =
      non_md_auto_hide_shelf_size - md_auto_hide_shelf_size;

  set_material_mode(GetParam());

  AshTestBase::SetUp();
}

int AshMDTestBase::GetMdMaximizedWindowHeightOffset() {
  return md_maximized_window_height_offset_;
}

int AshMDTestBase::GetMdAutoHiddenShelfHeightOffset() {
  return md_auto_hidden_shelf_height_offset_;
}

}  // namespace test
}  // namespace ash
