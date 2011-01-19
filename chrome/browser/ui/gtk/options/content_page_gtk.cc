// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/options/content_page_gtk.h"

#include <string>

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_provider.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/import_dialog_gtk.h"
#include "chrome/browser/ui/gtk/options/options_layout_gtk.h"
#include "chrome/browser/ui/gtk/options/passwords_exceptions_window_gtk.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "gfx/gtk_util.h"
#include "grit/app_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/options/options_window_view.h"
#endif  // defined(OS_CHROMEOS)

namespace {

// Background color for the status label when it's showing an error.
static const GdkColor kSyncLabelErrorBgColor = GDK_COLOR_RGB(0xff, 0x9a, 0x9a);

// Helper for WrapLabelAtAllocationHack.
void OnLabelAllocate(GtkWidget* label, GtkAllocation* allocation) {
  gtk_util::SetLabelWidth(label, allocation->width);

  // Disconnect ourselves.  Repeatedly resizing based on allocation causes
  // the dialog to become unshrinkable.
  g_signal_handlers_disconnect_by_func(
      label, reinterpret_cast<gpointer>(OnLabelAllocate), NULL);
}

// Set the label to use a request size equal to its initial allocation
// size.  This causes the label to wrap at the width of the container
// it is in, instead of at the default width.  This is called a hack
// because GTK doesn't really work when a widget to make its size
// request depend on its allocation.  It does, however, have the
// intended effect of wrapping the label at the proper width.
void WrapLabelAtAllocationHack(GtkWidget* label) {
  g_signal_connect(label, "size-allocate",
                   G_CALLBACK(OnLabelAllocate), NULL);
}

}  // anonymous namespace

///////////////////////////////////////////////////////////////////////////////
// ContentPageGtk, public:

ContentPageGtk::ContentPageGtk(Profile* profile)
    : OptionsPageBase(profile),
      sync_status_label_background_(NULL),
      sync_status_label_(NULL),
#if !defined(OS_CHROMEOS)
      sync_action_link_background_(NULL),
      sync_action_link_(NULL),
#endif
      sync_start_stop_button_(NULL),
      sync_customize_button_(NULL),
      privacy_dashboard_link_(NULL),
      initializing_(true),
      sync_service_(NULL),
      managed_prefs_banner_(profile->GetPrefs(), OPTIONS_PAGE_CONTENT) {
  if (profile->GetProfileSyncService()) {
    sync_service_ = profile->GetProfileSyncService();
    sync_service_->AddObserver(this);
  }

  // Prepare the group options layout.
  scoped_ptr<OptionsLayoutBuilderGtk>
    options_builder(OptionsLayoutBuilderGtk::CreateOptionallyCompactLayout());
  options_builder->AddWidget(managed_prefs_banner_.banner_widget(), false);
  if (sync_service_) {
    options_builder->AddOptionGroup(
        l10n_util::GetStringUTF8(IDS_SYNC_OPTIONS_GROUP_NAME),
        InitSyncGroup(), false);
    UpdateSyncControls();
  }

  // Add preferences observers.
  ask_to_save_passwords_.Init(prefs::kPasswordManagerEnabled,
                              profile->GetPrefs(), this);
  form_autofill_enabled_.Init(prefs::kAutoFillEnabled,
                              profile->GetPrefs(), this);
  if (browser_defaults::kCanToggleSystemTitleBar) {
    use_custom_chrome_frame_.Init(prefs::kUseCustomChromeFrame,
                                  profile->GetPrefs(), this);
  }

  options_builder->AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_OPTIONS_PASSWORDS_GROUP_NAME),
      InitPasswordSavingGroup(), false);
  options_builder->AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_SETTING_WINDOWS_GROUP_NAME),
      InitFormAutoFillGroup(), false);
  options_builder->AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_OPTIONS_BROWSING_DATA_GROUP_NAME),
      InitBrowsingDataGroup(), false);
  options_builder->AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_APPEARANCE_GROUP_NAME),
      InitThemesGroup(), false);
  page_ = options_builder->get_page_widget();

  // Load initial values.
  NotifyPrefChanged(NULL);

  registrar_.Add(this, NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
  ObserveThemeChanged();
}

ContentPageGtk::~ContentPageGtk() {
  if (sync_service_)
    sync_service_->RemoveObserver(this);
}

///////////////////////////////////////////////////////////////////////////////
// ContentsPageView, ProfileSyncServiceObserver implementation:

