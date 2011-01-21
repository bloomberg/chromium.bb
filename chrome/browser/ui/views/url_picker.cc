// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/url_picker.h"

#include "base/stl_util-inl.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/possible_url_model.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/net_util.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/table_model.h"
#include "ui/base/resource/resource_bundle.h"
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
                     Profile* profile)
    : profile_(profile),
      delegate_(delegate) {
  DCHECK(profile_);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  url_table_model_.reset(new PossibleURLModel());

  ui::TableColumn col1(IDS_ASI_PAGE_COLUMN, ui::TableColumn::LEFT, -1, 50);
  col1.sortable = true;
  ui::TableColumn col2(IDS_ASI_URL_COLUMN, TableColumn::LEFT, -1, 50);
  col2.sortable = true;
  std::vector<ui::TableColumn> cols;
  cols.push_back(col1);
  cols.push_back(col2);

  url_table_ = new views::TableView(url_table_model_.get(), cols,
                                    views::ICON_AND_TEXT, true, true,
                                    true);
  url_table_->SetObserver(this);

  // Yummy layout code.
  GridLayout* layout = GridLayout::CreatePanel(this);
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

  layout->StartRow(0, labels_column_set_id);
  views::Label* url_label = new views::Label();
  url_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  url_label->SetText(UTF16ToWide(l10n_util::GetStringUTF16(IDS_ASI_URL)));
  layout->AddView(url_label);

  url_field_ = new views::Textfield();
  url_field_->SetController(this);
  layout->AddView(url_field_);

  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_view_set_id);
  views::Label* description_label = new views::Label();
  description_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  description_label->SetText(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_ASI_DESCRIPTION)));
  description_label->SetFont(
      description_label->font().DeriveFont(0, gfx::Font::BOLD));
  layout->AddView(description_label);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(1, single_column_view_set_id);
  layout->AddView(url_table_);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  AddAccelerator(views::Accelerator(ui::VKEY_RETURN, false, false, false));
}

UrlPicker::~UrlPicker() {
  url_table_->SetModel(NULL);
}

void UrlPicker::Show(HWND parent) {
  DCHECK(!window());
  views::Window::CreateChromeWindow(parent, gfx::Rect(), this)->Show();
  url_field_->SelectAll();
  url_field_->RequestFocus();
  url_table_model_->Reload(profile_);
}

void UrlPicker::Close() {
  DCHECK(window());
  window()->Close();
}

std::wstring UrlPicker::GetWindowTitle() const {
  return UTF16ToWide(l10n_util::GetStringUTF16(IDS_ASI_ADD_TITLE));
}

bool UrlPicker::IsModal() const {
  return true;
}

std::wstring UrlPicker::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK)
    return UTF16ToWide(l10n_util::GetStringUTF16(IDS_ASI_ADD));
  return std::wstring();
}

void UrlPicker::ContentsChanged(views::Textfield* sender,
                                const std::wstring& new_contents) {
  GetDialogClientView()->UpdateDialogButtons();
}

bool UrlPicker::Accept() {
  if (!IsDialogButtonEnabled(MessageBoxFlags::DIALOGBUTTON_OK)) {
    if (!GetInputURL().is_valid())
      url_field_->RequestFocus();
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
  delegate_->AddBookmark(this, std::wstring(), url);
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
    }
  }
  return true;
}

void UrlPicker::OnSelectionChanged() {
  int selection = url_table_->FirstSelectedRow();
  if (selection >= 0 && selection < url_table_model_->RowCount()) {
    std::string languages =
        profile_->GetPrefs()->GetString(prefs::kAcceptLanguages);
    // Because this gets parsed by FixupURL(), it's safe to omit the scheme or
    // trailing slash, and unescape most characters, but we need to not drop any
    // username/password, or unescape anything that changes the meaning.
    string16 formatted = net::FormatUrl(url_table_model_->GetURL(selection),
        languages,
        net::kFormatUrlOmitAll & ~net::kFormatUrlOmitUsernamePassword,
        UnescapeRule::SPACES, NULL, NULL, NULL);
    url_field_->SetText(UTF16ToWide(formatted));
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
  return URLFixerUpper::FixupURL(UTF16ToUTF8(url_field_->text()),
                                 std::string());
}
