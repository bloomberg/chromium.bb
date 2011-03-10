// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OPTIONS_EXCEPTION_EDITOR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OPTIONS_EXCEPTION_EDITOR_VIEW_H_
#pragma once

#include <string>

#include "chrome/browser/content_setting_combo_model.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/textfield/textfield_controller.h"
#include "views/window/dialog_delegate.h"

namespace views {
class ImageView;
class Label;
}

class ContentExceptionsTableModel;

// ExceptionEditorView is responsible for showing a dialog that allows the user
// to create or edit a single content exception. If the user clicks ok the
// delegate is notified and completes the edit.
//
// To use an ExceptionEditorView create one and invoke Show on it.
// ExceptionEditorView is deleted when the dialog closes.
class ExceptionEditorView : public views::View,
                            public views::TextfieldController,
                            public views::DialogDelegate {
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

  // Creates a new ExceptionEditorView with the supplied args. |index| is the
  // index into the ContentExceptionsTableModel of the exception. This is not
  // used by ExceptionEditorView but instead passed to the delegate.
  ExceptionEditorView(Delegate* delegate,
                      ContentExceptionsTableModel* model,
                      bool allow_off_the_record,
                      int index,
                      const ContentSettingsPattern& pattern,
                      ContentSetting setting,
                      bool is_off_the_record);
  virtual ~ExceptionEditorView() {}

  void Show(gfx::NativeWindow parent);

  // views::DialogDelegate overrides.
  virtual bool IsModal() const;
  virtual std::wstring GetWindowTitle() const;
  virtual bool IsDialogButtonEnabled(
      MessageBoxFlags::DialogButton button) const;
  virtual bool Cancel();
  virtual bool Accept();
  virtual views::View* GetContentsView();

  // views::TextfieldController:
  // Updates whether the user can accept the dialog as well as updating image
  // views showing whether value is valid.
  virtual void ContentsChanged(views::Textfield* sender,
                               const std::wstring& new_contents);
  virtual bool HandleKeyEvent(views::Textfield* sender,
                              const views::KeyEvent& key_event);

 private:
  void Init();

  views::Label* CreateLabel(int message_id);

  // Returns true if we're adding a new item.
  bool is_new() const { return index_ == -1; }

  bool IsPatternValid(const ContentSettingsPattern& pattern,
                      bool is_off_the_record) const;

  void UpdateImageView(views::ImageView* image_view, bool is_valid);

  Delegate* delegate_;
  ContentExceptionsTableModel* model_;
  ContentSettingComboModel cb_model_;

  // Index of the item being edited. If -1, indices this is a new entry.
  const bool allow_off_the_record_;
  const int index_;
  const ContentSettingsPattern pattern_;
  const ContentSetting setting_;
  const bool is_off_the_record_;

  views::Textfield* pattern_tf_;
  views::ImageView* pattern_iv_;
  views::Combobox* action_cb_;
  views::Checkbox* incognito_cb_;

  DISALLOW_COPY_AND_ASSIGN(ExceptionEditorView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OPTIONS_EXCEPTION_EDITOR_VIEW_H_
