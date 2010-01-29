// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains stub implementations of the functions declared in
// browser_dialogs.h that are currently unimplemented in GTK-views.

#include <gtk/gtk.h>

#include "base/logging.h"
#include "chrome/browser/gtk/about_chrome_dialog.h"
#include "chrome/browser/fonts_languages_window.h"
#include "chrome/browser/gtk/bookmark_manager_gtk.h"
#include "chrome/browser/gtk/clear_browsing_data_dialog_gtk.h"
#include "chrome/browser/gtk/edit_search_engine_dialog.h"
#include "chrome/browser/gtk/keyword_editor_view.h"
#include "chrome/browser/gtk/options/passwords_exceptions_window_gtk.h"
#include "chrome/browser/gtk/repost_form_warning_gtk.h"
#include "chrome/browser/gtk/task_manager_gtk.h"
#include "chrome/browser/options_window.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/browser_dialogs.h"
#include "views/widget/widget.h"

namespace browser {

void ShowClearBrowsingDataView(views::Widget* parent,
                               Profile* profile) {
  ClearBrowsingDataDialogGtk::Show(GTK_WINDOW(parent->GetNativeView()),
                                   profile);
}

void ShowSelectProfileDialog() {
  // Only matters if we're going to support profile switching
  // (switches::kEnableUserDataDirProfiles).
  NOTIMPLEMENTED();
}

void ShowImporterView(views::Widget* parent, Profile* profile) {
  // Import currently doesn't matter.
  NOTIMPLEMENTED();
}

void ShowBookmarkManagerView(Profile* profile) {
  BookmarkManagerGtk::Show(profile);
}

void ShowPasswordsExceptionsWindowView(Profile* profile) {
  ShowPasswordsExceptionsWindow(profile);
}

void ShowKeywordEditorView(Profile* profile) {
  KeywordEditorView::Show(profile);
}

void ShowNewProfileDialog() {
  // Hasn't been implemented yet on linux.
  NOTIMPLEMENTED();
}

void ShowTaskManager() {
  TaskManagerGtk::Show();
}

void EditSearchEngine(gfx::NativeWindow parent,
                      const TemplateURL* template_url,
                      EditSearchEngineControllerDelegate* delegate,
                      Profile* profile) {
  new EditSearchEngineDialog(GTK_WINDOW(parent), template_url, NULL, profile);
}

void ShowRepostFormWarningDialog(gfx::NativeWindow parent_window,
                                 TabContents* tab_contents) {
  new RepostFormWarningGtk(GTK_WINDOW(parent_window),
                           &tab_contents->controller());
}

}  // namespace browser
