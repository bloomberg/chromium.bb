// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/options/content_page_view.h"

#include <windows.h>
#include <shlobj.h>
#include <vsstyle.h>
#include <vssym32.h>

#include <string>

#include "base/command_line.h"
#include "base/string_util.h"
#include "chrome/browser/autofill/autofill_dialog.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/sync/sync_setup_wizard.h"
#include "chrome/browser/ui/views/importer_view.h"
#include "chrome/browser/ui/views/options/managed_prefs_banner_view.h"
#include "chrome/browser/ui/views/options/options_group_view.h"
#include "chrome/browser/ui/views/options/passwords_exceptions_window_view.h"
#include "chrome/common/pref_names.h"
#include "gfx/canvas.h"
#include "gfx/native_theme_win.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/controls/button/radio_button.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

namespace {

// All the options pages are in the same view hierarchy. This means we need to
// make sure group identifiers don't collide across different pages.
const int kPasswordSavingRadioGroup = 201;
const int kFormAutofillRadioGroup = 202;

// Background color for the status label when it's showing an error.
static const SkColor kSyncLabelErrorBgColor = SkColorSetRGB(0xff, 0x9a, 0x9a);

static views::Background* CreateErrorBackground() {
  return views::Background::CreateSolidBackground(kSyncLabelErrorBgColor);
}

}  // namespace

ContentPageView::ContentPageView(Profile* profile)
    : show_passwords_button_(NULL),
      passwords_group_(NULL),
      passwords_asktosave_radio_(NULL),
      passwords_neversave_radio_(NULL),
      change_autofill_settings_button_(NULL),
      themes_group_(NULL),
      themes_reset_button_(NULL),
      themes_gallery_link_(NULL),
      browsing_data_group_(NULL),
      import_button_(NULL),
      sync_group_(NULL),
      sync_action_link_(NULL),
      sync_status_label_(NULL),
      sync_start_stop_button_(NULL),
      sync_customize_button_(NULL),
      privacy_dashboard_link_(NULL),
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
      UserMetricsRecordAction(
          UserMetricsAction("Options_PasswordManager_Enable"),
          profile()->GetPrefs());
    } else {
      UserMetricsRecordAction(
          UserMetricsAction("Options_PasswordManager_Disable"),
          profile()->GetPrefs());
    }
    ask_to_save_passwords_.SetValue(enabled);
  } else if (sender == show_passwords_button_) {
    UserMetricsRecordAction(
        UserMetricsAction("Options_ShowPasswordsExceptions"), NULL);
    PasswordsExceptionsWindowView::Show(profile());
  } else if (sender == change_autofill_settings_button_) {
    // This button should be disabled if we lack PersonalDataManager.
    DCHECK(profile()->GetPersonalDataManager());
    ShowAutoFillDialog(GetWindow()->GetNativeWindow(),
                       profile()->GetPersonalDataManager(),
                       profile());
  } else if (sender == themes_reset_button_) {
    UserMetricsRecordAction(UserMetricsAction("Options_ThemesReset"),
                            profile()->GetPrefs());
    profile()->ClearTheme();
  } else if (sender == import_button_) {
    views::Window::CreateChromeWindow(
      GetWindow()->GetNativeWindow(),
      gfx::Rect(),
      new ImporterView(profile(), importer::ALL))->Show();
  } else if (sender == sync_start_stop_button_) {
    DCHECK(sync_service_ && !sync_service_->IsManaged());

    if (sync_service_->HasSyncSetupCompleted()) {
      ConfirmMessageBoxDialog::RunWithCustomConfiguration(
          GetWindow()->GetNativeWindow(),
          this,
          UTF16ToWide(l10n_util::GetStringUTF16(
              IDS_SYNC_STOP_SYNCING_EXPLANATION_LABEL)),
          UTF16ToWide(l10n_util::GetStringUTF16(
              IDS_SYNC_STOP_SYNCING_DIALOG_TITLE)),
          UTF16ToWide(l10n_util::GetStringUTF16(
              IDS_SYNC_STOP_SYNCING_CONFIRM_BUTTON_LABEL)),
          UTF16ToWide(l10n_util::GetStringUTF16(IDS_CANCEL)),
          gfx::Size(views::Window::GetLocalizedContentsSize(
              IDS_CONFIRM_STOP_SYNCING_DIALOG_WIDTH_CHARS,
              IDS_CONFIRM_STOP_SYNCING_DIALOG_HEIGHT_LINES)));
      return;
    } else {
      sync_service_->ShowLoginDialog(GetWindow()->GetNativeWindow());
      ProfileSyncService::SyncEvent(ProfileSyncService::START_FROM_OPTIONS);
    }
  } else if (sender == sync_customize_button_) {
    // sync_customize_button_ should be invisible if sync is not yet set up.
    DCHECK(sync_service_->HasSyncSetupCompleted());
    sync_service_->ShowConfigure(GetWindow()->GetNativeWindow());
  }
}

