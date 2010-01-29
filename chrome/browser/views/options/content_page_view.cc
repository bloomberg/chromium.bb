// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/options/content_page_view.h"

#include <windows.h>
#include <shlobj.h>
#include <vsstyle.h>
#include <vssym32.h>

#include "app/gfx/canvas.h"
#include "app/gfx/native_theme_win.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/browser/autofill/autofill_dialog.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/views/importer_view.h"
#include "chrome/browser/views/options/options_group_view.h"
#include "chrome/browser/views/options/passwords_exceptions_window_view.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "views/controls/button/radio_button.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

namespace {

const int kPasswordSavingRadioGroup = 1;
const int kFormAutofillRadioGroup = 2;

// Background color for the status label when it's showing an error.
static const SkColor kSyncLabelErrorBgColor = SkColorSetRGB(0xff, 0x9a, 0x9a);

static views::Background* CreateErrorBackground() {
  return views::Background::CreateSolidBackground(kSyncLabelErrorBgColor);
}

}  // namespace

ContentPageView::ContentPageView(Profile* profile)
    : passwords_exceptions_button_(NULL),
      passwords_group_(NULL),
      passwords_asktosave_radio_(NULL),
      passwords_neversave_radio_(NULL),
      change_autofill_settings_button_(NULL),
      form_autofill_asktosave_radio_(NULL),
      form_autofill_neversave_radio_(NULL),
      themes_group_(NULL),
      themes_reset_button_(NULL),
      themes_gallery_link_(NULL),
      browsing_data_label_(NULL),
      browsing_data_group_(NULL),
      import_button_(NULL),
      sync_group_(NULL),
      sync_status_label_(NULL),
      sync_action_link_(NULL),
      sync_start_stop_button_(NULL),
      sync_service_(NULL),
      OptionsPageView(profile) {
  if (profile->GetProfileSyncService()) {
    sync_service_ = profile->GetProfileSyncService();
    sync_service_->AddObserver(this);
  }
}

ContentPageView::~ContentPageView() {
  if (sync_service_)
    sync_service_->RemoveObserver(this);
}

///////////////////////////////////////////////////////////////////////////////
// ContentPageView, views::ButtonListener implementation:

void ContentPageView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == passwords_asktosave_radio_ ||
      sender == passwords_neversave_radio_) {
    bool enabled = passwords_asktosave_radio_->checked();
    if (enabled) {
      UserMetricsRecordAction("Options_PasswordManager_Enable",
                              profile()->GetPrefs());
    } else {
      UserMetricsRecordAction("Options_PasswordManager_Disable",
                              profile()->GetPrefs());
    }
    ask_to_save_passwords_.SetValue(enabled);
  } else if (sender == form_autofill_asktosave_radio_ ||
             sender == form_autofill_neversave_radio_) {
    bool enabled = form_autofill_asktosave_radio_->checked();
    if (enabled) {
      UserMetricsRecordAction("Options_FormAutofill_Enable",
                              profile()->GetPrefs());
    } else {
      UserMetricsRecordAction("Options_FormAutofill_Disable",
                              profile()->GetPrefs());
    }
    ask_to_save_form_autofill_.SetValue(enabled);
  } else if (sender == passwords_exceptions_button_) {
    UserMetricsRecordAction("Options_ShowPasswordsExceptions", NULL);
    PasswordsExceptionsWindowView::Show(profile());
  } else if (sender == change_autofill_settings_button_) {
    ShowAutoFillDialog(
        profile()->GetPersonalDataManager(),
        profile()->GetPersonalDataManager()->profiles(),
        profile()->GetPersonalDataManager()->credit_cards());
  } else if (sender == themes_reset_button_) {
    UserMetricsRecordAction("Options_ThemesReset", profile()->GetPrefs());
    profile()->ClearTheme();
  } else if (sender == import_button_) {
    views::Window::CreateChromeWindow(
      GetWindow()->GetNativeWindow(),
      gfx::Rect(),
      new ImporterView(profile(), ALL))->Show();
  } else if (sender == sync_start_stop_button_) {
    DCHECK(sync_service_);

    if (sync_service_->HasSyncSetupCompleted()) {
      ConfirmMessageBoxDialog::RunWithCustomConfiguration(
          GetWindow()->GetNativeWindow(),
          this,
          l10n_util::GetString(IDS_SYNC_STOP_SYNCING_EXPLANATION_LABEL),
          l10n_util::GetString(IDS_SYNC_STOP_SYNCING_BUTTON_LABEL),
          l10n_util::GetString(IDS_SYNC_STOP_SYNCING_CONFIRM_BUTTON_LABEL),
          l10n_util::GetString(IDS_CANCEL),
          gfx::Size(views::Window::GetLocalizedContentsSize(
              IDS_CONFIRM_STOP_SYNCING_DIALOG_WIDTH_CHARS,
              IDS_CONFIRM_STOP_SYNCING_DIALOG_HEIGHT_LINES)));
      return;
    } else {
      sync_service_->EnableForUser();
      ProfileSyncService::SyncEvent(ProfileSyncService::START_FROM_OPTIONS);
    }
  }
}

