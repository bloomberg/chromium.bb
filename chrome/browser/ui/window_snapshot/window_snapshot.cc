// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_snapshot/window_snapshot.h"

#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "ui/snapshot/snapshot.h"

namespace chrome {

bool GrabWindowSnapshotForUser(
    gfx::NativeWindow window,
    std::vector<unsigned char>* png_representation,
    const gfx::Rect& snapshot_bounds) {
  if (g_browser_process->local_state()->GetBoolean(prefs::kDisableScreenshots))
    return false;
  return ui::GrabWindowSnapshot(window, png_representation,
      snapshot_bounds);
}

void RegisterScreenshotPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kDisableScreenshots, false);
}

}  // namespace chrome
