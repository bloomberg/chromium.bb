// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains stub implementations of the functions declared in
// browser_dialogs.h that are currently unimplemented in GTK-views.

#include <gtk/gtk.h>

#include "base/logging.h"
#include "chrome/browser/ui/gtk/edit_search_engine_dialog.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#include "chrome/browser/ui/webui/collected_cookies_ui_delegate.h"
#include "ui/gfx/native_widget_types.h"

#if !defined(WEBUI_TASK_MANAGER)
#include "chrome/browser/ui/gtk/task_manager_gtk.h"
#endif

#if !defined(OS_CHROMEOS)
#include "chrome/browser/ui/gtk/collected_cookies_gtk.h"
#include "chrome/browser/ui/gtk/repost_form_warning_gtk.h"
#endif

namespace browser {

#if !defined(WEBUI_TASK_MANAGER)
void ShowTaskManager() {
  TaskManagerGtk::Show(false);
}

void ShowBackgroundPages() {
  TaskManagerGtk::Show(true);
}
#endif

void EditSearchEngine(gfx::NativeWindow parent,
                      const TemplateURL* template_url,
                      EditSearchEngineControllerDelegate* delegate,
                      Profile* profile) {
  new EditSearchEngineDialog(GTK_WINDOW(parent), template_url, NULL, profile);
}

}  // namespace browser
