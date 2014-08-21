// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APPS_APP_INFO_DIALOG_H_
#define CHROME_BROWSER_UI_APPS_APP_INFO_DIALOG_H_

#include "base/callback_forward.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace extensions {
class Extension;
}

namespace gfx {
class Rect;
}

// Shows the chrome app information dialog box.
void ShowAppInfoDialog(gfx::NativeWindow parent,
                       const gfx::Rect& bounds,
                       Profile* profile,
                       const extensions::Extension* app,
                       const base::Closure& close_callback);

#endif  // CHROME_BROWSER_UI_APPS_APP_INFO_DIALOG_H_
