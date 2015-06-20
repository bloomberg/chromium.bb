// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASH_UTIL_H_
#define CHROME_BROWSER_UI_ASH_ASH_UTIL_H_

#include "ui/gfx/native_widget_types.h"

namespace ui {
class Accelerator;
}  // namespace ui

namespace chrome {

// Returns true if Ash should be run at startup.
bool ShouldOpenAshOnStartup();

// Returns true if |native_view/native_window| exists within the Ash
// environment.
bool IsNativeViewInAsh(gfx::NativeView native_view);
bool IsNativeWindowInAsh(gfx::NativeWindow native_window);

// Returns true if the given |accelerator| has been deprecated and hence can
// be consumed by web contents if needed.
bool IsAcceleratorDeprecated(const ui::Accelerator& accelerator);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_ASH_ASH_UTIL_H_
