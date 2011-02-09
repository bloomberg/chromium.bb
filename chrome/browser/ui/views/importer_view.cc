// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/importer_view.h"

#include "base/compiler_specific.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/importer/importer_list.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/label.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

using views::ColumnSet;
using views::GridLayout;

namespace browser {

// Declared in browser_dialogs.h so caller's don't have to depend on our header.
void ShowImporterView(views::Widget* parent,
                      Profile* profile) {
  views::Window::CreateChromeWindow(parent->GetNativeView(), gfx::Rect(),
      new ImporterView(profile, importer::ALL))->Show();
}

}  // namespace browser

ImporterView::ImporterView(Profile* profile, int initial_state)
    : import_from_label_(NULL),
      profile_combobox_(NULL),
      import_items_label_(NULL),
      history_checkbox_(NULL),
      favorites_checkbox_(NULL),
      passwords_checkbox_(NULL),
      search_engines_checkbox_(NULL),
      profile_(profile),
      importer_host_(new ImporterHost),
      importer_list_(new ImporterList),
      initial_state_(initial_state) {
  DCHECK(profile);
  importer_list_->DetectSourceProfiles(this);
  SetupControl();
}

ImporterView::~ImporterView() {
}

void ImporterView::SetupControl() {
  // Adds all controls.
  import_from_label_ = new views::Label(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_IMPORT_FROM_LABEL)));

  profile_combobox_ = new views::Combobox(this);
  profile_combobox_->set_listener(this);
  profile_combobox_->SetAccessibleName(
      WideToUTF16Hack(import_from_label_->GetText()));

  import_items_label_ = new views::Label(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_IMPORT_ITEMS_LABEL)));

  history_checkbox_ = InitCheckbox(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_IMPORT_HISTORY_CHKBOX)),
      (initial_state_ & importer::HISTORY) != 0);
  favorites_checkbox_ = InitCheckbox(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_IMPORT_FAVORITES_CHKBOX)),
      (initial_state_ & importer::FAVORITES) != 0);
  passwords_checkbox_ = InitCheckbox(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_IMPORT_PASSWORDS_CHKBOX)),
      (initial_state_ & importer::PASSWORDS) != 0);
  search_engines_checkbox_ = InitCheckbox(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_IMPORT_SEARCH_ENGINES_CHKBOX)),
      (initial_state_ & importer::SEARCH_ENGINES) != 0);

  // Arranges controls by using GridLayout.
  const int column_set_id = 0;
  GridLayout* layout = GridLayout::CreatePanel(this);
  SetLayoutManager(layout);
  ColumnSet* column_set = layout->AddColumnSet(column_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::FIXED, 200, 0);

  layout->StartRow(0, column_set_id);
  layout->AddView(import_from_label_);
  layout->AddView(profile_combobox_);

  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);
  layout->StartRow(0, column_set_id);
  layout->AddView(import_items_label_, 3, 1);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, column_set_id);
  layout->AddView(favorites_checkbox_, 3, 1);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, column_set_id);
  layout->AddView(search_engines_checkbox_, 3, 1);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, column_set_id);
  layout->AddView(passwords_checkbox_, 3, 1);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, column_set_id);
  layout->AddView(history_checkbox_, 3, 1);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
}

gfx::Size ImporterView::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_IMPORT_DIALOG_WIDTH_CHARS,
      IDS_IMPORT_DIALOG_HEIGHT_LINES));
}

void ImporterView::Layout() {
  GetLayoutManager()->Layout(this);
}

std::wstring ImporterView::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK) {
    return UTF16ToWide(l10n_util::GetStringUTF16(IDS_IMPORT_COMMIT));
  } else {
    return std::wstring();
  }
}

bool ImporterView::IsDialogButtonEnabled(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK) {
    return history_checkbox_->checked() ||
           favorites_checkbox_->checked() ||
           passwords_checkbox_->checked() ||
           search_engines_checkbox_->checked();
  }

  return true;
}

bool ImporterView::IsModal() const {
  return true;
}

std::wstring ImporterView::GetWindowTitle() const {
  return UTF16ToWide(l10n_util::GetStringUTF16(IDS_IMPORT_SETTINGS_TITLE));
}

bool ImporterView::Accept() {
  if (!IsDialogButtonEnabled(MessageBoxFlags::DIALOGBUTTON_OK))
    return false;

  uint16 items = GetCheckedItems();

  int selected_index = profile_combobox_->selected_item();
  StartImportingWithUI(GetWidget()->GetNativeView(), items,
                       importer_host_.get(),
                       importer_list_->GetSourceProfileInfoAt(selected_index),
                       profile_, this, false);
  // We return false here to prevent the window from being closed. We will be
  // notified back by our implementation of ImportObserver when the import is
  // complete so that we can close ourselves.
  return false;
}

