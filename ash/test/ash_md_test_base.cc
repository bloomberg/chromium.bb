// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_md_test_base.h"

#include "ash/common/shelf/shelf_constants.h"
#include "ash/test/material_design_controller_test_api.h"

namespace ash {
namespace test {

AshMDTestBase::AshMDTestBase() {}

AshMDTestBase::~AshMDTestBase() {}

void AshMDTestBase::SetUp() {
  AshTestBase::SetUp();

  material_design_state_.reset(new test::MaterialDesignControllerTestAPI(
      MaterialDesignController::Mode::NON_MATERIAL));
  const int non_md_shelf_size = GetShelfConstant(SHELF_SIZE);

  material_design_state_.reset(
      new test::MaterialDesignControllerTestAPI(GetParam()));
  const int md_state_shelf_size = GetShelfConstant(SHELF_SIZE);

  md_maximized_window_height_offset_ = non_md_shelf_size - md_state_shelf_size;
}

void AshMDTestBase::TearDown() {
  material_design_state_.reset();
  AshTestBase::TearDown();
}

int AshMDTestBase::GetMdMaximizedWindowHeightOffset() {
  return md_maximized_window_height_offset_;
}

}  // namespace test
}  // namespace ash
