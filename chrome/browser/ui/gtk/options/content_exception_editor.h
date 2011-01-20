// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_OPTIONS_CONTENT_EXCEPTION_EDITOR_H_
#define CHROME_BROWSER_UI_GTK_OPTIONS_CONTENT_EXCEPTION_EDITOR_H_
#pragma once

#include <gtk/gtk.h>

#include "chrome/browser/content_exceptions_table_model.h"
#include "chrome/browser/content_setting_combo_model.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"
#include "ui/base/gtk/gtk_signal.h"

// An editor which lets the user create or edit an individual exception to the
// current content setting policy. (i.e. let www.google.com always show
// images). Modal to parent.
class ContentExceptionEditor {
 public:
  class Delegate {
   public:
    // Invoked when the user accepts the edit.
    virtual void AcceptExceptionEdit(
        const ContentSettingsPattern& pattern,
        ContentSetting setting,
        bool is_off_the_record,
        int index,
        bool is_new) = 0;

   protected:
    virtual ~Delegate() {}
  };

  ContentExceptionEditor(GtkWindow* parent,
                         Delegate* delegate,
                         ContentExceptionsTableModel* model,
                         bool allow_off_the_record,
                         int index,
                         const ContentSettingsPattern& pattern,
                         ContentSetting setting,
                         bool is_off_the_record);
  virtual ~ContentExceptionEditor() {}

 private:
  // Returns true if we're adding a new item.
  bool is_new() const { return index_ == -1; }

  bool IsPatternValid(const ContentSettingsPattern& pattern,
                      bool is_off_the_record) const;

  void UpdateImage(GtkWidget* image, bool is_valid);

  // GTK callbacks
  CHROMEGTK_CALLBACK_0(ContentExceptionEditor, void, OnEntryChanged);
  CHROMEGTK_CALLBACK_1(ContentExceptionEditor, void, OnResponse, int);
  CHROMEGTK_CALLBACK_0(ContentExceptionEditor, void, OnWindowDestroy);

  Delegate* delegate_;
  ContentExceptionsTableModel* model_;

  // The model for Combobox widget.
  ContentSettingComboModel cb_model_;

  // Index of the item being edited. If -1, indicates this is a new entry.
  const int index_;
  const ContentSettingsPattern pattern_;
  const ContentSetting setting_;

  // UI widgets.
  GtkWidget* dialog_;
  GtkWidget* entry_;
  GtkWidget* pattern_image_;
  GtkWidget* action_combo_;
  GtkWidget* otr_checkbox_;

  DISALLOW_COPY_AND_ASSIGN(ContentExceptionEditor);
};

#endif  // CHROME_BROWSER_UI_GTK_OPTIONS_CONTENT_EXCEPTION_EDITOR_H_
