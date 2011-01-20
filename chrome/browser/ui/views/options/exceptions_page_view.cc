// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/options/exceptions_page_view.h"

#include "app/l10n_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "ui/base/text/text_elider.h"
#include "views/background.h"
#include "views/controls/button/native_button.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"

using views::ColumnSet;
using views::GridLayout;
using webkit_glue::PasswordForm;

///////////////////////////////////////////////////////////////////////////////
// ExceptionsTableModel
ExceptionsTableModel::ExceptionsTableModel(Profile* profile)
    : PasswordsTableModel(profile) {
}

ExceptionsTableModel::~ExceptionsTableModel() {
}

string16 ExceptionsTableModel::GetText(int row, int col_id) {
  DCHECK_EQ(col_id, IDS_PASSWORDS_PAGE_VIEW_SITE_COLUMN);
  return PasswordsTableModel::GetText(row, col_id);
}

int ExceptionsTableModel::CompareValues(int row1, int row2,
                                                       int col_id) {
  DCHECK_EQ(col_id, IDS_PASSWORDS_PAGE_VIEW_SITE_COLUMN);
  return PasswordsTableModel::CompareValues(row1, row2, col_id);
}

void ExceptionsTableModel::GetAllExceptionsForProfile() {
  DCHECK(!pending_login_query_);
  pending_login_query_ = password_store()->GetBlacklistLogins(this);
}

void ExceptionsTableModel::OnPasswordStoreRequestDone(
    int handle, const std::vector<webkit_glue::PasswordForm*>& result) {
  DCHECK_EQ(pending_login_query_, handle);
  pending_login_query_ = NULL;

  STLDeleteElements<PasswordRows>(&saved_signons_);
  std::wstring languages =
      UTF8ToWide(profile_->GetPrefs()->GetString(prefs::kAcceptLanguages));
  for (size_t i = 0; i < result.size(); ++i) {
    saved_signons_.push_back(new PasswordRow(
        ui::SortedDisplayURL(result[i]->origin, languages), result[i]));
  }
  if (observer_)
    observer_->OnModelChanged();
  if (row_count_observer_)
    row_count_observer_->OnRowCountChanged(RowCount());
}

///////////////////////////////////////////////////////////////////////////////
// ExceptionsPageView, public
ExceptionsPageView::ExceptionsPageView(Profile* profile)
    : OptionsPageView(profile),
      ALLOW_THIS_IN_INITIALIZER_LIST(show_button_(
          this,
          UTF16ToWide(l10n_util::GetStringUTF16(
              IDS_PASSWORDS_PAGE_VIEW_SHOW_BUTTON)),
          UTF16ToWide(l10n_util::GetStringUTF16(
              IDS_PASSWORDS_PAGE_VIEW_HIDE_BUTTON)))),
      ALLOW_THIS_IN_INITIALIZER_LIST(remove_button_(
          this,
          UTF16ToWide(l10n_util::GetStringUTF16(
              IDS_PASSWORDS_PAGE_VIEW_REMOVE_BUTTON)))),
      ALLOW_THIS_IN_INITIALIZER_LIST(remove_all_button_(
          this,
          UTF16ToWide(l10n_util::GetStringUTF16(
              IDS_PASSWORDS_PAGE_VIEW_REMOVE_ALL_BUTTON)))),
      table_model_(profile),
      table_view_(NULL) {
}

ExceptionsPageView::~ExceptionsPageView() {
  // The model is going away, prevent the table from accessing it.
  if (table_view_)
    table_view_->SetModel(NULL);
}

void ExceptionsPageView::OnSelectionChanged() {
  bool has_selection = table_view_->SelectedRowCount() > 0;
  remove_button_.SetEnabled(has_selection);
}

void ExceptionsPageView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  // Close will result in our destruction.
  if (sender == &remove_all_button_) {
    table_model_.ForgetAndRemoveAllSignons();
    return;
  }

  // The following require a selection (and only one, since table is single-
  // select only).
  views::TableSelectionIterator iter = table_view_->SelectionBegin();
  int row = *iter;
  DCHECK(++iter == table_view_->SelectionEnd());

  if (sender == &remove_button_) {
    table_model_.ForgetAndRemoveSignon(row);
  } else {
    NOTREACHED() << "Invalid button.";
  }
}

void ExceptionsPageView::OnRowCountChanged(size_t rows) {
  remove_all_button_.SetEnabled(rows > 0);
}

///////////////////////////////////////////////////////////////////////////////
// ExceptionsPageView, protected
void ExceptionsPageView::InitControlLayout() {
  SetupButtons();
  SetupTable();

  // Do the layout thing.
  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  const int top_column_set_id = 0;

  // Design the grid.
  ColumnSet* column_set = layout->AddColumnSet(top_column_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  // Fill the grid.
  layout->StartRow(0, top_column_set_id);
  layout->AddView(table_view_, 1, 6, GridLayout::FILL,
                  GridLayout::FILL);
  layout->AddView(&remove_button_);
  layout->StartRowWithPadding(0, top_column_set_id, 0,
                              kRelatedControlVerticalSpacing);
  layout->SkipColumns(1);
  layout->AddView(&remove_all_button_);
  layout->StartRowWithPadding(0, top_column_set_id, 0,
                              kRelatedControlVerticalSpacing);

  layout->SkipColumns(1);
  layout->AddView(&show_button_);
  layout->AddPaddingRow(1, 0);

  // Ask the database for exception data.
  table_model_.GetAllExceptionsForProfile();
}

///////////////////////////////////////////////////////////////////////////////
// ExceptionsPageView, private
void ExceptionsPageView::SetupButtons() {
  // Disable all buttons in the first place.
  remove_button_.set_parent_owned(false);
  remove_button_.SetEnabled(false);

  remove_all_button_.set_parent_owned(false);
  remove_all_button_.SetEnabled(false);

  show_button_.set_parent_owned(false);
  show_button_.SetEnabled(false);
  show_button_.SetVisible(false);
}

void ExceptionsPageView::SetupTable() {
  // Tell the table model we are concerned about how many rows it has.
  table_model_.set_row_count_observer(this);

  // Creates the different columns for the table.
  // The float resize values are the result of much tinkering.
  std::vector<ui::TableColumn> columns;
  columns.push_back(ui::TableColumn(IDS_PASSWORDS_PAGE_VIEW_SITE_COLUMN,
                                    ui::TableColumn::LEFT, -1, 0.55f));
  columns.back().sortable = true;
  table_view_ = new views::TableView(&table_model_, columns, views::TEXT_ONLY,
                                     true, true, true);
  // Make the table initially sorted by host.
  views::TableView::SortDescriptors sort;
  sort.push_back(views::TableView::SortDescriptor(
      IDS_PASSWORDS_PAGE_VIEW_SITE_COLUMN, true));
  table_view_->SetSortDescriptors(sort);
  table_view_->SetObserver(this);
}
