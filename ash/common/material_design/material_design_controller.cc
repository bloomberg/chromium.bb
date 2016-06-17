// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/common/ash_switches.h"
#include "ash/common/material_design/material_design_controller.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"

#if defined(OS_CHROMEOS)
#include "ui/chromeos/material_design_icon_controller.h"
#endif  // OS_CHROMEOS

namespace ash {

namespace {
MaterialDesignController::Mode mode_ =
    MaterialDesignController::Mode::UNINITIALIZED;
}  // namespace

// static
void MaterialDesignController::Initialize() {
  TRACE_EVENT0("startup", "ash::MaterialDesignController::InitializeMode");
  DCHECK_EQ(mode_, Mode::UNINITIALIZED);

  const std::string switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          ash::switches::kAshMaterialDesign);

  if (switch_value == ash::switches::kAshMaterialDesignExperimental) {
    SetMode(Mode::MATERIAL_EXPERIMENTAL);
  } else if (switch_value == ash::switches::kAshMaterialDesignEnabled) {
    SetMode(Mode::MATERIAL_NORMAL);
  } else if (switch_value == ash::switches::kAshMaterialDesignDisabled) {
    SetMode(Mode::NON_MATERIAL);
  } else {
    if (!switch_value.empty()) {
      LOG(ERROR) << "Invalid value='" << switch_value
                 << "' for command line switch '"
                 << ash::switches::kAshMaterialDesign << "'.";
    }
    SetMode(DefaultMode());
  }

#if defined(OS_CHROMEOS)
  ui::md_icon_controller::SetUseMaterialDesignNetworkIcons(
      UseMaterialDesignSystemIcons());
#endif  // OS_CHROMEOS
}

// static
MaterialDesignController::Mode MaterialDesignController::GetMode() {
  DCHECK_NE(mode_, Mode::UNINITIALIZED);
  return mode_;
}

// static
bool MaterialDesignController::IsOverviewMaterial() {
  return MaterialDesignController::IsMaterial();
}

// static
bool MaterialDesignController::IsShelfMaterial() {
  return MaterialDesignController::IsMaterialExperimental();
}

// static
bool MaterialDesignController::UseMaterialDesignSystemIcons() {
  return MaterialDesignController::IsMaterialExperimental();
}

// static
MaterialDesignController::Mode MaterialDesignController::mode() {
  return mode_;
}

// static
bool MaterialDesignController::IsMaterial() {
  return IsMaterialExperimental() || IsMaterialNormal();
}

// static
bool MaterialDesignController::IsMaterialNormal() {
  return GetMode() == Mode::MATERIAL_NORMAL;
}

// static
bool MaterialDesignController::IsMaterialExperimental() {
  return GetMode() == Mode::MATERIAL_EXPERIMENTAL;
}

// static
MaterialDesignController::Mode MaterialDesignController::DefaultMode() {
  return Mode::MATERIAL_NORMAL;
}

// static
void MaterialDesignController::SetMode(Mode mode) {
  mode_ = mode;
}

// static
void MaterialDesignController::Uninitialize() {
  mode_ = Mode::UNINITIALIZED;
}

}  // namespace ash
