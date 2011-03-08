// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/options/passwords_page_view.h"

#include "base/i18n/rtl.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_store.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/background.h"
#include "views/controls/button/native_button.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"

using views::ColumnSet;
using views::GridLayout;
using webkit_glue::PasswordForm;

///////////////////////////////////////////////////////////////////////////////
// MultiLabelButtons
MultiLabelButtons::MultiLabelButtons(views::ButtonListener* listener,
                                     const std::wstring& label,
                                     const std::wstring& alt_label)
    : NativeButton(listener, label),
      label_(label),
      alt_label_(alt_label) {
}

gfx::Size MultiLabelButtons::GetPreferredSize() {
  if (!IsVisible())
    return gfx::Size();

  if (pref_size_.IsEmpty()) {
    // Let's compute our preferred size.
    std::wstring current_label = label();
    SetLabel(label_);
    pref_size_ = NativeButton::GetPreferredSize();
    SetLabel(alt_label_);
    gfx::Size alt_pref_size = NativeButton::GetPreferredSize();
    // Revert to the original label.
    SetLabel(current_label);
    pref_size_.SetSize(std::max(pref_size_.width(), alt_pref_size.width()),
                       std::max(pref_size_.height(), alt_pref_size.height()));
  }
  return gfx::Size(pref_size_.width(), pref_size_.height());
}

///////////////////////////////////////////////////////////////////////////////
// PasswordsTableModel, public
PasswordsTableModel::PasswordsTableModel(Profile* profile)
    : observer_(NULL),
      row_count_observer_(NULL),
      pending_login_query_(NULL),
      saved_signons_cleanup_(&saved_signons_),
      profile_(profile) {
  DCHECK(profile && profile->GetPasswordStore(Profile::EXPLICIT_ACCESS));
}

PasswordsTableModel::~PasswordsTableModel() {
  CancelLoginsQuery();
}

int PasswordsTableModel::RowCount() {
  return static_cast<int>(saved_signons_.size());
}

string16 PasswordsTableModel::GetText(int row,
                                      int col_id) {
  switch (col_id) {
    case IDS_PASSWORDS_PAGE_VIEW_SITE_COLUMN: {  // Site.
      // Force URL to have LTR directionality.
      std::wstring url(saved_signons_[row]->display_url.display_url());
      return base::i18n::GetDisplayStringInLTRDirectionality(
          WideToUTF16Hack(url));
    }
    case IDS_PASSWORDS_PAGE_VIEW_USERNAME_COLUMN: {  // Username.
      std::wstring username = GetPasswordFormAt(row)->username_value;
      base::i18n::AdjustStringForLocaleDirection(&username);
      return WideToUTF16Hack(username);
    }
    default:
      NOTREACHED() << "Invalid column.";
      return string16();
  }
}

int PasswordsTableModel::CompareValues(int row1, int row2,
                                       int column_id) {
  if (column_id == IDS_PASSWORDS_PAGE_VIEW_SITE_COLUMN) {
    return saved_signons_[row1]->display_url.Compare(
        saved_signons_[row2]->display_url, GetCollator());
  }
  return TableModel::CompareValues(row1, row2, column_id);
}

void PasswordsTableModel::SetObserver(ui::TableModelObserver* observer) {
  observer_ = observer;
}

void PasswordsTableModel::GetAllSavedLoginsForProfile() {
  DCHECK(!pending_login_query_);
  pending_login_query_ = password_store()->GetAutofillableLogins(this);
}

void PasswordsTableModel::OnPasswordStoreRequestDone(
    int handle, const std::vector<PasswordForm*>& result) {
  DCHECK_EQ(pending_login_query_, handle);
  pending_login_query_ = NULL;

  STLDeleteElements<PasswordRows>(&saved_signons_);
  saved_signons_.resize(result.size(), NULL);
  std::string languages = profile_->GetPrefs()->GetString(
      prefs::kAcceptLanguages);
  for (size_t i = 0; i < result.size(); ++i) {
    saved_signons_[i] = new PasswordRow(
        ui::SortedDisplayURL(result[i]->origin, languages), result[i]);
  }
  if (observer_)
    observer_->OnModelChanged();
  if (row_count_observer_)
    row_count_observer_->OnRowCountChanged(RowCount());
}

