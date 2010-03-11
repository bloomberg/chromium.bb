// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/options/exception_editor_view.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/content_exceptions_table_model.h"
#include "chrome/browser/host_content_settings_map.h"
#include "googleurl/src/url_canon.h"
#include "googleurl/src/url_parse.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "views/grid_layout.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/standard_layout.h"
#include "views/window/window.h"

namespace {

// Returns true if the host name is valid.
bool ValidHost(const std::string& host) {
  if (host.empty())
    return false;

  url_canon::CanonHostInfo host_info;
  return !net::CanonicalizeHost(host, &host_info).empty();
}

}  // namespace

ExceptionEditorView::ExceptionEditorView(Delegate* delegate,
                                         ContentExceptionsTableModel* model,
                                         int index,
                                         const std::string& host,
                                         ContentSetting setting)
    : delegate_(delegate),
      model_(model),
      cb_model_(model->content_type() == CONTENT_SETTINGS_TYPE_COOKIES),
      index_(index),
      host_(host),
      setting_(setting) {
  Init();
}

void ExceptionEditorView::Show(gfx::NativeWindow parent) {
  views::Window* window =
      views::Window::CreateChromeWindow(parent, gfx::Rect(), this);
  window->Show();
  GetDialogClientView()->UpdateDialogButtons();
  host_tf_->SelectAll();
  host_tf_->RequestFocus();
}

bool ExceptionEditorView::IsModal() const {
  return true;
}

std::wstring ExceptionEditorView::GetWindowTitle() const {
  return is_new() ? l10n_util::GetString(IDS_EXCEPTION_EDITOR_NEW_TITLE) :
                    l10n_util::GetString(IDS_EXCEPTION_EDITOR_TITLE);
}

bool ExceptionEditorView::IsDialogButtonEnabled(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK) {
    return IsHostValid(UTF16ToUTF8(host_tf_->text()));
  }
  return true;
}

bool ExceptionEditorView::Cancel() {
  return true;
}

bool ExceptionEditorView::Accept() {
  std::string new_host = UTF16ToUTF8(host_tf_->text());
  ContentSetting setting =
      cb_model_.SettingForIndex(action_cb_->selected_item());
  delegate_->AcceptExceptionEdit(new_host, setting, index_, is_new());
  return true;
}

views::View* ExceptionEditorView::GetContentsView() {
  return this;
}

void ExceptionEditorView::ContentsChanged(views::Textfield* sender,
                                          const std::wstring& new_contents) {
  GetDialogClientView()->UpdateDialogButtons();
  UpdateImageView(host_iv_, IsHostValid(UTF16ToUTF8(host_tf_->text())));
}

bool ExceptionEditorView::HandleKeystroke(
    views::Textfield* sender,
    const views::Textfield::Keystroke& key) {
  return false;
}

void ExceptionEditorView::Init() {
  using views::GridLayout;

  host_tf_ = new views::Textfield();
  host_tf_->SetText(UTF8ToUTF16(host_));
  host_tf_->SetController(this);

  host_iv_ = new views::ImageView;

  UpdateImageView(host_iv_, IsHostValid(UTF16ToUTF8(host_tf_->text())));

  action_cb_ = new views::Combobox(&cb_model_);
  if (!is_new())
    action_cb_->SetSelectedItem(cb_model_.IndexForSetting(setting_));

  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  // For the Textfields.
  views::ColumnSet* column_set = layout->AddColumnSet(1);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  // Add the contents.
  layout->StartRow(0, 1);
  layout->AddView(CreateLabel(IDS_EXCEPTION_EDITOR_HOST_TITLE));
  layout->AddView(host_tf_);
  layout->AddView(host_iv_);

  layout->StartRowWithPadding(0, 1, 0, kRelatedControlVerticalSpacing);
  layout->AddView(CreateLabel(IDS_EXCEPTION_EDITOR_ACTION_TITLE));
  layout->AddView(action_cb_);
}

views::Label* ExceptionEditorView::CreateLabel(int message_id) {
  views::Label* label = new views::Label(l10n_util::GetString(message_id));
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  return label;
}

bool ExceptionEditorView::IsHostValid(const std::string& host) const {
  bool is_valid_host = ValidHost(host) &&
      (model_->IndexOfExceptionByHost(host) == -1);

  return is_new() ? is_valid_host : (!host.empty() &&
      ((host_ == host) ||  is_valid_host));
}

void ExceptionEditorView::UpdateImageView(views::ImageView* image_view,
                                          bool is_valid) {
   return image_view->SetImage(
       ResourceBundle::GetSharedInstance().GetBitmapNamed(
           is_valid ? IDR_INPUT_GOOD : IDR_INPUT_ALERT));
}
