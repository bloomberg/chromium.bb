// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/clear_browsing_data.h"

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/ui/browser.h"
#if defined(OS_WIN)
#include "chrome/browser/ui/views/clear_browsing_data_view.h"
#endif
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

namespace browser {
// Defined in browser_dialogs.h for creation of the view.
void ShowClearBrowsingDataView(gfx::NativeWindow parent,
                               Profile* profile) {

  views::Window::CreateChromeWindow(parent, gfx::Rect(),
                                    new ClearBrowsingDataView(profile))->Show();
}

}  // namespace browser


////////////////////////////////////////////////////////////////////////////////
// ClearBrowsingDataView, public:

ClearBrowsingDataView::ClearBrowsingDataView(Profile* profile)
    : del_history_checkbox_(NULL),
      del_downloads_checkbox_(NULL),
      del_cache_checkbox_(NULL),
      del_cookies_checkbox_(NULL),
      del_passwords_checkbox_(NULL),
      del_form_data_checkbox_(NULL),
      time_period_label_(NULL),
      time_period_combobox_(NULL),
      delete_in_progress_(false),
      profile_(profile),
      remover_(NULL) {
  DCHECK(profile);
  Init();
}

ClearBrowsingDataView::~ClearBrowsingDataView(void) {
  if (remover_) {
    // We were destroyed while clearing history was in progress. This can only
    // occur during automated tests (normally the user can't close the dialog
    // while clearing is in progress as the dialog is modal and not closeable).
    remover_->RemoveObserver(this);
  }
}

void ClearBrowsingDataView::Init() {
  // Views we will add to the *parent* of this dialog, since it will display
  // next to the buttons which we don't draw ourselves.
  throbber_ = new views::Throbber(50, true);
  throbber_->SetVisible(false);

  status_label_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_CLEAR_DATA_DELETING)));
  status_label_->SetVisible(false);

  // Regular view controls we draw by ourself. First, we add the dialog label.
  delete_all_label_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_CLEAR_BROWSING_DATA_LABEL)));
  AddChildView(delete_all_label_);

  // Add all the check-boxes.
  del_history_checkbox_ =
      AddCheckbox(UTF16ToWide(
          l10n_util::GetStringUTF16(IDS_DEL_BROWSING_HISTORY_CHKBOX)),
      profile_->GetPrefs()->GetBoolean(prefs::kDeleteBrowsingHistory));

  del_downloads_checkbox_ =
      AddCheckbox(UTF16ToWide(
          l10n_util::GetStringUTF16(IDS_DEL_DOWNLOAD_HISTORY_CHKBOX)),
      profile_->GetPrefs()->GetBoolean(prefs::kDeleteDownloadHistory));

  del_cache_checkbox_ =
      AddCheckbox(UTF16ToWide(
          l10n_util::GetStringUTF16(IDS_DEL_CACHE_CHKBOX)),
      profile_->GetPrefs()->GetBoolean(prefs::kDeleteCache));

  del_cookies_checkbox_ =
      AddCheckbox(UTF16ToWide(
          l10n_util::GetStringUTF16(IDS_DEL_COOKIES_CHKBOX)),
      profile_->GetPrefs()->GetBoolean(prefs::kDeleteCookies));

  del_passwords_checkbox_ =
      AddCheckbox(UTF16ToWide(
          l10n_util::GetStringUTF16(IDS_DEL_PASSWORDS_CHKBOX)),
      profile_->GetPrefs()->GetBoolean(prefs::kDeletePasswords));

  del_form_data_checkbox_ =
      AddCheckbox(UTF16ToWide(
          l10n_util::GetStringUTF16(IDS_DEL_FORM_DATA_CHKBOX)),
      profile_->GetPrefs()->GetBoolean(prefs::kDeleteFormData));

  // Add a label which appears before the combo box for the time period.
  time_period_label_ = new views::Label(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_CLEAR_BROWSING_DATA_TIME_LABEL)));
  AddChildView(time_period_label_);

  // Add the combo box showing how far back in time we want to delete.
  time_period_combobox_ = new views::Combobox(this);
  time_period_combobox_->SetSelectedItem(profile_->GetPrefs()->GetInteger(
                                         prefs::kDeleteTimePeriod));
  time_period_combobox_->set_listener(this);
  time_period_combobox_->SetAccessibleName(
      WideToUTF16Hack(time_period_label_->GetText()));
  AddChildView(time_period_combobox_);

  // Create the throbber and related views. The throbber and status link are
  // contained in throbber_view_, which is positioned by DialogClientView right
  // next to the buttons.
  throbber_view_ = new views::View();

  GridLayout* layout = new GridLayout(throbber_view_);
  throbber_view_->SetLayoutManager(layout);
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  // DialogClientView positions the extra view at kButtonHEdgeMargin, but we
  // put all our controls at views::kPanelHorizMargin. Add a padding column so
  // things line up nicely.
  if (views::kPanelHorizMargin - views::kButtonHEdgeMargin > 0) {
    column_set->AddPaddingColumn(
        0, views::kPanelHorizMargin - views::kButtonHEdgeMargin);
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

////////////////////////////////////////////////////////////////////////////////
// ClearBrowsingDataView, views::View implementation:

gfx::Size ClearBrowsingDataView::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_CLEARDATA_DIALOG_WIDTH_CHARS,
      IDS_CLEARDATA_DIALOG_HEIGHT_LINES));
}