PasswordForm* PasswordsTableModel::GetPasswordFormAt(int row) {
  DCHECK(row >= 0 && row < RowCount());
  return saved_signons_[row]->form.get();
}

void PasswordsTableModel::ForgetAndRemoveSignon(int row) {
  DCHECK(row >= 0 && row < RowCount());
  PasswordRows::iterator target_iter = saved_signons_.begin() + row;
  // Remove from DB, memory, and vector.
  PasswordRow* password_row = *target_iter;
  password_store()->RemoveLogin(*(password_row->form.get()));
  delete password_row;
  saved_signons_.erase(target_iter);
  if (observer_)
    observer_->OnItemsRemoved(row, 1);
  if (row_count_observer_)
    row_count_observer_->OnRowCountChanged(RowCount());
}

void PasswordsTableModel::ForgetAndRemoveAllSignons() {
  PasswordRows::iterator iter = saved_signons_.begin();
  while (iter != saved_signons_.end()) {
    // Remove from DB, memory, and vector.
    PasswordRow* row = *iter;
    password_store()->RemoveLogin(*(row->form.get()));
    delete row;
    iter = saved_signons_.erase(iter);
  }
  if (observer_)
    observer_->OnModelChanged();
  if (row_count_observer_)
    row_count_observer_->OnRowCountChanged(RowCount());
}

///////////////////////////////////////////////////////////////////////////////
// PasswordsTableModel, private
void PasswordsTableModel::CancelLoginsQuery() {
  if (pending_login_query_) {
    password_store()->CancelLoginsQuery(pending_login_query_);
    pending_login_query_ = NULL;
  }
}

///////////////////////////////////////////////////////////////////////////////
// PasswordsPageView, public
PasswordsPageView::PasswordsPageView(Profile* profile)
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
      table_view_(NULL),
      current_selected_password_(NULL) {
  allow_show_passwords_.Init(prefs::kPasswordManagerAllowShowPasswords,
                             profile->GetPrefs(),
                             this);
}

PasswordsPageView::~PasswordsPageView() {
  // The model is going away, prevent the table from accessing it.
  if (table_view_)
    table_view_->SetModel(NULL);
}

void PasswordsPageView::OnSelectionChanged() {
  bool has_selection = table_view_->SelectedRowCount() > 0;
  remove_button_.SetEnabled(has_selection);

  PasswordForm* selected = NULL;
  if (has_selection) {
    views::TableSelectionIterator iter = table_view_->SelectionBegin();
    selected = table_model_.GetPasswordFormAt(*iter);
    DCHECK(++iter == table_view_->SelectionEnd());
  }

  if (selected != current_selected_password_) {
    // Reset the password related views.
    show_button_.SetLabel(UTF16ToWide(
        l10n_util::GetStringUTF16(IDS_PASSWORDS_PAGE_VIEW_SHOW_BUTTON)));
    show_button_.SetEnabled(has_selection);
    password_label_.SetText(std::wstring());

    current_selected_password_ = selected;
  }
}

void PasswordsPageView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  // Close will result in our destruction.
  if (sender == &remove_all_button_) {
    ConfirmMessageBoxDialog::Run(
        GetWindow()->GetNativeWindow(),
        this,
        UTF16ToWide(l10n_util::GetStringUTF16(
            IDS_PASSWORDS_PAGE_VIEW_TEXT_DELETE_ALL_PASSWORDS)),
        UTF16ToWide(l10n_util::GetStringUTF16(
            IDS_PASSWORDS_PAGE_VIEW_CAPTION_DELETE_ALL_PASSWORDS)));
    return;
  }

  // The following require a selection (and only one, since table is single-
  // select only).
  views::TableSelectionIterator iter = table_view_->SelectionBegin();
  int row = *iter;
  PasswordForm* selected = table_model_.GetPasswordFormAt(row);
  DCHECK(++iter == table_view_->SelectionEnd());

  if (sender == &remove_button_) {
    table_model_.ForgetAndRemoveSignon(row);
  } else if (sender == &show_button_) {
    if (password_label_.GetText().length() == 0 &&
        allow_show_passwords_.GetValue()) {
      password_label_.SetText(selected->password_value);
      show_button_.SetLabel(UTF16ToWide(
          l10n_util::GetStringUTF16(IDS_PASSWORDS_PAGE_VIEW_HIDE_BUTTON)));
    } else {
      HidePassword();
    }
  } else {
    NOTREACHED() << "Invalid button.";
  }
}

