// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_MATERIAL_DESIGN_CONTROLLER_TEST_API_H_
#define ASH_TEST_MATERIAL_DESIGN_CONTROLLER_TEST_API_H_

#include "ash/material_design/material_design_controller.h"
#include "base/macros.h"

namespace ash {
namespace test {

// Test API to access the internal state of the MaterialDesignController class.
// Creating an instance of this class and then destroying it preserves global
// state in MaterialDesignController class.
class MaterialDesignControllerTestAPI {
 public:
  explicit MaterialDesignControllerTestAPI(MaterialDesignController::Mode mode);
  ~MaterialDesignControllerTestAPI();

  // Wrapper functions for MaterialDesignController internal functions.
  static MaterialDesignController::Mode DefaultMode();
  static void Uninitialize();

 private:
  const MaterialDesignController::Mode previous_mode_;

  DISALLOW_COPY_AND_ASSIGN(MaterialDesignControllerTestAPI);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_MATERIAL_DESIGN_CONTROLLER_TEST_API_H_
