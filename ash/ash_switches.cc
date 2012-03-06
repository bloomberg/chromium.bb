// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_switches.h"

#include <string>

#include "base/command_line.h"

namespace ash {
namespace switches {

// Use the in-progress uber system tray.
const char kAshUberTray[] = "ash-uber-tray";

// Force the "compact" window mode regardless of the value of kAuraWindowMode.
// This can be used to override a value set in chrome://flags.
// Also implies fully-opaque windows for performance.
// TODO(derat): Remove this once the normal mode is usable on all platforms.
const char kAuraForceCompactWindowMode[] = "aura-force-compact-window-mode";

// Use Google-style dialog box frames.
const char kAuraGoogleDialogFrames[] = "aura-google-dialog-frames";

// (Most) Chrome OS hardware reports ACPI power button releases correctly.
// Standard hardware reports releases immediately after presses.  If set, we
// lock the screen or shutdown the system immediately in response to a press
// instead of displaying an interactive animation.
const char kAuraLegacyPowerButton[] = "aura-legacy-power-button";

// Avoid drawing drop shadows under windows.
const char kAuraNoShadows[] = "aura-no-shadows";

// If present animations are disabled.
const char kAuraWindowAnimationsDisabled[] = "aura-window-animations-disabled";

// Use a custom window style, e.g. --aura-window-mode=compact.
// When this flag is not passed we default to "overlapping" mode.
const char kAuraWindowMode[] = "aura-window-mode";

// Show only a single maximized window, like traditional non-Aura builds.
// Useful for low-resolution screens, such as on laptops.
const char kAuraWindowModeCompact[] = "compact";

// Smart window management with workspace manager that automatically lays out
// windows.
const char kAuraWindowModeManaged[] = "managed";

// Use Aura to manage windows of type WINDOW_TYPE_PANEL.
const char kAuraPanelManager[] = "aura-panels";

}  // namespace switches
}  // namespace ash
