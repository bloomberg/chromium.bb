// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_EDIT_SEARCH_ENGINE_DIALOG_H_
#define CHROME_BROWSER_UI_GTK_EDIT_SEARCH_ENGINE_DIALOG_H_
#pragma once

#include <gtk/gtk.h>
#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "ui/base/gtk/gtk_signal.h"

class EditSearchEngineController;
class EditSearchEngineControllerDelegate;
class Profile;
class TemplateURL;

class EditSearchEngineDialog {
 public:
  EditSearchEngineDialog(GtkWindow* parent_window,
                         const TemplateURL* template_url,
                         EditSearchEngineControllerDelegate* delegate,
                         Profile* profile);
  virtual ~EditSearchEngineDialog();

 private:
  // Create and show the window.
  void Init(GtkWindow* parent_window, Profile* profile);

  // Retrieve the user input in the various fields.
  string16 GetTitleInput() const;
  string16 GetKeywordInput() const;
  std::string GetURLInput() const;

  // Set sensitivity of buttons based on entry state.
  void EnableControls();

  // Updates the tooltip and image of the image view based on is_valid. If
  // is_valid is false the tooltip of the image view is set to the message with
  // id invalid_message_id, otherwise the tooltip is set to the empty text.
  void UpdateImage(GtkWidget* image, bool is_valid, int invalid_message_id);

  // Callback for entry changes.
  CHROMEG_CALLBACK_0(EditSearchEngineDialog, void, OnEntryChanged,
                     GtkEditable*);

  // Callback for dialog buttons.
  CHROMEGTK_CALLBACK_1(EditSearchEngineDialog, void, OnResponse, int);

  // Callback for window destruction.
  CHROMEGTK_CALLBACK_0(EditSearchEngineDialog, void, OnWindowDestroy);

  // The dialog window.
  GtkWidget* dialog_;

  // Text entries for each field.
  GtkWidget* title_entry_;
  GtkWidget* keyword_entry_;
  GtkWidget* url_entry_;

  // Images showing whether each entry is okay or has errors.
  GtkWidget* title_image_;
  GtkWidget* keyword_image_;
  GtkWidget* url_image_;

  // The ok button (we need a reference to it so we can de-activate it when the
  // entries are not all filled in.)
  GtkWidget* ok_button_;

  scoped_ptr<EditSearchEngineController> controller_;

  DISALLOW_COPY_AND_ASSIGN(EditSearchEngineDialog);
};

#endif  // CHROME_BROWSER_UI_GTK_EDIT_SEARCH_ENGINE_DIALOG_H_