void ContentPageGtk::OnStateChanged() {
  // If the UI controls are not yet initialized, then don't do anything. This
  // can happen if the Options dialog is up, but the Content tab is not yet
  // clicked.
  if (!initializing_)
    UpdateSyncControls();
}

///////////////////////////////////////////////////////////////////////////////
// ContentPageGtk, private:

// If |pref_name| is NULL, set the state of all the widgets. (This is used
// in ContentPageGtk() above to initialize the dialog.) Otherwise, reset the
// state of the widget for the given preference name, as it has changed.
void ContentPageGtk::NotifyPrefChanged(const std::string* pref_name) {
  initializing_ = true;
  if (!pref_name || *pref_name == prefs::kPasswordManagerEnabled) {
    if (ask_to_save_passwords_.GetValue()) {
      gtk_toggle_button_set_active(
          GTK_TOGGLE_BUTTON(passwords_asktosave_radio_), TRUE);
    } else {
      gtk_toggle_button_set_active(
          GTK_TOGGLE_BUTTON(passwords_neversave_radio_), TRUE);
    }
    bool isPasswordManagerEnabled = !ask_to_save_passwords_.IsManaged();
    gtk_widget_set_sensitive(passwords_asktosave_radio_,
                             isPasswordManagerEnabled);
    gtk_widget_set_sensitive(passwords_neversave_radio_,
                             isPasswordManagerEnabled);
    gtk_widget_set_sensitive(show_passwords_button_,
                             isPasswordManagerEnabled ||
                             ask_to_save_passwords_.GetValue());
  }
  if (!pref_name || *pref_name == prefs::kAutoFillEnabled) {
    bool disabled_by_policy = form_autofill_enabled_.IsManaged() &&
        !form_autofill_enabled_.GetValue();
    gtk_widget_set_sensitive(autofill_button_, !disabled_by_policy);
  }
  if (browser_defaults::kCanToggleSystemTitleBar &&
      (!pref_name || *pref_name == prefs::kUseCustomChromeFrame)) {
    if (use_custom_chrome_frame_.GetValue()) {
      gtk_toggle_button_set_active(
          GTK_TOGGLE_BUTTON(system_title_bar_hide_radio_), TRUE);
    } else {
      gtk_toggle_button_set_active(
          GTK_TOGGLE_BUTTON(system_title_bar_show_radio_), TRUE);
    }
  }
  initializing_ = false;
}

void ContentPageGtk::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  if (type == NotificationType::BROWSER_THEME_CHANGED)
    ObserveThemeChanged();
  else
    OptionsPageBase::Observe(type, source, details);
}

void ContentPageGtk::ObserveThemeChanged() {
#if defined(TOOLKIT_GTK)
  GtkThemeProvider* provider = GtkThemeProvider::GetFrom(profile());
  bool is_gtk_theme = provider->UseGtkTheme();
  gtk_widget_set_sensitive(gtk_theme_button_, !is_gtk_theme);
#else
  BrowserThemeProvider* provider =
      reinterpret_cast<BrowserThemeProvider*>(profile()->GetThemeProvider());
  bool is_gtk_theme = false;
#endif

  bool is_classic_theme = !is_gtk_theme && provider->GetThemeID().empty();
  gtk_widget_set_sensitive(themes_reset_button_, !is_classic_theme);
}

GtkWidget* ContentPageGtk::InitPasswordSavingGroup() {
  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  // Ask to save radio button.
  passwords_asktosave_radio_ = gtk_radio_button_new_with_label(NULL,
      l10n_util::GetStringUTF8(IDS_OPTIONS_PASSWORDS_ASKTOSAVE).c_str());
  g_signal_connect(passwords_asktosave_radio_, "toggled",
                   G_CALLBACK(OnPasswordRadioToggledThunk), this);
  gtk_box_pack_start(GTK_BOX(vbox), passwords_asktosave_radio_, FALSE,
                     FALSE, 0);

  // Never save radio button.
  passwords_neversave_radio_ = gtk_radio_button_new_with_label_from_widget(
      GTK_RADIO_BUTTON(passwords_asktosave_radio_),
      l10n_util::GetStringUTF8(IDS_OPTIONS_PASSWORDS_NEVERSAVE).c_str());
  g_signal_connect(passwords_neversave_radio_, "toggled",
                   G_CALLBACK(OnPasswordRadioToggledThunk), this);
  gtk_box_pack_start(GTK_BOX(vbox), passwords_neversave_radio_, FALSE,
                     FALSE, 0);

  // Add the show passwords button into its own horizontal box so it does not
  // depend on the spacing above.
  GtkWidget* button_hbox = gtk_hbox_new(FALSE, gtk_util::kLabelSpacing);
  gtk_container_add(GTK_CONTAINER(vbox), button_hbox);
  show_passwords_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_OPTIONS_PASSWORDS_SHOWPASSWORDS).c_str());
  g_signal_connect(show_passwords_button_, "clicked",
                   G_CALLBACK(OnShowPasswordsButtonClickedThunk), this);
  gtk_box_pack_start(GTK_BOX(button_hbox), show_passwords_button_, FALSE,
                     FALSE, 0);

  return vbox;
}

