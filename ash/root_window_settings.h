// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ROOT_WINDOW_SETTINGS_H_
#define ASH_ROOT_WINDOW_SETTINGS_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"

namespace aura {
class RootWindow;
}

namespace ash {
namespace internal {

class RootWindowController;

// Per root window information should be stored here
// instead of using plain aura root window property because
// it can prevent mis-using on non root window.
struct RootWindowSettings {
  RootWindowSettings();

  // Indicate if the window in the active workspace should
  // use the transparent "solo-window" header style.
  bool solo_window_header;

  // ID of the display associated with the root window.
  int64 display_id;

  // RootWindowController for the root window. This may be NULL
  // for the root window used for mirroring.
  RootWindowController* controller;
};

// Initializes and returns RootWindowSettings for |root|.
// It is owned by the |root|.
RootWindowSettings* InitRootWindowSettings(aura::RootWindow* root);

// Returns the RootWindowSettings for |root|.
ASH_EXPORT RootWindowSettings* GetRootWindowSettings(aura::RootWindow* root);

// const version of GetRootWindowSettings.
ASH_EXPORT const RootWindowSettings*
GetRootWindowSettings(const aura::RootWindow* root);

}  // namespace internal
}  // namespace ash

#endif  // ASH_ROOT_WINDOW_SETTINGS_H_