views::View* ImporterView::GetContentsView() {
  return this;
}

void ImporterView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  // When no checkbox is checked we should disable the "Import" button.
  // This forces the button to evaluate what state they should be in.
  GetDialogClientView()->UpdateDialogButtons();
}

int ImporterView::GetItemCount() {
  DCHECK(importer_host_.get());
  return checkbox_items_.size();
}

string16 ImporterView::GetItemAt(int index) {
  DCHECK(importer_host_.get());

  if (!importer_list_->source_profiles_loaded())
    return l10n_util::GetStringUTF16(IDS_IMPORT_LOADING_PROFILES);
  else
    return WideToUTF16Hack(importer_list_->GetSourceProfileNameAt(index));
}

void ImporterView::ItemChanged(views::Combobox* combobox,
                               int prev_index, int new_index) {
  DCHECK(combobox);
  DCHECK(checkbox_items_.size() >=
      static_cast<size_t>(importer_list_->GetAvailableProfileCount()));

  if (prev_index == new_index)
    return;

  if (!importer_list_->source_profiles_loaded()) {
    SetCheckedItemsState(0);
    return;
  }

  // Save the current state
  uint16 prev_items = GetCheckedItems();
  checkbox_items_[prev_index] = prev_items;

  // Enable/Disable the checkboxes for this Item
  uint16 new_enabled_items = importer_list_->GetSourceProfileInfoAt(
      new_index).services_supported;
  SetCheckedItemsState(new_enabled_items);

  // Set the checked items for this Item
  uint16 new_items = checkbox_items_[new_index];
  SetCheckedItems(new_items);
}

void ImporterView::SourceProfilesLoaded() {
  DCHECK(importer_list_->source_profiles_loaded());
  checkbox_items_.resize(
      importer_list_->GetAvailableProfileCount(), initial_state_);

  // Reload the profile combobox.
  profile_combobox_->ModelChanged();
}

void ImporterView::ImportCanceled() {
  ImportComplete();
}

void ImporterView::ImportComplete() {
  // Now close this window since the import completed or was canceled.
  window()->Close();
}

views::Checkbox* ImporterView::InitCheckbox(const std::wstring& text,
                                            bool checked) {
  views::Checkbox* checkbox = new views::Checkbox(text);
  checkbox->SetChecked(checked);
  checkbox->set_listener(this);
  return checkbox;
}

uint16 ImporterView::GetCheckedItems() {
  uint16 items = importer::NONE;
  if (history_checkbox_->IsEnabled() && history_checkbox_->checked())
    items |= importer::HISTORY;
  if (favorites_checkbox_->IsEnabled() && favorites_checkbox_->checked())
    items |= importer::FAVORITES;
  if (passwords_checkbox_->IsEnabled() && passwords_checkbox_->checked())
    items |= importer::PASSWORDS;
  if (search_engines_checkbox_->IsEnabled() &&
      search_engines_checkbox_->checked())
    items |= importer::SEARCH_ENGINES;
  return items;
}

void ImporterView::SetCheckedItemsState(uint16 items) {
  if (items & importer::HISTORY) {
    history_checkbox_->SetEnabled(true);
  } else {
    history_checkbox_->SetEnabled(false);
    history_checkbox_->SetChecked(false);
  }
  if (items & importer::FAVORITES) {
    favorites_checkbox_->SetEnabled(true);
  } else {
    favorites_checkbox_->SetEnabled(false);
    favorites_checkbox_->SetChecked(false);
  }
  if (items & importer::PASSWORDS) {
    passwords_checkbox_->SetEnabled(true);
  } else {
    passwords_checkbox_->SetEnabled(false);
    passwords_checkbox_->SetChecked(false);
  }
  if (items & importer::SEARCH_ENGINES) {
    search_engines_checkbox_->SetEnabled(true);
  } else {
    search_engines_checkbox_->SetEnabled(false);
    search_engines_checkbox_->SetChecked(false);
  }
}

void ImporterView::SetCheckedItems(uint16 items) {
  if (history_checkbox_->IsEnabled())
    history_checkbox_->SetChecked(!!(items & importer::HISTORY));

  if (favorites_checkbox_->IsEnabled())
    favorites_checkbox_->SetChecked(!!(items & importer::FAVORITES));

  if (passwords_checkbox_->IsEnabled())
    passwords_checkbox_->SetChecked(!!(items & importer::PASSWORDS));

  if (search_engines_checkbox_->IsEnabled())
    search_engines_checkbox_->SetChecked(!!(items & importer::SEARCH_ENGINES));
}
