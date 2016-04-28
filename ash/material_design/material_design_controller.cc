// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/ash_switches.h"
#include "ash/material_design/material_design_controller.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"

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
}

// static
bool MaterialDesignController::IsMaterial() {
  return IsMaterialExperimental() || mode_ == Mode::MATERIAL_NORMAL;
}

// static
bool MaterialDesignController::IsMaterialExperimental() {
  DCHECK_NE(mode_, Mode::UNINITIALIZED);
  return mode_ == Mode::MATERIAL_EXPERIMENTAL;
}

// static
MaterialDesignController::Mode MaterialDesignController::mode() {
  return mode_;
}

// static
MaterialDesignController::Mode MaterialDesignController::DefaultMode() {
  return Mode::NON_MATERIAL;
}

// static
void MaterialDesignController::SetMode(Mode mode) {
  mode_ = mode;
}

// static
void MaterialDesignController::Uninitialize() {
  mode_ = Mode::UNINITIALIZED;
}

}  // namespace ui
