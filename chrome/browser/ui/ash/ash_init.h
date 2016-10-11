// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASH_INIT_H_
#define CHROME_BROWSER_UI_ASH_ASH_INIT_H_

#include "ui/gfx/native_widget_types.h"

namespace chrome {

// Creates the Ash Shell and opens the Ash window. |remote_window| is only used
// on windows. It provides the HWND to the remote window.
void OpenAsh(gfx::AcceleratedWidget remote_window);

// Closes the Ash window and destroys the Ash Shell.
void CloseAsh();

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_ASH_ASH_INIT_H_
