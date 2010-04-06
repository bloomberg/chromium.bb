// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_SYNC_CUSTOMIZE_CONTROLLER_CPPSAFE_H_
#define CHROME_BROWSER_COCOA_SYNC_CUSTOMIZE_CONTROLLER_CPPSAFE_H_

class ProfileSyncService;

#include "gfx/native_widget_types.h"

// Implementation is in sync_customize_controller.mm.

// Creates a sync customize dialog which is modal to parent_window.
// The dialog automatically cleans itself up on close.  It should be
// impossible to close and destroy parent_window while the customize
// dialog remains open.
void ShowSyncCustomizeDialog(gfx::NativeWindow parent_window,
                             ProfileSyncService* sync_service);

#endif  // CHROME_BROWSER_COCOA_SYNC_CUSTOMIZE_CONTROLLER_CPPSAFE_H_
