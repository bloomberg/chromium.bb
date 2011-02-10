// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/clear_browsing_data_view.h"

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/url_request/url_request_context.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/insets.h"
#include "views/background.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/label.h"
#include "views/controls/separator.h"
#include "views/controls/throbber.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"
#include "views/widget/widget.h"
#include "views/window/dialog_client_view.h"
#include "views/window/window.h"

using views::GridLayout;

// The combo box is vertically aligned to the 'time-period' label, which makes
// the combo box look a little too close to the check box above it when we use
// standard layout to separate them. We therefore add a little extra margin to
// the label, giving it a little breathing space.
static const int kExtraMarginForTimePeriodLabel = 3;

////////////////////////////////////////////////////////////////////////////////
// ClearBrowsingDataView2, public:

ClearBrowsingDataView2::ClearBrowsingDataView2(Profile* profile,
    ClearDataView* clear_data_view)
    : clear_data_parent_window_(clear_data_view),
      allow_clear_(true),
      throbber_view_(NULL),
      throbber_(NULL),
      status_label_(NULL),
      delete_all_label_(NULL),
      del_history_checkbox_(NULL),
      del_downloads_checkbox_(NULL),
      del_cache_checkbox_(NULL),
      del_cookies_checkbox_(NULL),
      del_passwords_checkbox_(NULL),
      del_form_data_checkbox_(NULL),
      time_period_label_(NULL),
      time_period_combobox_(NULL),
      clear_browsing_data_button_(NULL),
      delete_in_progress_(false),
      profile_(profile),
      remover_(NULL) {
  DCHECK(profile);
  Init();
  InitControlLayout();
}

ClearBrowsingDataView2::~ClearBrowsingDataView2(void) {
  if (remover_) {
    // We were destroyed while clearing history was in progress. This can only
    // occur during automated tests (normally the user can't close the dialog
    // while clearing is in progress as the dialog is modal and not closeable).
    remover_->RemoveObserver(this);
  }
}

void ClearBrowsingDataView2::SetAllowClear(bool allow) {
  allow_clear_ = allow;
  UpdateControlEnabledState();
}

void ClearBrowsingDataView2::Init() {
  throbber_ = new views::Throbber(50, true);
  throbber_->SetVisible(false);

  status_label_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_CLEAR_DATA_DELETING)));
  status_label_->SetVisible(false);

  // Regular view controls we draw by ourself. First, we add the dialog label.
  delete_all_label_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_CLEAR_BROWSING_DATA_LABEL)));

  // Add all the check-boxes.
  del_history_checkbox_ = AddCheckbox(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_DEL_BROWSING_HISTORY_CHKBOX)),
      profile_->GetPrefs()->GetBoolean(prefs::kDeleteBrowsingHistory));

  del_downloads_checkbox_ = AddCheckbox(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_DEL_DOWNLOAD_HISTORY_CHKBOX)),
      profile_->GetPrefs()->GetBoolean(prefs::kDeleteDownloadHistory));

  del_cache_checkbox_ = AddCheckbox(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_DEL_CACHE_CHKBOX)),
      profile_->GetPrefs()->GetBoolean(prefs::kDeleteCache));

  del_cookies_checkbox_ = AddCheckbox(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_DEL_COOKIES_CHKBOX)),
      profile_->GetPrefs()->GetBoolean(prefs::kDeleteCookies));

  del_passwords_checkbox_ = AddCheckbox(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_DEL_PASSWORDS_CHKBOX)),
      profile_->GetPrefs()->GetBoolean(prefs::kDeletePasswords));

  del_form_data_checkbox_ = AddCheckbox(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_DEL_FORM_DATA_CHKBOX)),
      profile_->GetPrefs()->GetBoolean(prefs::kDeleteFormData));

  clear_browsing_data_button_ = new views::NativeButton(
      this,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_CLEAR_BROWSING_DATA_COMMIT)));

  // Add a label which appears before the combo box for the time period.
  time_period_label_ = new views::Label(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_CLEAR_BROWSING_DATA_TIME_LABEL)));

  // Add the combo box showing how far back in time we want to delete.
  time_period_combobox_ = new views::Combobox(this);
  time_period_combobox_->SetSelectedItem(profile_->GetPrefs()->GetInteger(
                                         prefs::kDeleteTimePeriod));
  time_period_combobox_->set_listener(this);
  time_period_combobox_->SetAccessibleName(
      WideToUTF16Hack(time_period_label_->GetText()));

  // Create the throbber and related views. The throbber and status link are
  // contained in throbber_view_, which is positioned by DialogClientView right
  // next to the buttons.
  throbber_view_ = new views::View();

  GridLayout* layout = new GridLayout(throbber_view_);
  throbber_view_->SetLayoutManager(layout);
  views::ColumnSet* column_set = layout->AddColumnSet(0);

  // DialogClientView positions the extra view at kButtonHEdgeMargin, but we
  // put all our controls at kPanelHorizMargin. Add a padding column so things
  // line up nicely.
  if (kPanelHorizMargin - views::kButtonHEdgeMargin > 0) {
    column_set->AddPaddingColumn(
        0, kPanelHorizMargin - views::kButtonHEdgeMargin);
  }
  column_set->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  layout->StartRow(1, 0);
  layout->AddView(throbber_);
  layout->AddView(status_label_);
}