void ContentPageView::LinkActivated(views::Link* source, int event_flags) {
  if (source == themes_gallery_link_) {
    UserMetricsRecordAction("Options_ThemesGallery", profile()->GetPrefs());
    BrowserList::GetLastActive()->OpenThemeGalleryTabAndActivate();
    return;
  }
  DCHECK_EQ(source, sync_action_link_);
  DCHECK(sync_service_);
  sync_service_->ShowLoginDialog();
}

////////////////////////////////////////////////////////////////////////////////
// ContentPageView, OptionsPageView implementation:

void ContentPageView::InitControlLayout() {
  using views::GridLayout;
  using views::ColumnSet;

  GridLayout* layout = new GridLayout(this);
  layout->SetInsets(5, 5, 5, 5);
  SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  if (sync_service_) {
    layout->StartRow(0, single_column_view_set_id);
    InitSyncGroup();
    layout->AddView(sync_group_);
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  }

  layout->StartRow(0, single_column_view_set_id);
  InitPasswordSavingGroup();
  layout->AddView(passwords_group_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_view_set_id);
  InitFormAutofillGroup();
  layout->AddView(form_autofill_group_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_view_set_id);
  InitBrowsingDataGroup();
  layout->AddView(browsing_data_group_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_view_set_id);
  InitThemesGroup();
  layout->AddView(themes_group_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Init member prefs so we can update the controls if prefs change.
  ask_to_save_passwords_.Init(prefs::kPasswordManagerEnabled,
                              profile()->GetPrefs(), this);
  ask_to_save_form_autofill_.Init(prefs::kFormAutofillEnabled,
                                  profile()->GetPrefs(), this);
  is_using_default_theme_.Init(prefs::kCurrentThemeID,
                               profile()->GetPrefs(), this);
}

void ContentPageView::NotifyPrefChanged(const std::wstring* pref_name) {
  if (!pref_name || *pref_name == prefs::kPasswordManagerEnabled) {
    if (ask_to_save_passwords_.GetValue()) {
      passwords_asktosave_radio_->SetChecked(true);
    } else {
      passwords_neversave_radio_->SetChecked(true);
    }
  }
  if (!pref_name || *pref_name == prefs::kFormAutofillEnabled) {
    if (ask_to_save_form_autofill_.GetValue()) {
      form_autofill_asktosave_radio_->SetChecked(true);
    } else {
      form_autofill_neversave_radio_->SetChecked(true);
    }
  }
  if (!pref_name || *pref_name == prefs::kCurrentThemeID) {
    themes_reset_button_->SetEnabled(
        is_using_default_theme_.GetValue().length() > 0);
  }
}

///////////////////////////////////////////////////////////////////////////////
// ContentsPageView, views::View overrides:

void ContentPageView::Layout() {
  if (is_initialized())
    UpdateSyncControls();
  // We need to Layout twice - once to get the width of the contents box...
  View::Layout();
  passwords_asktosave_radio_->SetBounds(
      0, 0, passwords_group_->GetContentsWidth(), 0);
  passwords_neversave_radio_->SetBounds(
      0, 0, passwords_group_->GetContentsWidth(), 0);
  browsing_data_label_->SetBounds(
      0, 0, browsing_data_group_->GetContentsWidth(), 0);
  if (is_initialized()) {
    sync_status_label_->SetBounds(
        0, 0, sync_group_->GetContentsWidth(), 0);
  }
  // ... and twice to get the height of multi-line items correct.
  View::Layout();
}


///////////////////////////////////////////////////////////////////////////////
// ContentsPageView, ProfileSyncServiceObserver implementation:

void ContentPageView::OnStateChanged() {
  // If the UI controls are not yet initialized, then don't do anything. This
  // can happen if the Options dialog is up, but the Content tab is not yet
  // clicked.
  if (is_initialized())
    Layout();
}

///////////////////////////////////////////////////////////////////////////////
// ContentPageView, private:

void ContentPageView::InitPasswordSavingGroup() {
  passwords_asktosave_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_OPTIONS_PASSWORDS_ASKTOSAVE),
      kPasswordSavingRadioGroup);
  passwords_asktosave_radio_->set_listener(this);
  passwords_asktosave_radio_->SetMultiLine(true);
  passwords_neversave_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_OPTIONS_PASSWORDS_NEVERSAVE),
      kPasswordSavingRadioGroup);
  passwords_neversave_radio_->set_listener(this);
  passwords_neversave_radio_->SetMultiLine(true);
  passwords_exceptions_button_ = new views::NativeButton(
      this, l10n_util::GetString(IDS_OPTIONS_PASSWORDS_EXCEPTIONS));

  using views::GridLayout;
  using views::ColumnSet;

  views::View* contents = new views::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(passwords_asktosave_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(passwords_neversave_radio_);
  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(passwords_exceptions_button_);

  passwords_group_ = new OptionsGroupView(
      contents, l10n_util::GetString(IDS_OPTIONS_PASSWORDS_GROUP_NAME), L"",
      true);
}