void ContentPageView::LinkActivated(views::Link* source, int event_flags) {
  if (source == themes_gallery_link_) {
    UserMetricsRecordAction(UserMetricsAction("Options_ThemesGallery"),
                            profile()->GetPrefs());
    BrowserList::GetLastActive()->OpenThemeGalleryTabAndActivate();
    return;
  }
  if (source == sync_action_link_) {
    DCHECK(sync_service_ && !sync_service_->IsManaged());
    sync_service_->ShowErrorUI(GetWindow()->GetNativeWindow());
    return;
  }
  if (source == privacy_dashboard_link_) {
    BrowserList::GetLastActive()->OpenPrivacyDashboardTabAndActivate();
    return;
  }
  NOTREACHED() << "Invalid link source.";
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

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(
      new ManagedPrefsBannerView(profile()->GetPrefs(), OPTIONS_PAGE_CONTENT));

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
  form_autofill_enabled_.Init(prefs::kAutoFillEnabled,
                              profile()->GetPrefs(), this);
  is_using_default_theme_.Init(prefs::kCurrentThemeID,
                               profile()->GetPrefs(), this);
}

void ContentPageView::NotifyPrefChanged(const std::string* pref_name) {
  if (!pref_name || *pref_name == prefs::kPasswordManagerEnabled) {
    if (ask_to_save_passwords_.GetValue()) {
      passwords_asktosave_radio_->SetChecked(true);
    } else {
      passwords_neversave_radio_->SetChecked(true);
    }

    // Disable UI elements that are managed via policy.
    bool enablePasswordManagerElements = !ask_to_save_passwords_.IsManaged();
    passwords_asktosave_radio_->SetEnabled(enablePasswordManagerElements);
    passwords_neversave_radio_->SetEnabled(enablePasswordManagerElements);
    show_passwords_button_->SetEnabled(enablePasswordManagerElements ||
                                       ask_to_save_passwords_.GetValue());
  }
  if (!pref_name || *pref_name == prefs::kAutoFillEnabled) {
    bool disabled_by_policy = form_autofill_enabled_.IsManaged() &&
        !form_autofill_enabled_.GetValue();
    change_autofill_settings_button_->SetEnabled(
        !disabled_by_policy && profile()->GetPersonalDataManager());
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
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTIONS_PASSWORDS_ASKTOSAVE)),
      kPasswordSavingRadioGroup);
  passwords_asktosave_radio_->set_listener(this);
  passwords_asktosave_radio_->SetMultiLine(true);
  passwords_neversave_radio_ = new views::RadioButton(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTIONS_PASSWORDS_NEVERSAVE)),
      kPasswordSavingRadioGroup);
  passwords_neversave_radio_->set_listener(this);
  passwords_neversave_radio_->SetMultiLine(true);
  show_passwords_button_ = new views::NativeButton(
      this, UTF16ToWide(
          l10n_util::GetStringUTF16(IDS_OPTIONS_PASSWORDS_SHOWPASSWORDS)));

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
  layout->AddView(passwords_asktosave_radio_, 1, 1,
                  GridLayout::FILL, GridLayout::LEADING);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(passwords_neversave_radio_, 1, 1,
                  GridLayout::FILL, GridLayout::LEADING);
  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(show_passwords_button_);

  passwords_group_ = new OptionsGroupView(
      contents,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTIONS_PASSWORDS_GROUP_NAME)),
      L"",
      true);
}