GtkWidget* ContentPageGtk::InitFormAutoFillGroup() {
  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  GtkWidget* button_hbox = gtk_hbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_container_add(GTK_CONTAINER(vbox), button_hbox);

  // AutoFill button.
  autofill_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_OPTIONS).c_str());

  g_signal_connect(G_OBJECT(autofill_button_), "clicked",
                   G_CALLBACK(OnAutoFillButtonClickedThunk), this);
  gtk_box_pack_start(GTK_BOX(button_hbox), autofill_button_, FALSE, FALSE, 0);

  return vbox;
}

GtkWidget* ContentPageGtk::InitBrowsingDataGroup() {
  GtkWidget* button_box = gtk_hbox_new(FALSE, gtk_util::kControlSpacing);

  // Import button.
  GtkWidget* import_button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_OPTIONS_IMPORT_DATA_BUTTON).c_str());
  g_signal_connect(import_button, "clicked",
                   G_CALLBACK(OnImportButtonClickedThunk), this);
  gtk_box_pack_start(GTK_BOX(button_box), import_button, FALSE, FALSE, 0);

  return button_box;
}

GtkWidget* ContentPageGtk::InitThemesGroup() {
  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  GtkWidget* hbox = gtk_hbox_new(FALSE, gtk_util::kControlSpacing);

#if defined(TOOLKIT_GTK)
  // GTK theme button.
  gtk_theme_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_THEMES_GTK_BUTTON).c_str());
  g_signal_connect(gtk_theme_button_, "clicked",
                   G_CALLBACK(OnGtkThemeButtonClickedThunk), this);
  gtk_box_pack_start(GTK_BOX(hbox), gtk_theme_button_, FALSE, FALSE, 0);
#endif

  // Reset theme button.
  themes_reset_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_THEMES_SET_CLASSIC).c_str());
  g_signal_connect(themes_reset_button_, "clicked",
                   G_CALLBACK(OnResetDefaultThemeButtonClickedThunk), this);
  gtk_box_pack_start(GTK_BOX(hbox), themes_reset_button_, FALSE, FALSE, 0);

  // Get themes button.
  GtkWidget* themes_gallery_button = gtk_chrome_link_button_new(
      l10n_util::GetStringUTF8(IDS_THEMES_GALLERY_BUTTON).c_str());
  g_signal_connect(themes_gallery_button, "clicked",
                   G_CALLBACK(OnGetThemesButtonClickedThunk), this);
  gtk_box_pack_start(GTK_BOX(hbox), themes_gallery_button, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  // "Use system title bar and borders" radio buttons.
  if (browser_defaults::kCanToggleSystemTitleBar) {
    // Use system title bar and borders
    system_title_bar_show_radio_ = gtk_radio_button_new_with_label(NULL,
        l10n_util::GetStringUTF8(IDS_SHOW_WINDOW_DECORATIONS_RADIO).c_str());
    g_signal_connect(system_title_bar_show_radio_, "toggled",
                     G_CALLBACK(OnSystemTitleBarRadioToggledThunk), this);
    gtk_box_pack_start(GTK_BOX(vbox), system_title_bar_show_radio_, FALSE,
                       FALSE, 0);

    // Hide system title bar and use custom borders
    system_title_bar_hide_radio_ = gtk_radio_button_new_with_label_from_widget(
        GTK_RADIO_BUTTON(system_title_bar_show_radio_),
        l10n_util::GetStringUTF8(IDS_HIDE_WINDOW_DECORATIONS_RADIO).c_str());
    g_signal_connect(system_title_bar_hide_radio_, "toggled",
                     G_CALLBACK(OnSystemTitleBarRadioToggledThunk), this);
    gtk_box_pack_start(GTK_BOX(vbox), system_title_bar_hide_radio_, FALSE,
                       FALSE, 0);
  }

  return vbox;
}

