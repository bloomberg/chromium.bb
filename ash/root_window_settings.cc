// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/root_window_settings.h"

#include "ui/aura/root_window.h"
#include "ui/aura/window_property.h"
#include "ui/gfx/display.h"

DECLARE_WINDOW_PROPERTY_TYPE(ash::internal::RootWindowSettings*);

namespace ash {
namespace internal {

DEFINE_OWNED_WINDOW_PROPERTY_KEY(RootWindowSettings,
                                 kRootWindowSettingsKey, NULL);

RootWindowSettings::RootWindowSettings()
    : solo_window_header(false),
      display_id(gfx::Display::kInvalidDisplayID),
      controller(NULL) {
}

RootWindowSettings* InitRootWindowSettings(aura::RootWindow* root) {
  RootWindowSettings* settings = new RootWindowSettings();
  root->SetProperty(kRootWindowSettingsKey, settings);
  return settings;
}

RootWindowSettings* GetRootWindowSettings(aura::RootWindow* root) {
  return root->GetProperty(kRootWindowSettingsKey);
}

const RootWindowSettings* GetRootWindowSettings(const aura::RootWindow* root) {
  return root->GetProperty(kRootWindowSettingsKey);
}

}  // namespace internal
}  // namespace ash