void ContentPageView::InitFormAutofillGroup() {
  change_autofill_settings_button_ = new views::NativeButton(
      this, UTF16ToWide(l10n_util::GetStringUTF16(IDS_AUTOFILL_OPTIONS)));

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

  layout->StartRow(0, leading_column_view_set_id);
  layout->AddView(change_autofill_settings_button_);

  form_autofill_group_ = new OptionsGroupView(
      contents,
      UTF16ToWide(
          l10n_util::GetStringUTF16(IDS_AUTOFILL_SETTING_WINDOWS_GROUP_NAME)),
      L"", true);
}

void ContentPageView::InitThemesGroup() {
  themes_reset_button_ = new views::NativeButton(this,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_THEMES_RESET_BUTTON)));
  themes_gallery_link_ = new views::Link(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_THEMES_GALLERY_BUTTON)));
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
      contents, UTF16ToWide(l10n_util::GetStringUTF16(IDS_THEMES_GROUP_NAME)),
      L"", false);
}

void ContentPageView::InitBrowsingDataGroup() {
  import_button_ = new views::NativeButton(this,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTIONS_IMPORT_DATA_BUTTON)));

  using views::GridLayout;
  using views::ColumnSet;

  views::View* contents = new views::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  // Add the browsing data import button.
  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(import_button_);

  browsing_data_group_ = new OptionsGroupView(
      contents,
      UTF16ToWide(
          l10n_util::GetStringUTF16(IDS_OPTIONS_BROWSING_DATA_GROUP_NAME)),
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

  privacy_dashboard_link_ = new views::Link();
  privacy_dashboard_link_->set_collapse_when_hidden(true);
  privacy_dashboard_link_->SetController(this);
  privacy_dashboard_link_->SetText(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_SYNC_PRIVACY_DASHBOARD_LINK_LABEL)));

  sync_start_stop_button_ = new views::NativeButton(this, std::wstring());
  sync_customize_button_ = new views::NativeButton(this, std::wstring());

  using views::GridLayout;
  using views::ColumnSet;

  views::View* contents = new views::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(sync_status_label_, 3, 1,
                  GridLayout::FILL, GridLayout::LEADING);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(sync_action_link_, 3, 1);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(sync_start_stop_button_);
  layout->AddView(sync_customize_button_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(privacy_dashboard_link_, 3, 1);

  sync_group_ = new OptionsGroupView(
      contents,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_SYNC_OPTIONS_GROUP_NAME)),
      std::wstring(), true);
}

void ContentPageView::UpdateSyncControls() {
  DCHECK(sync_service_);
  std::wstring status_label;
  std::wstring link_label;
  std::wstring customize_button_label;
  string16 button_label;
  bool managed = sync_service_->IsManaged();
  bool sync_setup_completed = sync_service_->HasSyncSetupCompleted();
  bool status_has_error = sync_ui_util::GetStatusLabels(sync_service_,
      &status_label, &link_label) == sync_ui_util::SYNC_ERROR;
  customize_button_label =
    UTF16ToWide(l10n_util::GetStringUTF16(IDS_SYNC_CUSTOMIZE_BUTTON_LABEL));
  if (sync_setup_completed) {
    button_label =
        l10n_util::GetStringUTF16(IDS_SYNC_STOP_SYNCING_BUTTON_LABEL);
  } else if (sync_service_->SetupInProgress()) {
    button_label = l10n_util::GetStringUTF16(IDS_SYNC_NTP_SETUP_IN_PROGRESS);
  } else {
    button_label = l10n_util::GetStringUTF16(IDS_SYNC_START_SYNC_BUTTON_LABEL);
  }

  sync_status_label_->SetText(status_label);
  sync_start_stop_button_->SetEnabled(
      !sync_service_->WizardIsVisible() && !managed);
  sync_start_stop_button_->SetLabel(UTF16ToWide(button_label));
  sync_customize_button_->SetLabel(customize_button_label);
  sync_customize_button_->SetVisible(sync_setup_completed && !status_has_error);
  sync_customize_button_->SetEnabled(!managed);
  sync_action_link_->SetText(link_label);
  sync_action_link_->SetVisible(!link_label.empty());
  sync_action_link_->SetEnabled(!managed);

  if (status_has_error) {
    sync_status_label_->set_background(CreateErrorBackground());
    sync_action_link_->set_background(CreateErrorBackground());
  } else {
    sync_status_label_->set_background(NULL);
    sync_action_link_->set_background(NULL);
  }
}