GtkWidget* ContentPageGtk::InitSyncGroup() {
  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  // Sync label.
  sync_status_label_background_ = gtk_event_box_new();
  sync_status_label_ = gtk_label_new("");
  WrapLabelAtAllocationHack(sync_status_label_);

  gtk_misc_set_alignment(GTK_MISC(sync_status_label_), 0, 0.5);
  gtk_box_pack_start(GTK_BOX(vbox), sync_status_label_background_, FALSE,
                     FALSE, 0);
  gtk_container_add(GTK_CONTAINER(sync_status_label_background_),
                    sync_status_label_);

#if !defined(OS_CHROMEOS)
  // Sync action link.
  GtkWidget* link_hbox = gtk_hbox_new(FALSE, gtk_util::kLabelSpacing);
  sync_action_link_background_ = gtk_event_box_new();
  sync_action_link_ = gtk_chrome_link_button_new("");
  g_signal_connect(sync_action_link_, "clicked",
                   G_CALLBACK(OnSyncActionLinkClickedThunk), this);
  gtk_box_pack_start(GTK_BOX(vbox), link_hbox, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(link_hbox), sync_action_link_background_, FALSE,
                     FALSE, 0);
  gtk_container_add(GTK_CONTAINER(sync_action_link_background_),
                    sync_action_link_);
  gtk_widget_hide(sync_action_link_background_);
#endif

  // Add the sync button into its own horizontal box so it does not
  // depend on the spacing above.
  GtkWidget* button_hbox = gtk_hbox_new(FALSE, gtk_util::kLabelSpacing);
  gtk_container_add(GTK_CONTAINER(vbox), button_hbox);
  sync_start_stop_button_ = gtk_button_new_with_label("");
  g_signal_connect(sync_start_stop_button_, "clicked",
                   G_CALLBACK(OnSyncStartStopButtonClickedThunk), this);
  gtk_box_pack_start(GTK_BOX(button_hbox), sync_start_stop_button_, FALSE,
                     FALSE, 0);
  sync_customize_button_ = gtk_button_new_with_label("");
  g_signal_connect(sync_customize_button_, "clicked",
                   G_CALLBACK(OnSyncCustomizeButtonClickedThunk), this);
  gtk_box_pack_start(GTK_BOX(button_hbox), sync_customize_button_, FALSE,
                     FALSE, 0);

  // Add the privacy dashboard link.
  GtkWidget* dashboard_link_hbox =
      gtk_hbox_new(FALSE, gtk_util::kLabelSpacing);
  GtkWidget* dashboard_link_background = gtk_event_box_new();
  std::string dashboard_link_label =
      l10n_util::GetStringUTF8(IDS_SYNC_PRIVACY_DASHBOARD_LINK_LABEL);
  privacy_dashboard_link_ =
      gtk_chrome_link_button_new(dashboard_link_label.c_str());
  g_signal_connect(privacy_dashboard_link_, "clicked",
                   G_CALLBACK(OnPrivacyDashboardLinkClickedThunk), this);
  gtk_box_pack_start(GTK_BOX(vbox), dashboard_link_hbox, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(dashboard_link_hbox),
                     dashboard_link_background, FALSE, FALSE, 0);
  gtk_container_add(GTK_CONTAINER(dashboard_link_background),
                    privacy_dashboard_link_);


  return vbox;
}