void ContentPageView::InitFormAutofillGroup() {
  form_autofill_asktosave_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_OPTIONS_AUTOFILL_SAVE),
      kFormAutofillRadioGroup);
  form_autofill_asktosave_radio_->set_listener(this);
  form_autofill_asktosave_radio_->SetMultiLine(true);
  form_autofill_neversave_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_OPTIONS_AUTOFILL_NEVERSAVE),
      kFormAutofillRadioGroup);
  form_autofill_neversave_radio_->set_listener(this);
  form_autofill_neversave_radio_->SetMultiLine(true);

  change_autofill_settings_button_ = new views::NativeButton(
      this, l10n_util::GetString(IDS_OPTIONS_AUTOFILL_SETTINGS));

  using views::GridLayout;
  using views::ColumnSet;

  views::View* contents = new views::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  const int fill_column_view_set_id = 0;
  const int leading_column_view_set_id = 1;
  ColumnSet* column_set = layout->AddColumnSet(fill_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set = layout->AddColumnSet(leading_column_view_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, fill_column_view_set_id);
  layout->AddView(form_autofill_asktosave_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, fill_column_view_set_id);
  layout->AddView(form_autofill_neversave_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, leading_column_view_set_id);
  layout->AddView(change_autofill_settings_button_);

  form_autofill_group_ = new OptionsGroupView(
      contents, l10n_util::GetString(IDS_AUTOFILL_SETTING_WINDOWS_GROUP_NAME),
      L"", true);
}

void ContentPageView::InitThemesGroup() {
  themes_reset_button_ = new views::NativeButton(this,
      l10n_util::GetString(IDS_THEMES_RESET_BUTTON));
  themes_gallery_link_ = new views::Link(
      l10n_util::GetString(IDS_THEMES_GALLERY_BUTTON));
  themes_gallery_link_->SetController(this);

  using views::GridLayout;
  using views::ColumnSet;

  views::View* contents = new views::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  const int double_column_view_set_id = 1;
  ColumnSet* double_col_set = layout->AddColumnSet(double_column_view_set_id);
  double_col_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                            GridLayout::USE_PREF, 0, 0);
  double_col_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  double_col_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                            GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, double_column_view_set_id);
  layout->AddView(themes_reset_button_);
  layout->AddView(themes_gallery_link_);

  themes_group_ = new OptionsGroupView(
      contents, l10n_util::GetString(IDS_THEMES_GROUP_NAME),
      L"", false);
}

void ContentPageView::InitBrowsingDataGroup() {
  import_button_ = new views::NativeButton(this,
      l10n_util::GetString(IDS_OPTIONS_IMPORT_DATA_BUTTON));
  browsing_data_label_ = new views::Label(
      l10n_util::GetString(IDS_OPTIONS_BROWSING_DATA_INFO));
  browsing_data_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  browsing_data_label_->SetMultiLine(true);

  using views::GridLayout;
  using views::ColumnSet;

  views::View* contents = new views::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  // Add the browsing data label component.
  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(browsing_data_label_);

  // Add some padding for not making the next component close together.
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(import_button_);

  browsing_data_group_ = new OptionsGroupView(
      contents, l10n_util::GetString(IDS_OPTIONS_BROWSING_DATA_GROUP_NAME),
      L"", true);
}

void ContentPageView::OnConfirmMessageAccept() {
  sync_service_->DisableForUser();
  ProfileSyncService::SyncEvent(ProfileSyncService::STOP_FROM_OPTIONS);
}

void ContentPageView::InitSyncGroup() {
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
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(sync_status_label_);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(sync_action_link_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(sync_start_stop_button_);

  sync_group_ = new OptionsGroupView(contents,
      l10n_util::GetString(IDS_SYNC_OPTIONS_GROUP_NAME), std::wstring(), true);
}

void ContentPageView::UpdateSyncControls() {
  DCHECK(sync_service_);
  std::wstring status_label;
  std::wstring link_label;
  std::wstring button_label;
  bool sync_setup_completed = sync_service_->HasSyncSetupCompleted();
  bool status_has_error = sync_ui_util::GetStatusLabels(sync_service_,
      &status_label, &link_label) == sync_ui_util::SYNC_ERROR;
  if (sync_setup_completed) {
    button_label = l10n_util::GetString(IDS_SYNC_STOP_SYNCING_BUTTON_LABEL);
  } else if (sync_service_->SetupInProgress()) {
    button_label = l10n_util::GetString(IDS_SYNC_NTP_SETUP_IN_PROGRESS);
  } else {
    button_label = l10n_util::GetString(IDS_SYNC_START_SYNC_BUTTON_LABEL);
  }

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
