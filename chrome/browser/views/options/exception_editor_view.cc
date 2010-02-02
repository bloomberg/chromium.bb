// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/options/exception_editor_view.h"

#include "app/l10n_util.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/views/options/content_exceptions_table_model.h"
#include "googleurl/src/url_canon.h"
#include "googleurl/src/url_parse.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "views/grid_layout.h"
#include "views/controls/label.h"
#include "views/standard_layout.h"
#include "views/window/window.h"

// The settings shown in the combobox if show_ask_ is false;
static const ContentSetting kNoAskSettings[] = { CONTENT_SETTING_ALLOW,
                                                 CONTENT_SETTING_BLOCK };

// The settings shown in the combobox if show_ask_ is true;
static const ContentSetting kAskSettings[] = { CONTENT_SETTING_ALLOW,
                                               CONTENT_SETTING_ASK,
                                               CONTENT_SETTING_BLOCK };

// Returns true if the host name is valid.
static bool ValidHost(const std::string& host) {
  if (host.empty())
    return false;

  url_canon::CanonHostInfo host_info;
  return !net::CanonicalizeHost(host, &host_info).empty();
}

int ExceptionEditorView::ActionComboboxModel::GetItemCount() {
  return show_ask_ ? arraysize(kAskSettings) : arraysize(kNoAskSettings);
}

std::wstring ExceptionEditorView::ActionComboboxModel::GetItemAt(int index) {
  switch (setting_for_index(index)) {
    case CONTENT_SETTING_ALLOW:
      return l10n_util::GetString(IDS_EXCEPTIONS_ALLOW_BUTTON);
    case CONTENT_SETTING_BLOCK:
      return l10n_util::GetString(IDS_EXCEPTIONS_BLOCK_BUTTON);
    case CONTENT_SETTING_ASK:
      return l10n_util::GetString(IDS_EXCEPTIONS_ASK_BUTTON);
    default:
      NOTREACHED();
  }
  return std::wstring();
}

ContentSetting ExceptionEditorView::ActionComboboxModel::setting_for_index(
    int index) {
  return show_ask_ ? kAskSettings[index] : kNoAskSettings[index];
}

int ExceptionEditorView::ActionComboboxModel::index_for_setting(
    ContentSetting setting) {
  for (int i = 0; i < GetItemCount(); ++i)
    if (setting_for_index(i) == setting)
      return i;
  NOTREACHED();
  return 0;
}

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
    std::string new_host = UTF16ToUTF8(host_tf_->text());
    if (is_new()) {
      return ValidHost(new_host) &&
          (model_->IndexOfExceptionByHost(new_host) == -1);
    }
    return !new_host.empty() &&
        (host_ == new_host ||
         (ValidHost(new_host) &&
          model_->IndexOfExceptionByHost(new_host) == -1));
  }
  return true;
}

bool ExceptionEditorView::Cancel() {
  return true;
}

bool ExceptionEditorView::Accept() {
  std::string new_host = UTF16ToUTF8(host_tf_->text());
  ContentSetting setting =
      cb_model_.setting_for_index(action_cb_->selected_item());
  delegate_->AcceptExceptionEdit(new_host, setting, index_, is_new());
  return true;
}

views::View* ExceptionEditorView::GetContentsView() {
  return this;
}

void ExceptionEditorView::ContentsChanged(views::Textfield* sender,
                                          const std::wstring& new_contents) {
  GetDialogClientView()->UpdateDialogButtons();
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

  action_cb_ = new views::Combobox(&cb_model_);
  if (!is_new())
    action_cb_->SetSelectedItem(cb_model_.index_for_setting(setting_));

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

  layout->StartRowWithPadding(0, 1, 0, kRelatedControlVerticalSpacing);
  layout->AddView(CreateLabel(IDS_EXCEPTION_EDITOR_ACTION_TITLE));
  layout->AddView(action_cb_);
}

views::Label* ExceptionEditorView::CreateLabel(int message_id) {
  views::Label* label = new views::Label(l10n_util::GetString(message_id));
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  return label;
}