void ContentPageGtk::UpdateSyncControls() {
  DCHECK(sync_service_);
  string16 status_label;
  string16 link_label;
  std::string customize_button_label;
  bool managed = sync_service_->IsManaged();
  bool sync_setup_completed = sync_service_->HasSyncSetupCompleted();
  bool status_has_error = sync_ui_util::GetStatusLabels(sync_service_,
      &status_label, &link_label) == sync_ui_util::SYNC_ERROR;
  customize_button_label =
    l10n_util::GetStringUTF8(IDS_SYNC_CUSTOMIZE_BUTTON_LABEL);

  std::string start_stop_button_label;
  bool is_start_stop_button_visible = false;
  bool is_start_stop_button_sensitive = false;
  if (sync_setup_completed) {
    start_stop_button_label =
        l10n_util::GetStringUTF8(IDS_SYNC_STOP_SYNCING_BUTTON_LABEL);
#if defined(OS_CHROMEOS)
    is_start_stop_button_visible = false;
#else
    is_start_stop_button_visible = true;
#endif
    is_start_stop_button_sensitive = !managed;
  } else if (sync_service_->SetupInProgress()) {
    start_stop_button_label =
        l10n_util::GetStringUTF8(IDS_SYNC_NTP_SETUP_IN_PROGRESS);
    is_start_stop_button_visible = true;
    is_start_stop_button_sensitive = false;
  } else {
    start_stop_button_label =
        l10n_util::GetStringUTF8(IDS_SYNC_START_SYNC_BUTTON_LABEL);
    is_start_stop_button_visible = true;
    is_start_stop_button_sensitive = !managed;
  }
  gtk_widget_set_no_show_all(sync_start_stop_button_,
                             !is_start_stop_button_visible);
  if (is_start_stop_button_visible)
    gtk_widget_show(sync_start_stop_button_);
  else
    gtk_widget_hide(sync_start_stop_button_);
  gtk_widget_set_sensitive(sync_start_stop_button_,
                           is_start_stop_button_sensitive);
  gtk_button_set_label(GTK_BUTTON(sync_start_stop_button_),
                       start_stop_button_label.c_str());

  gtk_label_set_label(GTK_LABEL(sync_status_label_),
                      UTF16ToUTF8(status_label).c_str());

  gtk_widget_set_child_visible(sync_customize_button_,
      sync_setup_completed && !status_has_error);
  gtk_button_set_label(GTK_BUTTON(sync_customize_button_),
                       customize_button_label.c_str());
  gtk_widget_set_sensitive(sync_customize_button_, !managed);
#if !defined(OS_CHROMEOS)
  gtk_chrome_link_button_set_label(GTK_CHROME_LINK_BUTTON(sync_action_link_),
                                   UTF16ToUTF8(link_label).c_str());
  if (link_label.empty()) {
    gtk_widget_set_no_show_all(sync_action_link_background_, TRUE);
    gtk_widget_hide(sync_action_link_background_);
  } else {
    gtk_widget_set_no_show_all(sync_action_link_background_, FALSE);
    gtk_widget_show(sync_action_link_background_);
  }
  gtk_widget_set_sensitive(sync_action_link_, !managed);
#endif
  if (status_has_error) {
    gtk_widget_modify_bg(sync_status_label_background_, GTK_STATE_NORMAL,
                         &kSyncLabelErrorBgColor);
#if !defined(OS_CHROMEOS)
    gtk_widget_modify_bg(sync_action_link_background_, GTK_STATE_NORMAL,
                         &kSyncLabelErrorBgColor);
#endif
  } else {
    gtk_widget_modify_bg(sync_status_label_background_, GTK_STATE_NORMAL, NULL);
#if !defined(OS_CHROMEOS)
    gtk_widget_modify_bg(sync_action_link_background_, GTK_STATE_NORMAL, NULL);
#endif
  }
}

void ContentPageGtk::OnAutoFillButtonClicked(GtkWidget* widget) {
  ShowAutoFillDialog(NULL, profile()->GetPersonalDataManager(), profile());
}

void ContentPageGtk::OnImportButtonClicked(GtkWidget* widget) {
  ImportDialogGtk::Show(
      GTK_WINDOW(gtk_widget_get_toplevel(widget)),
      profile(), importer::ALL);
}

void ContentPageGtk::OnGtkThemeButtonClicked(GtkWidget* widget) {
  UserMetricsRecordAction(UserMetricsAction("Options_GtkThemeSet"),
                          profile()->GetPrefs());
  profile()->SetNativeTheme();
}

void ContentPageGtk::OnResetDefaultThemeButtonClicked(GtkWidget* widget) {
  UserMetricsRecordAction(UserMetricsAction("Options_ThemesReset"),
                          profile()->GetPrefs());
  profile()->ClearTheme();
}

void ContentPageGtk::OnGetThemesButtonClicked(GtkWidget* widget) {
  UserMetricsRecordAction(UserMetricsAction("Options_ThemesGallery"),
                          profile()->GetPrefs());
#if defined(OS_CHROMEOS)
  // Close options dialog for ChromeOS becuase it is always stacked on top
  // of browser window and blocks user's view.
  chromeos::CloseOptionsWindow();
#endif  // defined(OS_CHROMEOS)

  BrowserList::GetLastActive()->OpenThemeGalleryTabAndActivate();
}