void PasswordsPageView::OnRowCountChanged(size_t rows) {
  remove_all_button_.SetEnabled(rows > 0);
}

void PasswordsPageView::OnConfirmMessageAccept() {
  table_model_.ForgetAndRemoveAllSignons();
}

///////////////////////////////////////////////////////////////////////////////
// PasswordsPageView, protected
void PasswordsPageView::InitControlLayout() {
  SetupButtonsAndLabels();
  SetupTable();

  // Do the layout thing.
  const int top_column_set_id = 0;
  GridLayout* layout = GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  // Design the grid.
  ColumnSet* column_set = layout->AddColumnSet(top_column_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  // Fill the grid.
  layout->StartRow(0, top_column_set_id);
  layout->AddView(table_view_, 1, 8, GridLayout::FILL,
                  GridLayout::FILL);
  layout->AddView(&remove_button_);
  layout->StartRowWithPadding(0, top_column_set_id, 0,
                              views::kRelatedControlVerticalSpacing);
  layout->SkipColumns(1);
  layout->AddView(&remove_all_button_);
  layout->StartRowWithPadding(0, top_column_set_id, 0,
                              views::kRelatedControlVerticalSpacing);
  layout->SkipColumns(1);
  layout->AddView(&show_button_);
  layout->StartRowWithPadding(0, top_column_set_id, 0,
                              views::kRelatedControlVerticalSpacing);
  layout->SkipColumns(1);
  layout->AddView(&password_label_);
  layout->AddPaddingRow(1, 0);

  // Ask the database for saved password data.
  table_model_.GetAllSavedLoginsForProfile();
}

///////////////////////////////////////////////////////////////////////////////
// PasswordsPageView, private
void PasswordsPageView::SetupButtonsAndLabels() {
  // Disable all buttons in the first place.
  show_button_.set_parent_owned(false);
  show_button_.SetEnabled(false);

  remove_button_.set_parent_owned(false);
  remove_button_.SetEnabled(false);

  remove_all_button_.set_parent_owned(false);
  remove_all_button_.SetEnabled(false);

  password_label_.set_parent_owned(false);
}

void PasswordsPageView::SetupTable() {
  // Tell the table model we are concern about how many rows it has.
  table_model_.set_row_count_observer(this);

  // Creates the different columns for the table.
  // The float resize values are the result of much tinkering.
  std::vector<ui::TableColumn> columns;
  columns.push_back(ui::TableColumn(IDS_PASSWORDS_PAGE_VIEW_SITE_COLUMN,
                                    ui::TableColumn::LEFT, -1, 0.55f));
  columns.back().sortable = true;
  columns.push_back(ui::TableColumn(
      IDS_PASSWORDS_PAGE_VIEW_USERNAME_COLUMN, ui::TableColumn::LEFT,
      -1, 0.37f));
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

void PasswordsPageView::HidePassword() {
  password_label_.SetText(L"");
  show_button_.SetLabel(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_PASSWORDS_PAGE_VIEW_SHOW_BUTTON)));
}

void PasswordsPageView::NotifyPrefChanged(const std::string* pref_name) {
  if (!pref_name || *pref_name == prefs::kPasswordManagerAllowShowPasswords) {
    bool show = allow_show_passwords_.GetValue();
    if (!show)
      HidePassword();
    show_button_.SetVisible(show);
    password_label_.SetVisible(show);
    // Update the layout (it may depend on the button size).
    show_button_.InvalidateLayout();
    Layout();
  }
}
