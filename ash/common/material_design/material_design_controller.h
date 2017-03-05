// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_MATERIAL_DESIGN_MATERIAL_DESIGN_CONTROLLER_H_
#define ASH_COMMON_MATERIAL_DESIGN_MATERIAL_DESIGN_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/macros.h"

namespace ash {

// TODO(estade): deprecated, remove: crbug.com/690957
class ASH_EXPORT MaterialDesignController {
 public:
  // Returns true.
  static bool IsShelfMaterial();

  // Returns true.
  static bool IsSystemTrayMenuMaterial();

  // Returns true.
  static bool UseMaterialDesignSystemIcons();

 private:
  // Declarations only. Do not allow construction of an object.
  MaterialDesignController();
  ~MaterialDesignController();

  DISALLOW_COPY_AND_ASSIGN(MaterialDesignController);
};

}  // namespace ash

#endif  // ASH_COMMON_MATERIAL_DESIGN_MATERIAL_DESIGN_CONTROLLER_H_