void ContentPageGtk::OnSystemTitleBarRadioToggled(GtkWidget* widget) {
  DCHECK(browser_defaults::kCanToggleSystemTitleBar);
  if (initializing_)
    return;

  // We get two signals when selecting a radio button, one for the old radio
  // being toggled off and one for the new one being toggled on. Ignore the
  // signal for the toggling off the old button.
  if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
    return;

  bool use_custom = gtk_toggle_button_get_active(
      GTK_TOGGLE_BUTTON(system_title_bar_hide_radio_));
  if (use_custom) {
    UserMetricsRecordAction(UserMetricsAction("Options_CustomFrame_Enable"),
                            profile()->GetPrefs());
  } else {
    UserMetricsRecordAction(UserMetricsAction("Options_CustomFrame_Disable"),
                            profile()->GetPrefs());
  }

  use_custom_chrome_frame_.SetValue(use_custom);
}

void ContentPageGtk::OnShowPasswordsButtonClicked(GtkWidget* widget) {
  ShowPasswordsExceptionsWindow(profile());
}

void ContentPageGtk::OnPasswordRadioToggled(GtkWidget* widget) {
  if (initializing_)
    return;

  // We get two signals when selecting a radio button, one for the old radio
  // being toggled off and one for the new one being toggled on. Ignore the
  // signal for the toggling off the old button.
  if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
    return;

  bool enabled = gtk_toggle_button_get_active(
      GTK_TOGGLE_BUTTON(passwords_asktosave_radio_));
  if (enabled) {
    UserMetricsRecordAction(UserMetricsAction("Options_PasswordManager_Enable"),
                            profile()->GetPrefs());
  } else {
    UserMetricsRecordAction(
        UserMetricsAction("Options_PasswordManager_Disable"),
        profile()->GetPrefs());
  }
  ask_to_save_passwords_.SetValue(enabled);
}

void ContentPageGtk::OnSyncStartStopButtonClicked(GtkWidget* widget) {
  DCHECK(sync_service_ && !sync_service_->IsManaged());

  if (sync_service_->HasSyncSetupCompleted()) {
    GtkWidget* dialog = gtk_message_dialog_new(
        GTK_WINDOW(gtk_widget_get_toplevel(widget)),
        static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL),
        GTK_MESSAGE_WARNING,
        GTK_BUTTONS_NONE,
        "%s",
        l10n_util::GetStringFUTF8(
            IDS_SYNC_STOP_SYNCING_EXPLANATION_LABEL,
            l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)).c_str());
    gtk_util::ApplyMessageDialogQuirks(dialog);
    gtk_window_set_title(GTK_WINDOW(dialog),
                         l10n_util::GetStringUTF8(
                             IDS_SYNC_STOP_SYNCING_DIALOG_TITLE).c_str());
    gtk_dialog_add_buttons(
        GTK_DIALOG(dialog),
        l10n_util::GetStringUTF8(IDS_CANCEL).c_str(),
        GTK_RESPONSE_REJECT,
        l10n_util::GetStringUTF8(
            IDS_SYNC_STOP_SYNCING_CONFIRM_BUTTON_LABEL).c_str(),
        GTK_RESPONSE_ACCEPT,
        NULL);

    g_signal_connect(dialog, "response",
                     G_CALLBACK(OnStopSyncDialogResponseThunk), this);

    gtk_util::ShowDialog(dialog);
    return;
  } else {
    sync_service_->ShowLoginDialog(NULL);
    ProfileSyncService::SyncEvent(ProfileSyncService::START_FROM_OPTIONS);
  }
}

void ContentPageGtk::OnSyncCustomizeButtonClicked(GtkWidget* widget) {
  // sync_customize_button_ should be invisible if sync is not yet set up.
  DCHECK(sync_service_ && !sync_service_->IsManaged() &&
         sync_service_->HasSyncSetupCompleted());
  sync_service_->ShowConfigure(NULL);
}

void ContentPageGtk::OnSyncActionLinkClicked(GtkWidget* widget) {
  DCHECK(sync_service_ && !sync_service_->IsManaged());
  sync_service_->ShowErrorUI(NULL);
}

void ContentPageGtk::OnStopSyncDialogResponse(GtkWidget* widget, int response) {
  if (response == GTK_RESPONSE_ACCEPT) {
    sync_service_->DisableForUser();
    ProfileSyncService::SyncEvent(ProfileSyncService::STOP_FROM_OPTIONS);
  }
  gtk_widget_destroy(widget);
}

void ContentPageGtk::OnPrivacyDashboardLinkClicked(GtkWidget* widget) {
  BrowserList::GetLastActive()->OpenPrivacyDashboardTabAndActivate();
}
