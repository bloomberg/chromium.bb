// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_MATERIAL_DESIGN_MATERIAL_DESIGN_CONTROLLER_H_
#define ASH_COMMON_MATERIAL_DESIGN_MATERIAL_DESIGN_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/macros.h"

namespace ash {

namespace test {
class MaterialDesignControllerTestAPI;
}  // namespace test

// Central controller to handle material design modes.
class ASH_EXPORT MaterialDesignController {
 public:
  // The different material design modes for Chrome OS system UI.
  enum Mode {
    // Not initialized.
    UNINITIALIZED = -1,
    // Classic, non-material design.
    NON_MATERIAL = 0,
    // Basic material design.
    MATERIAL_NORMAL = 1,
    // Material design with experimental features.
    MATERIAL_EXPERIMENTAL = 2
  };

  // Initializes |mode_|. Must be called before calling IsMaterial(),
  // IsMaterialExperimental(), IsMaterialNormal(), or GetMode().
  static void Initialize();

  // Returns the currently initialized MaterialDesignController::Mode type for
  // Chrome OS system UI.
  static Mode GetMode();

  // Returns true if overview mode is using Material Design.
  static bool IsOverviewMaterial();

  // Returns true if Material Design features are enabled for Chrome OS shelf.
  static bool IsShelfMaterial();

  // Returns true if material design versions of icons should be used in the
  // status tray and system menu.
  static bool UseMaterialDesignSystemIcons();

 private:
  friend class test::MaterialDesignControllerTestAPI;

  // Declarations only. Do not allow construction of an object.
  MaterialDesignController();
  ~MaterialDesignController();

  // Material Design |Mode| for Chrome OS system UI. Used only by tests.
  static Mode mode();

  // Returns true if Material Design is enabled in Chrome OS system UI.
  // Maps to "ash-md" flag "enabled" or "experimental" values.
  static bool IsMaterial();

  // Returns true if Material Design normal features are enabled in Chrome OS
  // system UI. Maps to "--ash-md=enabled" command line switch value.
  static bool IsMaterialNormal();

  // Returns true if Material Design experimental features are enabled in
  // Chrome OS system UI. Maps to "--ash-md=experimental" command line switch
  // value.
  static bool IsMaterialExperimental();

  // Returns the per-platform default material design variant.
  static Mode DefaultMode();

  // Sets |mode_| to |mode|. Can be used by tests to directly set the mode.
  static void SetMode(Mode mode);

  // Resets the initialization state to uninitialized. To be used by tests to
  // allow calling Initialize() more than once.
  static void Uninitialize();

  DISALLOW_COPY_AND_ASSIGN(MaterialDesignController);
};

}  // namespace ash

#endif  // ASH_COMMON_MATERIAL_DESIGN_MATERIAL_DESIGN_CONTROLLER_H_
