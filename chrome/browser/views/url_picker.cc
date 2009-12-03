// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/url_picker.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/table_model.h"
#include "app/table_model_observer.h"
#include "base/keyboard_codes.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/possible_url_model.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/net_util.h"
#include "views/background.h"
#include "views/controls/label.h"
#include "views/controls/table/table_view.h"
#include "views/controls/textfield/textfield.h"
#include "views/focus/focus_manager.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/widget/widget.h"

using views::ColumnSet;
using views::GridLayout;

// Preferred width of the table.
static const int kTableWidth = 300;

UrlPickerDelegate::~UrlPickerDelegate() {}

////////////////////////////////////////////////////////////////////////////////
//
// UrlPicker implementation
//
////////////////////////////////////////////////////////////////////////////////
UrlPicker::UrlPicker(UrlPickerDelegate* delegate,
                     Profile* profile,
                     bool show_title)
    : profile_(profile),
      expected_title_handle_(0),
      delegate_(delegate) {
  DCHECK(profile_);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  url_table_model_.reset(new PossibleURLModel());

  TableColumn col1(IDS_ASI_PAGE_COLUMN, TableColumn::LEFT, -1,
                          50);
  col1.sortable = true;
  TableColumn col2(IDS_ASI_URL_COLUMN, TableColumn::LEFT, -1,
                          50);
  col2.sortable = true;
  std::vector<TableColumn> cols;
  cols.push_back(col1);
  cols.push_back(col2);

  url_table_ = new views::TableView(url_table_model_.get(), cols,
                                    views::ICON_AND_TEXT, true, true,
                                    true);
  url_table_->SetObserver(this);

  // Yummy layout code.
  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  const int labels_column_set_id = 0;
  const int single_column_view_set_id = 1;

  ColumnSet* column_set = layout->AddColumnSet(labels_column_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::FIXED, kTableWidth, 0);

  if (show_title) {
    layout->StartRow(0, labels_column_set_id);
    views::Label* title_label = new views::Label();
    title_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    title_label->SetText(l10n_util::GetString(IDS_ASI_TITLE_LABEL));
    layout->AddView(title_label);

    title_field_ = new views::Textfield();
    title_field_->SetController(this);
    layout->AddView(title_field_);

    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  } else {
    title_field_ = NULL;
  }

  layout->StartRow(0, labels_column_set_id);
  views::Label* url_label = new views::Label();
  url_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  url_label->SetText(l10n_util::GetString(IDS_ASI_URL));
  layout->AddView(url_label);

  url_field_ = new views::Textfield();
  url_field_->SetController(this);
  layout->AddView(url_field_);

  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_view_set_id);
  views::Label* description_label = new views::Label();
  description_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  description_label->SetText(l10n_util::GetString(IDS_ASI_DESCRIPTION));
  description_label->SetFont(
      description_label->GetFont().DeriveFont(0, gfx::Font::BOLD));
  layout->AddView(description_label);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(1, single_column_view_set_id);
  layout->AddView(url_table_);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  AddAccelerator(views::Accelerator(base::VKEY_RETURN, false, false, false));
}

UrlPicker::~UrlPicker() {
  url_table_->SetModel(NULL);
}

void UrlPicker::Show(HWND parent) {
  DCHECK(!window());
  views::Window::CreateChromeWindow(parent, gfx::Rect(), this)->Show();
  if (title_field_) {
    title_field_->SetText(l10n_util::GetString(IDS_ASI_DEFAULT_TITLE));
    title_field_->SelectAll();
    title_field_->RequestFocus();
  } else {
    url_field_->SelectAll();
    url_field_->RequestFocus();
  }
  url_table_model_->Reload(profile_);
}

void UrlPicker::Close() {
  DCHECK(window());
  window()->Close();
}

std::wstring UrlPicker::GetWindowTitle() const {
  return l10n_util::GetString(IDS_ASI_ADD_TITLE);
}

bool UrlPicker::IsModal() const {
  return true;
}

std::wstring UrlPicker::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK)
    return l10n_util::GetString(IDS_ASI_ADD);
  return std::wstring();
}

