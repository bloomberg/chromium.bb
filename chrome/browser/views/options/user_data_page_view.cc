// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef CHROME_PERSONALIZATION

#include "chrome/browser/views/options/user_data_page_view.h"

#include "app/l10n_util.h"
#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/sync/auth_error_state.h"
#include "chrome/browser/sync/personalization_strings.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_status_ui_helper.h"
#include "chrome/browser/views/clear_browsing_data.h"
#include "chrome/browser/views/importer_view.h"
#include "chrome/browser/views/options/options_group_view.h"
#include "chrome/browser/views/options/options_page_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_member.h"
#include "chrome/common/pref_service.h"
#include "grit/generated_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "views/background.h"
#include "views/controls/button/native_button.h"
#include "views/controls/label.h"
#include "views/controls/link.h"
#include "views/controls/table/table_view.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/view.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

// Background color for the status label when it's showing an error.
static const SkColor kSyncLabelErrorBgColor = SkColorSetRGB(0xff, 0x9a, 0x9a);

static views::Background* CreateErrorBackground() {
  return views::Background::CreateSolidBackground(kSyncLabelErrorBgColor);
}

UserDataPageView::UserDataPageView(Profile* profile)
    : OptionsPageView(profile),
      sync_group_(NULL),
      sync_status_label_(NULL),
      sync_action_link_(NULL),
      sync_start_stop_button_(NULL),
      sync_service_(profile->GetProfilePersonalization()->sync_service()) {
  DCHECK(sync_service_);
  sync_service_->AddObserver(this);
}

UserDataPageView::~UserDataPageView() {
  sync_service_->RemoveObserver(this);
}

void UserDataPageView::ButtonPressed(views::Button* sender) {
  DCHECK(sender == sync_start_stop_button_);
  if (!sync_service_->IsSyncEnabledByUser()) {
    // Sync has not been enabled yet.
    sync_service_->EnableForUser();
  } else {
    sync_service_->DisableForUser();
  }
}

void UserDataPageView::LinkActivated(views::Link* source, int event_flags) {
  DCHECK_EQ(source, sync_action_link_);
  sync_service_->ShowLoginDialog();
}

void UserDataPageView::InitControlLayout() {
  using views::GridLayout;
  using views::ColumnSet;

  GridLayout* layout = new GridLayout(this);
  layout->SetInsets(5, 5, 5, 5);
  SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, single_column_view_set_id);
  InitSyncGroup();
  layout->AddView(sync_group_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
}

void UserDataPageView::NotifyPrefChanged(const std::wstring* pref_name) {
}

void UserDataPageView::OnStateChanged() {
  // If the UI controls are not yet initialized, then don't do anything. This
  // can happen if the Options dialog is up, but the User Data tab is not yet
  // clicked.
  if (IsInitialized())
    Layout();
}

void UserDataPageView::HighlightGroup(OptionsGroup highlight_group) {
}

void UserDataPageView::Layout() {
  UpdateControls();
  // We need to Layout twice - once to get the width of the contents box...
  View::Layout();
  int sync_group_width = sync_group_->GetContentsWidth();
  sync_status_label_->SetBounds(0, 0, sync_group_width, 0);
  // ... and twice to get the height of multi-line items correct.
  View::Layout();
}

void UserDataPageView::InitSyncGroup() {
  sync_status_label_ = new views::Label;
  sync_status_label_->SetMultiLine(true);
  sync_status_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);

  sync_action_link_ = new views::Link();
  sync_action_link_->set_collapse_when_hidden(true);
  sync_action_link_->SetController(this);

  sync_start_stop_button_ = new views::NativeButton(this, std::wstring());

  using views::GridLayout;
  using views::ColumnSet;

  views::View* contents = new views::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  // Currently we add the status label and the link in two rows. This is not
  // the same as the mocks. If we add label and the link in two columns to make
  // it look like the mocks then we can have a UI layout issue. If the label
  // has text that fits in one line, the appearance is fine. But if the label
  // has text with multiple lines, the appearance can be a bit detached. See
  // bug 1648522 for details.
  // TODO(munjal): Change the layout after we fix bug 1648522.
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(sync_status_label_);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(sync_action_link_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(sync_start_stop_button_);

  sync_group_ = new OptionsGroupView(contents,
                                     kSyncGroupName,
                                     std::wstring(),
                                     true);
}

void UserDataPageView::UpdateControls() {
  std::wstring status_label;
  std::wstring link_label;
  std::wstring button_label;
  bool sync_enabled = sync_service_->IsSyncEnabledByUser();
  bool status_has_error = SyncStatusUIHelper::GetLabels(sync_service_,
      &status_label, &link_label) == SyncStatusUIHelper::SYNC_ERROR;
  button_label = sync_enabled ? kStopSyncButtonLabel :
                 sync_service_->SetupInProgress() ? UTF8ToWide(kSettingUpText)
                 : kStartSyncButtonLabel;

  sync_status_label_->SetText(status_label);
  sync_start_stop_button_->SetEnabled(!sync_service_->WizardIsVisible());
  sync_start_stop_button_->SetLabel(button_label);
  sync_action_link_->SetText(link_label);
  sync_action_link_->SetVisible(!link_label.empty());
  if (status_has_error) {
    sync_status_label_->set_background(CreateErrorBackground());
    sync_action_link_->set_background(CreateErrorBackground());
  } else {
    sync_status_label_->set_background(NULL);
    sync_action_link_->set_background(NULL);
  }
}

#endif  // CHROME_PERSONALIZATION