void ClearBrowsingDataView::Layout() {
  gfx::Size panel_size = GetPreferredSize();

  // Delete All label goes to the top left corner.
  gfx::Size sz = delete_all_label_->GetPreferredSize();
  delete_all_label_->SetBounds(
      views::kPanelHorizMargin, views::kPanelVertMargin,
      sz.width(), sz.height());

  // Check-boxes go beneath it (with a little indentation).
  sz = del_history_checkbox_->GetPreferredSize();
  del_history_checkbox_->SetBounds(2 * views::kPanelHorizMargin,
                                   delete_all_label_->y() +
                                       delete_all_label_->height() +
                                       views::kRelatedControlVerticalSpacing,
                                   sz.width(), sz.height());

  sz = del_downloads_checkbox_->GetPreferredSize();
  del_downloads_checkbox_->SetBounds(2 * views::kPanelHorizMargin,
                                     del_history_checkbox_->y() +
                                         del_history_checkbox_->height() +
                                         views::kRelatedControlVerticalSpacing,
                                     sz.width(), sz.height());

  sz = del_cache_checkbox_->GetPreferredSize();
  del_cache_checkbox_->SetBounds(2 * views::kPanelHorizMargin,
                                 del_downloads_checkbox_->y() +
                                     del_downloads_checkbox_->height() +
                                     views::kRelatedControlVerticalSpacing,
                                 sz.width(), sz.height());

  sz = del_cookies_checkbox_->GetPreferredSize();
  del_cookies_checkbox_->SetBounds(2 * views::kPanelHorizMargin,
                                   del_cache_checkbox_->y() +
                                       del_cache_checkbox_->height() +
                                       views::kRelatedControlVerticalSpacing,
                                   sz.width(), sz.height());

  sz = del_passwords_checkbox_->GetPreferredSize();
  del_passwords_checkbox_->SetBounds(2 * views::kPanelHorizMargin,
                                     del_cookies_checkbox_->y() +
                                         del_cookies_checkbox_->height() +
                                         views::kRelatedControlVerticalSpacing,
                                     sz.width(), sz.height());

  sz = del_form_data_checkbox_->GetPreferredSize();
  del_form_data_checkbox_->SetBounds(2 * views::kPanelHorizMargin,
                                     del_passwords_checkbox_->y() +
                                         del_passwords_checkbox_->height() +
                                         views::kRelatedControlVerticalSpacing,
                                     sz.width(), sz.height());

  // Time period label is next below the combo boxes.
  sz = time_period_label_->GetPreferredSize();
  time_period_label_->SetBounds(views::kPanelHorizMargin,
                                del_form_data_checkbox_->y() +
                                    del_form_data_checkbox_->height() +
                                    views::kRelatedControlVerticalSpacing +
                                    kExtraMarginForTimePeriodLabel,
                                sz.width(), sz.height());

  // Time period combo box goes on the right of the label, and we align it
  // vertically to the label as well.
  int label_y_size = sz.height();
  sz = time_period_combobox_->GetPreferredSize();
  time_period_combobox_->SetBounds(time_period_label_->x() +
                                       time_period_label_->width() +
                                       views::kRelatedControlVerticalSpacing,
                                   time_period_label_->y() -
                                       ((sz.height() - label_y_size) / 2),
                                   sz.width(), sz.height());
}

