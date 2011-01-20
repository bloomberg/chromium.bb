// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/clear_server_data.h"

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "gfx/insets.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/url_request/url_request_context.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/background.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/label.h"
#include "views/controls/separator.h"
#include "views/controls/throbber.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/widget/widget.h"
#include "views/window/dialog_client_view.h"
#include "views/window/window.h"

using views::GridLayout;
using views::ColumnSet;

// The combo box is vertically aligned to the 'time-period' label, which makes
// the combo box look a little too close to the check box above it when we use
// standard layout to separate them. We therefore add a little extra margin to
// the label, giving it a little breathing space.
static const int kExtraMarginForTimePeriodLabel = 3;

////////////////////////////////////////////////////////////////////////////////
// ClearServerDataView, public:

ClearServerDataView::ClearServerDataView(Profile* profile,
    ClearDataView* clear_data_view)
    : profile_(profile),
      clear_data_parent_window_(clear_data_view),
      allow_clear_(true) {
  DCHECK(profile);
  DCHECK(clear_data_view);

  // Always show preferences for the original profile. Most state when off
  // the record comes from the original profile, but we explicitly use
  // the original profile to avoid potential problems.
  profile_ = profile->GetOriginalProfile();
  sync_service_ = profile_->GetProfileSyncService();

  if (NULL != sync_service_) {
    sync_service_->ResetClearServerDataState();
    sync_service_->AddObserver(this);
  }

  Init();
  InitControlLayout();
  InitControlVisibility();
}

ClearServerDataView::~ClearServerDataView(void) {
  if (NULL != sync_service_) {
    sync_service_->RemoveObserver(this);
  }
}

void ClearServerDataView::Init() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Font title_font =
      rb.GetFont(ResourceBundle::BaseFont).DeriveFont(0, gfx::Font::BOLD);

  flash_title_label_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_CLEAR_DATA_ADOBE_FLASH_TITLE)));
  flash_title_label_->SetFont(title_font);

  flash_description_label_= new views::Label(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_CLEAR_DATA_ADOBE_FLASH_DESCRIPTION)));
  flash_link_ = new views::Link(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_FLASH_STORAGE_SETTINGS)));
  flash_link_->SetController(this);

  chrome_sync_title_label_= new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_CLEAR_DATA_CHROME_SYNC_TITLE)));
  chrome_sync_title_label_->SetFont(title_font);

  chrome_sync_description_label_ = new views::Label(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_CLEAR_DATA_CLEAR_SERVER_DATA_DESCRIPTION)));

  clear_server_data_button_= new views::NativeButton(
      this,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_CLEAR_DATA_CLEAR_BUTTON)));

  dashboard_label_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_CLEAR_DASHBOARD_DESCRIPTION)));

  dashboard_link_ = new views::Link();
  dashboard_link_->SetController(this);
  dashboard_link_->SetText(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_SYNC_PRIVACY_DASHBOARD_LINK_LABEL)));

  status_label_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_CLEAR_DATA_DELETING)));
  throbber_ = new views::Throbber(50, true);
}

void ClearServerDataView::InitControlLayout() {
  GridLayout* layout = CreatePanelGridLayout(this);
  this->SetLayoutManager(layout);

  int centered_column_set_id = 0;
  ColumnSet * column_set = layout->AddColumnSet(centered_column_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  int left_aligned_column_set_id = 1;
  column_set = layout->AddColumnSet(left_aligned_column_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  const int three_column_set_id = 2;
  column_set = layout->AddColumnSet(three_column_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  AddWrappingLabelRow(layout, flash_title_label_,
                      centered_column_set_id, true);
  AddWrappingLabelRow(layout, flash_description_label_,
                      centered_column_set_id, true);

  layout->StartRow(0, left_aligned_column_set_id);
  layout->AddView(flash_link_);
  layout->AddPaddingRow(0, kPanelVertMargin);

  AddWrappingLabelRow(layout, chrome_sync_title_label_,
                      centered_column_set_id, true);
  AddWrappingLabelRow(layout, chrome_sync_description_label_,
                      centered_column_set_id, true);

  layout->StartRow(0, three_column_set_id);
  layout->AddView(clear_server_data_button_, 1, 1,
                  GridLayout::LEADING, GridLayout::CENTER);
  layout->AddView(status_label_, 1, 1,
                  GridLayout::LEADING, GridLayout::CENTER);
  layout->AddView(throbber_, 1, 1,
                  GridLayout::TRAILING, GridLayout::CENTER);
  layout->AddPaddingRow(0, kPanelVertMargin);

  AddWrappingLabelRow(layout, dashboard_label_, centered_column_set_id, true);

  layout->StartRow(0, left_aligned_column_set_id);
  layout->AddView(dashboard_link_);
}

void ClearServerDataView::InitControlVisibility() {
  bool allow_clear_server_data_ui =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableClearServerData);

  // Hide progress indicators
  throbber_->SetVisible(false);
  status_label_->SetVisible(false);

  // Only show the sync portion if not behind the flag
  chrome_sync_title_label_->SetVisible(allow_clear_server_data_ui);
  chrome_sync_description_label_->SetVisible(allow_clear_server_data_ui);
  clear_server_data_button_->SetVisible(allow_clear_server_data_ui);
  dashboard_label_->SetVisible(allow_clear_server_data_ui);
  dashboard_link_->SetVisible(allow_clear_server_data_ui);

  // Enable our clear button, set false for delete_in_progress
  UpdateClearButtonEnabledState(false);
}

void ClearServerDataView::SetAllowClear(bool allow) {
  allow_clear_ = allow;
  UpdateControlEnabledState();
}

////////////////////////////////////////////////////////////////////////////////
// ClearServerDataView, views::View implementation:

void ClearServerDataView::AddWrappingLabelRow(views::GridLayout* layout,
                                              views::Label* label,
                                              int id,
                                              bool related_follows) {
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout->StartRow(0, id);
  layout->AddView(label);
  AddSpacing(layout, related_follows);
}

void ClearServerDataView::AddSpacing(views::GridLayout* layout,
                                     bool related_follows) {
  layout->AddPaddingRow(0, related_follows ? kRelatedControlVerticalSpacing
                                           : kUnrelatedControlVerticalSpacing);
}

gfx::Size ClearServerDataView::GetPreferredSize() {
  // If we didn't return a preferred size, the containing view auto-sizes to
  // the width of the narrowest button, which is not what we want
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_CLEARDATA_DIALOG_WIDTH_CHARS,
      IDS_CLEARDATA_DIALOG_HEIGHT_LINES));
}

