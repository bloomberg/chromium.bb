// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gpu/gpu_mode_manager.h"

#include "base/bind.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/gpu_data_manager.h"

// static
void GpuModeManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      prefs::kHardwareAccelerationModeEnabled, true);
}

GpuModeManager::GpuModeManager() {
  if (g_browser_process->local_state()) {  // Skip for unit tests
    pref_registrar_.Init(g_browser_process->local_state());
    // Do nothing when the pref changes. It takes effect after
    // chrome restarts.
    pref_registrar_.Add(
        prefs::kHardwareAccelerationModeEnabled,
        base::Bind(&base::DoNothing));

    if (!IsGpuModePrefEnabled()) {
      content::GpuDataManager* gpu_data_manager =
          content::GpuDataManager::GetInstance();
      DCHECK(gpu_data_manager);
      gpu_data_manager->DisableHardwareAcceleration();
    }
  }
}

GpuModeManager::~GpuModeManager() {
}

// static
bool GpuModeManager::IsGpuModePrefEnabled() {
  PrefService* service = g_browser_process->local_state();
  DCHECK(service);
  return service->GetBoolean(
      prefs::kHardwareAccelerationModeEnabled);
}