void UrlPicker::OnURLInfoAvailable(
    HistoryService::Handle handle,
    bool success,
    const history::URLRow* info,
    history::VisitVector* unused) {
  if (handle != expected_title_handle_)
    return;

  std::wstring s;
  if (success)
    s = info->title();
  if (s.empty())
    s = l10n_util::GetString(IDS_ASI_DEFAULT_TITLE);

  if (title_field_) {
    // expected_title_handle_ is reset if the title Textfield is edited so we
    // can safely set the value.
    title_field_->SetText(s);
    title_field_->SelectAll();
  }
  expected_title_handle_ = 0;
}

void UrlPicker::InitiateTitleAutoFill(const GURL& url) {
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (!hs)
    return;

  if (expected_title_handle_)
    hs->CancelRequest(expected_title_handle_);

  expected_title_handle_ = hs->QueryURL(url, false, &history_consumer_,
      NewCallback(this, &UrlPicker::OnURLInfoAvailable));
}

void UrlPicker::ContentsChanged(views::Textfield* sender,
                                const std::wstring& new_contents) {
  // If the user has edited the title field we no longer want to autofill it
  // so we reset the expected handle to an impossible value.
  if (sender == title_field_)
    expected_title_handle_ = 0;
  GetDialogClientView()->UpdateDialogButtons();
}

bool UrlPicker::Accept() {
  if (!IsDialogButtonEnabled(MessageBoxFlags::DIALOGBUTTON_OK)) {
    if (!GetInputURL().is_valid())
      url_field_->RequestFocus();
    else if (title_field_)
      title_field_->RequestFocus();
    return false;
  }
  PerformModelChange();
  return true;
}

int UrlPicker::GetDefaultDialogButton() const {
  // Don't set a default button, this view already has an accelerator for the
  // enter key.
  return MessageBoxFlags::DIALOGBUTTON_NONE;
}

bool UrlPicker::IsDialogButtonEnabled(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK)
    return GetInputURL().is_valid();
  return true;
}

views::View* UrlPicker::GetContentsView() {
  return this;
}

void UrlPicker::PerformModelChange() {
  DCHECK(delegate_);
  GURL url(GetInputURL());
  const std::wstring title =
      title_field_ ? title_field_->text() : std::wstring();
  delegate_->AddBookmark(this, title, url);
}

gfx::Size UrlPicker::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_URLPICKER_DIALOG_WIDTH_CHARS,
      IDS_URLPICKER_DIALOG_HEIGHT_LINES));
}

bool UrlPicker::AcceleratorPressed(
    const views::Accelerator& accelerator) {
  if (accelerator.GetKeyCode() == VK_ESCAPE) {
    window()->Close();
  } else if (accelerator.GetKeyCode() == VK_RETURN) {
    views::FocusManager* fm = GetFocusManager();
    if (fm->GetFocusedView() == url_table_) {
      // Return on table behaves like a double click.
      OnDoubleClick();
    } else if (fm->GetFocusedView()== url_field_) {
      // Return on the url field accepts the input if url is valid. If the URL
      // is invalid, focus is left on the url field.
      if (GetInputURL().is_valid()) {
        PerformModelChange();
        if (window())
          window()->Close();
      } else {
        url_field_->SelectAll();
      }
    } else if (title_field_ && fm->GetFocusedView() == title_field_) {
      url_field_->SelectAll();
      url_field_->RequestFocus();
    }
  }
  return true;
}

void UrlPicker::OnSelectionChanged() {
  int selection = url_table_->FirstSelectedRow();
  if (selection >= 0 && selection < url_table_model_->RowCount()) {
    std::wstring languages =
        profile_->GetPrefs()->GetString(prefs::kAcceptLanguages);
    // Because the url_field_ is user-editable, we set the URL with
    // username:password and escaped path and query.
    std::wstring formatted = net::FormatUrl(url_table_model_->GetURL(selection),
        languages, false, UnescapeRule::NONE, NULL, NULL, NULL);
    url_field_->SetText(formatted);
    if (title_field_)
      title_field_->SetText(url_table_model_->GetTitle(selection));
    GetDialogClientView()->UpdateDialogButtons();
  }
}

void UrlPicker::OnDoubleClick() {
  int selection = url_table_->FirstSelectedRow();
  if (selection >= 0 && selection < url_table_model_->RowCount()) {
    OnSelectionChanged();
    PerformModelChange();
    if (window())
      window()->Close();
  }
}

GURL UrlPicker::GetInputURL() const {
  return GURL(URLFixerUpper::FixupURL(UTF16ToUTF8(url_field_->text()), ""));
}