////////////////////////////////////////////////////////////////////////////////
// ClearServerDataView, views::ButtonListener implementation:

void ClearServerDataView::ButtonPressed(
    views::Button* sender, const views::Event& event) {

  if (sender == clear_server_data_button_) {
    // Protect against the unlikely case where the server received a
    // message, and the syncer syncs and resets itself before the
    // user tries pressing the Clear button in this dialog again.
    // TODO(raz) Confirm whether we have an issue here
    if (sync_service_->HasSyncSetupCompleted()) {
      ConfirmMessageBoxDialog::Run(
          GetWindow()->GetNativeWindow(),
          this,
          UTF16ToWide(l10n_util::GetStringUTF16(IDS_CONFIRM_CLEAR_DESCRIPTION)),
          UTF16ToWide(l10n_util::GetStringUTF16(IDS_CONFIRM_CLEAR_TITLE)));
      }
  }

  UpdateControlEnabledState();
}

void ClearServerDataView::LinkActivated(views::Link* source,
                                          int event_flags) {
  Browser* browser = Browser::Create(profile_);
  if (source == flash_link_) {
    browser->OpenURL(GURL(l10n_util::GetStringUTF8(IDS_FLASH_STORAGE_URL)),
                     GURL(), NEW_FOREGROUND_TAB, PageTransition::LINK);
    browser->window()->Show();
  } else {
    browser->OpenPrivacyDashboardTabAndActivate();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ClearServerDataView, ConfirmMessageBoxObserver implementation:

void ClearServerDataView::OnConfirmMessageAccept() {
  clear_data_parent_window_->StartClearingServerData();
  sync_service_->ClearServerData();
  UpdateControlEnabledState();
}

void ClearServerDataView::OnConfirmMessageCancel() {
  UpdateControlEnabledState();
}

////////////////////////////////////////////////////////////////////////////////
// ClearServerDataView, ProfileSyncServiceObserver implementation:

void ClearServerDataView::OnStateChanged() {
  // Note that we are listening for all notifications here.  Clearing from
  // another browser should cause the "clear" button to go gray (if a
  // notification nudges the syncer, causing OnStateChanged to be called with
  // sync disabled)
  UpdateControlEnabledState();
}

////////////////////////////////////////////////////////////////////////////////
// ClearServerDataView, private:

void ClearServerDataView::UpdateControlEnabledState() {
  bool delete_in_progress = false;

  // We only want to call Suceeded/FailedClearingServerData once, not every
  // time the view is refreshed.  As such, on success/failure we handle that
  // state and immediately reset things back to CLEAR_NOT_STARTED.
  ProfileSyncService::ClearServerDataState clear_state =
    profile_->GetProfileSyncService()->GetClearServerDataState();
  profile_->GetProfileSyncService()->ResetClearServerDataState();

  if (NULL != sync_service_) {
    sync_service_->ResetClearServerDataState();
  }

  switch (clear_state) {
    case ProfileSyncService::CLEAR_NOT_STARTED:
      // We can get here when we first start and after a failed clear (which
      // does not close the tab), do nothing.
      break;
    case ProfileSyncService::CLEAR_CLEARING:
        // Clearing buttons on all tabs are disabled at this
        // point, throbber is going
        status_label_->SetText(
            UTF16ToWide(l10n_util::GetStringUTF16(IDS_CLEAR_DATA_SENDING)));
        status_label_->SetVisible(true);
        delete_in_progress = true;
      break;
    case ProfileSyncService::CLEAR_FAILED:
        // Show an error and reallow clearing
        clear_data_parent_window_->FailedClearingServerData();
        status_label_->SetText(
            UTF16ToWide(l10n_util::GetStringUTF16(IDS_CLEAR_DATA_ERROR)));
        status_label_->SetVisible(true);
        delete_in_progress = false;
      break;
    case ProfileSyncService::CLEAR_SUCCEEDED:
        // Close the dialog box, success!
        status_label_->SetVisible(false);
        delete_in_progress = false;
        clear_data_parent_window_->SucceededClearingServerData();
      break;
    }

  // allow_clear can be false when a local browsing data clear is happening
  // from the neighboring tab.  delete_in_progress means that a clear is
  // pending in the current tab.
  UpdateClearButtonEnabledState(delete_in_progress);

  throbber_->SetVisible(delete_in_progress);
  if (delete_in_progress)
    throbber_->Start();
  else
    throbber_->Stop();
}

void ClearServerDataView::UpdateClearButtonEnabledState(
    bool delete_in_progress) {
  this->clear_server_data_button_->SetEnabled(
      sync_service_ != NULL &&
      sync_service_->HasSyncSetupCompleted() &&
      !delete_in_progress && allow_clear_);
}
