// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APPS_APP_INFO_DIALOG_H_
#define CHROME_BROWSER_UI_APPS_APP_INFO_DIALOG_H_

#include "base/callback.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace extensions {
class Extension;
}

// Shows the chrome app information dialog box.
// |close_callback| may be null.
void ShowAppInfoDialog(gfx::NativeWindow parent_window,
                       Profile* profile,
                       const extensions::Extension* app,
                       const base::Closure& close_callback);

#endif  // CHROME_BROWSER_UI_APPS_APP_INFO_DIALOG_H_
