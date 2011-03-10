// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains stub implementations of the functions declared in
// browser_dialogs.h that are currently unimplemented in GTK-views.

#include <gtk/gtk.h>

#include "base/logging.h"
#include "chrome/browser/fonts_languages_window.h"
#include "chrome/browser/ui/gtk/about_chrome_dialog.h"
#include "chrome/browser/ui/gtk/clear_browsing_data_dialog_gtk.h"
#include "chrome/browser/ui/gtk/collected_cookies_gtk.h"
#include "chrome/browser/ui/gtk/edit_search_engine_dialog.h"
#include "chrome/browser/ui/gtk/keyword_editor_view.h"
#include "chrome/browser/ui/gtk/options/content_settings_window_gtk.h"
#include "chrome/browser/ui/gtk/options/passwords_exceptions_window_gtk.h"
#include "chrome/browser/ui/gtk/repost_form_warning_gtk.h"
#include "chrome/browser/ui/gtk/task_manager_gtk.h"
#include "chrome/browser/ui/options/options_window.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "views/widget/widget.h"

namespace browser {

void ShowClearBrowsingDataView(views::Widget* parent,
                               Profile* profile) {
  ClearBrowsingDataDialogGtk::Show(GTK_WINDOW(parent->GetNativeView()),
                                   profile);
}

void ShowPasswordsExceptionsWindowView(Profile* profile) {
  ShowPasswordsExceptionsWindow(profile);
}

void ShowKeywordEditorView(Profile* profile) {
  KeywordEditorView::Show(profile);
}

void ShowTaskManager() {
  TaskManagerGtk::Show(false);
}

void ShowBackgroundPages() {
  TaskManagerGtk::Show(true);
}

void EditSearchEngine(gfx::NativeWindow parent,
                      const TemplateURL* template_url,
                      EditSearchEngineControllerDelegate* delegate,
                      Profile* profile) {
  new EditSearchEngineDialog(GTK_WINDOW(parent), template_url, NULL, profile);
}

void ShowRepostFormWarningDialog(gfx::NativeWindow parent_window,
                                 TabContents* tab_contents) {
  new RepostFormWarningGtk(GTK_WINDOW(parent_window), tab_contents);
}

void ShowContentSettingsWindow(gfx::NativeWindow parent_window,
                               ContentSettingsType content_type,
                               Profile* profile) {
  ContentSettingsWindowGtk::Show(parent_window, content_type, profile);
}

void ShowCollectedCookiesDialog(gfx::NativeWindow parent_window,
                                TabContents* tab_contents) {
  new CollectedCookiesGtk(GTK_WINDOW(parent_window), tab_contents);
}

}  // namespace browser