void ClearBrowsingDataView2::InitControlLayout() {
  GridLayout* layout = GridLayout::CreatePanel(this);
  this->SetLayoutManager(layout);

  int leading_column_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(leading_column_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  const int indented_column_set_id = 1;
  column_set = layout->AddColumnSet(indented_column_set_id);
  column_set->AddPaddingColumn(0, views::Checkbox::GetTextIndent());
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  const int two_column_set_id = 2;
  column_set = layout->AddColumnSet(two_column_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  const int fill_column_set_id = 3;
  column_set = layout->AddColumnSet(fill_column_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  // Delete All label goes to the top left corner.
  layout->StartRow(0, leading_column_set_id);
  layout->AddView(delete_all_label_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Check-boxes go beneath it (with a little indentation).
  layout->StartRow(0, indented_column_set_id);
  layout->AddView(del_history_checkbox_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  layout->StartRow(0, indented_column_set_id);
  layout->AddView(del_downloads_checkbox_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  layout->StartRow(0, indented_column_set_id);
  layout->AddView(del_cache_checkbox_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  layout->StartRow(0, indented_column_set_id);
  layout->AddView(del_cookies_checkbox_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  layout->StartRow(0, indented_column_set_id);
  layout->AddView(del_passwords_checkbox_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  layout->StartRow(0, indented_column_set_id);
  layout->AddView(del_form_data_checkbox_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Time period label is next below the combo boxes followed by time
  // period combo box
  layout->StartRow(0, two_column_set_id);
  layout->AddView(time_period_label_);
  time_period_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout->AddView(time_period_combobox_, 1, 1, views::GridLayout::LEADING,
                  views::GridLayout::CENTER);

  layout->AddPaddingRow(0, views::kUnrelatedControlLargeVerticalSpacing);

  // Left-align the throbber
  layout->StartRow(0, two_column_set_id);
  layout->AddView(throbber_view_, 1, 1,
                  GridLayout::LEADING, GridLayout::CENTER);

  // Right-align the clear button
  layout->AddView(clear_browsing_data_button_, 1, 1,
                  GridLayout::TRAILING, GridLayout::CENTER);
}

////////////////////////////////////////////////////////////////////////////////
// ClearBrowsingDataView2, ComboboxModel implementation:

int ClearBrowsingDataView2::GetItemCount() {
  return 5;
}

string16 ClearBrowsingDataView2::GetItemAt(int index) {
  switch (index) {
    case 0: return l10n_util::GetStringUTF16(IDS_CLEAR_DATA_HOUR);
    case 1: return l10n_util::GetStringUTF16(IDS_CLEAR_DATA_DAY);
    case 2: return l10n_util::GetStringUTF16(IDS_CLEAR_DATA_WEEK);
    case 3: return l10n_util::GetStringUTF16(IDS_CLEAR_DATA_4WEEKS);
    case 4: return l10n_util::GetStringUTF16(IDS_CLEAR_DATA_EVERYTHING);
    default: NOTREACHED() << "Missing item";
             return ASCIIToUTF16("?");
  }
}

////////////////////////////////////////////////////////////////////////////////
// ClearBrowsingDataView2, views::ComboBoxListener implementation:

void ClearBrowsingDataView2::ItemChanged(views::Combobox* sender,
                                        int prev_index, int new_index) {
  if (sender == time_period_combobox_ && prev_index != new_index)
    profile_->GetPrefs()->SetInteger(prefs::kDeleteTimePeriod, new_index);
}

////////////////////////////////////////////////////////////////////////////////
// ClearBrowsingDataView2, views::ButtonListener implementation:

void ClearBrowsingDataView2::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == clear_browsing_data_button_) {
    profile_->GetPrefs()->SetBoolean(prefs::kDeleteBrowsingHistory,
        del_history_checkbox_->checked() ? true : false);
    profile_->GetPrefs()->SetBoolean(prefs::kDeleteDownloadHistory,
        del_downloads_checkbox_->checked() ? true : false);
    profile_->GetPrefs()->SetBoolean(prefs::kDeleteCache,
        del_cache_checkbox_->checked() ? true : false);
    profile_->GetPrefs()->SetBoolean(prefs::kDeleteCookies,
        del_cookies_checkbox_->checked() ? true : false);
    profile_->GetPrefs()->SetBoolean(prefs::kDeletePasswords,
        del_passwords_checkbox_->checked() ? true : false);
    profile_->GetPrefs()->SetBoolean(prefs::kDeleteFormData,
        del_form_data_checkbox_->checked() ? true : false);
    clear_data_parent_window_->StartClearingBrowsingData();
    OnDelete();
  }

  // When no checkbox is checked we should not have the action button enabled.
  // This forces the button to evaluate what state they should be in.
  UpdateControlEnabledState();
}

void ClearBrowsingDataView2::LinkActivated(views::Link* source,
                                          int event_flags) {
  Browser* browser = Browser::Create(profile_);
  browser->OpenURL(GURL(l10n_util::GetStringUTF8(IDS_FLASH_STORAGE_URL)),
                   GURL(), NEW_FOREGROUND_TAB, PageTransition::LINK);
  browser->window()->Show();
}

////////////////////////////////////////////////////////////////////////////////
// ClearBrowsingDataView2, private:

views::Checkbox* ClearBrowsingDataView2::AddCheckbox(const std::wstring& text,
                                                    bool checked) {
  views::Checkbox* checkbox = new views::Checkbox(text);
  checkbox->SetChecked(checked);
  checkbox->set_listener(this);
  return checkbox;
}

void ClearBrowsingDataView2::UpdateControlEnabledState() {
  del_history_checkbox_->SetEnabled(!delete_in_progress_);
  del_downloads_checkbox_->SetEnabled(!delete_in_progress_);
  del_cache_checkbox_->SetEnabled(!delete_in_progress_);
  del_cookies_checkbox_->SetEnabled(!delete_in_progress_);
  del_passwords_checkbox_->SetEnabled(!delete_in_progress_);
  del_form_data_checkbox_->SetEnabled(!delete_in_progress_);
  time_period_combobox_->SetEnabled(!delete_in_progress_);

  status_label_->SetVisible(delete_in_progress_);
  throbber_->SetVisible(delete_in_progress_);
  if (delete_in_progress_)
    throbber_->Start();
  else
    throbber_->Stop();

  clear_browsing_data_button_->SetEnabled(
      allow_clear_ &&
      !delete_in_progress_ &&
          (del_history_checkbox_->checked() ||
           del_downloads_checkbox_->checked() ||
           del_cache_checkbox_->checked() ||
           del_cookies_checkbox_->checked() ||
           del_passwords_checkbox_->checked() ||
           del_form_data_checkbox_->checked()));
}

// Convenience method that returns true if the supplied checkbox is selected
// and enabled.
static bool IsCheckBoxEnabledAndSelected(views::Checkbox* cb) {
  return (cb->IsEnabled() && cb->checked());
}

void ClearBrowsingDataView2::OnDelete() {
  int period_selected = time_period_combobox_->selected_item();

  int remove_mask = 0;
  if (IsCheckBoxEnabledAndSelected(del_history_checkbox_))
    remove_mask |= BrowsingDataRemover::REMOVE_HISTORY;
  if (IsCheckBoxEnabledAndSelected(del_downloads_checkbox_))
    remove_mask |= BrowsingDataRemover::REMOVE_DOWNLOADS;
  if (IsCheckBoxEnabledAndSelected(del_cookies_checkbox_))
    remove_mask |= BrowsingDataRemover::REMOVE_COOKIES;
  if (IsCheckBoxEnabledAndSelected(del_passwords_checkbox_))
    remove_mask |= BrowsingDataRemover::REMOVE_PASSWORDS;
  if (IsCheckBoxEnabledAndSelected(del_form_data_checkbox_))
    remove_mask |= BrowsingDataRemover::REMOVE_FORM_DATA;
  if (IsCheckBoxEnabledAndSelected(del_cache_checkbox_))
    remove_mask |= BrowsingDataRemover::REMOVE_CACHE;

  delete_in_progress_ = true;
  UpdateControlEnabledState();

  // BrowsingDataRemover deletes itself when done.
  remover_ = new BrowsingDataRemover(profile_,
      static_cast<BrowsingDataRemover::TimePeriod>(period_selected),
      base::Time());
  remover_->AddObserver(this);
  remover_->Remove(remove_mask);
}

void ClearBrowsingDataView2::OnBrowsingDataRemoverDone() {
  clear_data_parent_window_->StopClearingBrowsingData();
}
