// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_WEB_DIALOG_UTIL_H_
#define CHROME_BROWSER_UI_ASH_WEB_DIALOG_UTIL_H_

#include "ui/gfx/native_widget_types.h"

namespace content {
class BrowserContext;
}

namespace ui {
class WebDialogDelegate;
}

namespace chrome {

// Creates and shows an HTML dialog with the given delegate and context. The
// dialog is created as a child of |parent|. If |parent| is null the dialog
// will be placed in a fallback container window in the ash window hierarchy.
// See ash/public/cpp/shell_window_ids.h for |container_id| values. The window
// is destroyed when it is closed. See also chrome::ShowWebDialog().
void ShowWebDialogWithContainer(gfx::NativeView parent,
                                int container_id,
                                content::BrowserContext* context,
                                ui::WebDialogDelegate* delegate);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_ASH_WEB_DIALOG_UTIL_H_
