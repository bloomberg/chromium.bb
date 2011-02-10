// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/options/exception_editor_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/content_exceptions_table_model.h"
#include "googleurl/src/url_canon.h"
#include "googleurl/src/url_parse.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"
#include "views/window/window.h"

ExceptionEditorView::ExceptionEditorView(
    Delegate* delegate,
    ContentExceptionsTableModel* model,
    bool allow_off_the_record,
    int index,
    const ContentSettingsPattern& pattern,
    ContentSetting setting,
    bool is_off_the_record)
    : delegate_(delegate),
      model_(model),
      cb_model_(model->content_type()),
      allow_off_the_record_(allow_off_the_record),
      index_(index),
      pattern_(pattern),
      setting_(setting),
      is_off_the_record_(is_off_the_record) {
  // Geolocation exceptions are handled by SimpleContentExceptionsView.
  DCHECK_NE(setting_, CONTENT_SETTINGS_TYPE_GEOLOCATION);
  // Notification exceptions are handled by SimpleContentExceptionsView.
  DCHECK_NE(setting_, CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
  Init();
}

void ExceptionEditorView::Show(gfx::NativeWindow parent) {
  views::Window* window =
      views::Window::CreateChromeWindow(parent, gfx::Rect(), this);
  window->Show();
  GetDialogClientView()->UpdateDialogButtons();
  pattern_tf_->SelectAll();
  pattern_tf_->RequestFocus();
}

bool ExceptionEditorView::IsModal() const {
  return true;
}

std::wstring ExceptionEditorView::GetWindowTitle() const {
  if (is_new())
    return UTF16ToWide(
        l10n_util::GetStringUTF16(IDS_EXCEPTION_EDITOR_NEW_TITLE));

  return UTF16ToWide(l10n_util::GetStringUTF16(IDS_EXCEPTION_EDITOR_TITLE));
}

bool ExceptionEditorView::IsDialogButtonEnabled(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK) {
    return IsPatternValid(ContentSettingsPattern(
                          UTF16ToUTF8(pattern_tf_->text())),
                          incognito_cb_->checked());
  }
  return true;
}

bool ExceptionEditorView::Cancel() {
  return true;
}

bool ExceptionEditorView::Accept() {
  ContentSettingsPattern new_pattern(UTF16ToUTF8(pattern_tf_->text()));
  ContentSetting setting =
      cb_model_.SettingForIndex(action_cb_->selected_item());
  bool is_off_the_record = incognito_cb_->checked();
  delegate_->AcceptExceptionEdit(
      new_pattern, setting, is_off_the_record, index_, is_new());
  return true;
}

views::View* ExceptionEditorView::GetContentsView() {
  return this;
}

void ExceptionEditorView::ContentsChanged(views::Textfield* sender,
                                          const std::wstring& new_contents) {
  GetDialogClientView()->UpdateDialogButtons();
  UpdateImageView(pattern_iv_, IsPatternValid(ContentSettingsPattern(
      UTF16ToUTF8(pattern_tf_->text())), incognito_cb_->checked()));
}

bool ExceptionEditorView::HandleKeyEvent(views::Textfield* sender,
                                         const views::KeyEvent& key_event) {
  return false;
}

void ExceptionEditorView::Init() {
  using views::GridLayout;

  pattern_tf_ = new views::Textfield();
  pattern_tf_->SetText(UTF8ToUTF16(pattern_.AsString()));
  pattern_tf_->SetController(this);

  pattern_iv_ = new views::ImageView;

  UpdateImageView(pattern_iv_, IsPatternValid(ContentSettingsPattern(
      UTF16ToUTF8(pattern_tf_->text())), is_off_the_record_));

  action_cb_ = new views::Combobox(&cb_model_);
  if (!is_new())
    action_cb_->SetSelectedItem(cb_model_.IndexForSetting(setting_));

  incognito_cb_ = new views::Checkbox(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_EXCEPTION_EDITOR_OTR_TITLE)));
  incognito_cb_->SetChecked(is_off_the_record_);

  GridLayout* layout = GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  // For the Textfields.
  views::ColumnSet* column_set = layout->AddColumnSet(1);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  // Add the contents.
  layout->StartRow(0, 1);
  layout->AddView(CreateLabel(IDS_EXCEPTION_EDITOR_PATTERN_TITLE));
  layout->AddView(pattern_tf_);
  layout->AddView(pattern_iv_);

  layout->StartRowWithPadding(0, 1, 0, views::kRelatedControlVerticalSpacing);
  layout->AddView(CreateLabel(IDS_EXCEPTION_EDITOR_ACTION_TITLE));
  layout->AddView(action_cb_);

  if (allow_off_the_record_) {
    layout->StartRowWithPadding(0, 1, 0, views::kRelatedControlVerticalSpacing);
    layout->AddView(incognito_cb_, 3, 1, GridLayout::FILL, GridLayout::FILL);
  }
}

views::Label* ExceptionEditorView::CreateLabel(int message_id) {
  views::Label* label =
        new views::Label(UTF16ToWide(l10n_util::GetStringUTF16(message_id)));
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  return label;
}

bool ExceptionEditorView::IsPatternValid(
    const ContentSettingsPattern& pattern,
    bool is_off_the_record) const {
  bool is_valid_pattern = pattern.IsValid() &&
      (model_->IndexOfExceptionByPattern(pattern, is_off_the_record) == -1);

  return is_new() ? is_valid_pattern : (!pattern.AsString().empty() &&
      ((pattern_ == pattern) ||  is_valid_pattern));
}

void ExceptionEditorView::UpdateImageView(views::ImageView* image_view,
                                          bool is_valid) {
   return image_view->SetImage(
       ResourceBundle::GetSharedInstance().GetBitmapNamed(
           is_valid ? IDR_INPUT_GOOD : IDR_INPUT_ALERT));
}
