// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APPS_APPS_METRO_HANDLER_WIN_H_
#define CHROME_BROWSER_UI_APPS_APPS_METRO_HANDLER_WIN_H_

#include "ui/gfx/native_widget_types.h"

// Check if there are apps running and if not, return true. Otherwise, Show a
// modal dialog on |parent| asking whether the user is OK with their packaged
// apps closing, in order to relaunch to or from Metro mode. Returns true if the
// user clicks OK.
bool VerifyMetroSwitchForApps(gfx::NativeWindow parent,
                              int win8_restart_command_id);

#endif  // CHROME_BROWSER_UI_APPS_APPS_METRO_HANDLER_WIN_H_