////////////////////////////////////////////////////////////////////////////////
// ClearBrowsingDataView, views::DialogDelegate implementation:

int ClearBrowsingDataView::GetDefaultDialogButton() const {
  return MessageBoxFlags::DIALOGBUTTON_NONE;
}

std::wstring ClearBrowsingDataView::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  DCHECK((button == MessageBoxFlags::DIALOGBUTTON_OK) ||
         (button == MessageBoxFlags::DIALOGBUTTON_CANCEL));
  return UTF16ToWide(l10n_util::GetStringUTF16(
      (button == MessageBoxFlags::DIALOGBUTTON_OK) ?
          IDS_CLEAR_BROWSING_DATA_COMMIT : IDS_CANCEL));
}

bool ClearBrowsingDataView::IsDialogButtonEnabled(
    MessageBoxFlags::DialogButton button) const {
  if (delete_in_progress_)
    return false;

  if (button == MessageBoxFlags::DIALOGBUTTON_OK) {
    return del_history_checkbox_->checked() ||
           del_downloads_checkbox_->checked() ||
           del_cache_checkbox_->checked() ||
           del_cookies_checkbox_->checked() ||
           del_passwords_checkbox_->checked() ||
           del_form_data_checkbox_->checked();
  }

  return true;
}

bool ClearBrowsingDataView::CanResize() const {
  return false;
}

bool ClearBrowsingDataView::CanMaximize() const {
  return false;
}

bool ClearBrowsingDataView::IsAlwaysOnTop() const {
  return false;
}

bool ClearBrowsingDataView::HasAlwaysOnTopMenu() const {
  return false;
}

bool ClearBrowsingDataView::IsModal() const {
  return true;
}

std::wstring ClearBrowsingDataView::GetWindowTitle() const {
  return UTF16ToWide(l10n_util::GetStringUTF16(IDS_CLEAR_BROWSING_DATA_TITLE));
}

bool ClearBrowsingDataView::Accept() {
  if (!IsDialogButtonEnabled(MessageBoxFlags::DIALOGBUTTON_OK)) {
    return false;
  }

  PrefService* prefs = profile_->GetPrefs();
  prefs->SetBoolean(prefs::kDeleteBrowsingHistory,
                    del_history_checkbox_->checked());
  prefs->SetBoolean(prefs::kDeleteDownloadHistory,
                    del_downloads_checkbox_->checked());
  prefs->SetBoolean(prefs::kDeleteCache,
                    del_cache_checkbox_->checked());
  prefs->SetBoolean(prefs::kDeleteCookies,
                    del_cookies_checkbox_->checked());
  prefs->SetBoolean(prefs::kDeletePasswords,
                    del_passwords_checkbox_->checked());
  prefs->SetBoolean(prefs::kDeleteFormData,
                    del_form_data_checkbox_->checked());
  OnDelete();
  return false;  // We close the dialog in OnBrowsingDataRemoverDone().
}

views::View* ClearBrowsingDataView::GetContentsView() {
  return this;
}

views::View* ClearBrowsingDataView::GetInitiallyFocusedView() {
  return GetDialogClientView()->cancel_button();
}

views::ClientView* ClearBrowsingDataView::CreateClientView(
    views::Window* window) {
  views::Link* flash_link = new views::Link(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_FLASH_STORAGE_SETTINGS)));
  flash_link->SetController(this);

  views::View* settings_view = new views::View();
  GridLayout* layout = new GridLayout(settings_view);
  layout->SetInsets(
      gfx::Insets(0, views::kPanelHorizMargin, 0, views::kButtonHEdgeMargin));
  settings_view->SetLayoutManager(layout);
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, 0);
  layout->AddView(new views::Separator());
  layout->StartRowWithPadding(0, 0, 0, views::kRelatedControlVerticalSpacing);
  layout->AddView(flash_link, 1, 1, GridLayout::LEADING, GridLayout::CENTER);

  views::DialogClientView* client_view =
      new views::DialogClientView(window, GetContentsView());
  client_view->SetBottomView(settings_view);
  return client_view;
}

views::View* ClearBrowsingDataView::GetExtraView() {
  return throbber_view_;
}

bool ClearBrowsingDataView::GetSizeExtraViewHeightToButtons() {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// ClearBrowsingDataView, ComboboxModel implementation:

int ClearBrowsingDataView::GetItemCount() {
  return 5;
}

string16 ClearBrowsingDataView::GetItemAt(int index) {
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
// ClearBrowsingDataView, views::ComboBoxListener implementation:

void ClearBrowsingDataView::ItemChanged(views::Combobox* sender,
                                        int prev_index, int new_index) {
  if (sender == time_period_combobox_ && prev_index != new_index)
    profile_->GetPrefs()->SetInteger(prefs::kDeleteTimePeriod, new_index);
}

////////////////////////////////////////////////////////////////////////////////
// ClearBrowsingDataView, views::ButtonListener implementation:

void ClearBrowsingDataView::ButtonPressed(views::Button* sender,
                                          const views::Event& event) {
  // When no checkbox is checked we should not have the action button enabled.
  // This forces the button to evaluate what state they should be in.
  GetDialogClientView()->UpdateDialogButtons();
}

void ClearBrowsingDataView::LinkActivated(views::Link* source,
                                          int event_flags) {
  Browser* browser = Browser::Create(profile_);
  browser->OpenURL(GURL(l10n_util::GetStringUTF8(IDS_FLASH_STORAGE_URL)),
                   GURL(), NEW_FOREGROUND_TAB, PageTransition::LINK);
  browser->window()->Show();
}

////////////////////////////////////////////////////////////////////////////////
// ClearBrowsingDataView, private:

views::Checkbox* ClearBrowsingDataView::AddCheckbox(const std::wstring& text,
                                                    bool checked) {
  views::Checkbox* checkbox = new views::Checkbox(text);
  checkbox->SetChecked(checked);
  checkbox->set_listener(this);
  AddChildView(checkbox);
  return checkbox;
}

void ClearBrowsingDataView::UpdateControlEnabledState() {
  window()->EnableClose(!delete_in_progress_);

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

  // Make sure to update the state for OK and Cancel buttons.
  GetDialogClientView()->UpdateDialogButtons();
}

// Convenience method that returns true if the supplied checkbox is selected
// and enabled.
static bool IsCheckBoxEnabledAndSelected(views::Checkbox* cb) {
  return (cb->IsEnabled() && cb->checked());
}

void ClearBrowsingDataView::OnDelete() {
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

void ClearBrowsingDataView::OnBrowsingDataRemoverDone() {
  // No need to remove ourselves as an observer as BrowsingDataRemover deletes
  // itself after we return.
  remover_ = NULL;
  window()->Close();
}
