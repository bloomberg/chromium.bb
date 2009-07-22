// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/options/advanced_contents_gtk.h"

#include <sys/types.h>
#include <sys/wait.h>

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "base/linux_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/fonts_languages_window.h"
#include "chrome/browser/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/gtk/options/options_layout_gtk.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/options_page_base.h"
#include "chrome/browser/options_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/pref_member.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/process_watcher.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/cookie_policy.h"

namespace {

// Command used to configure the gconf proxy settings.
const char kProxyConfigBinary[] = "gnome-network-preferences";

// The pixel width we wrap labels at.
// TODO(evanm): make the labels wrap at the appropriate width.
const int kWrapWidth = 475;

GtkWidget* CreateWrappedLabel(int string_id) {
  GtkWidget* label = gtk_label_new(
      l10n_util::GetStringUTF8(string_id).c_str());
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
  gtk_widget_set_size_request(label, kWrapWidth, -1);
  return label;
}

GtkWidget* CreateCheckButtonWithWrappedLabel(int string_id) {
  GtkWidget* checkbox = gtk_check_button_new();
  gtk_container_add(GTK_CONTAINER(checkbox),
                    CreateWrappedLabel(string_id));
  return checkbox;
}

}  // anonymous namespace


///////////////////////////////////////////////////////////////////////////////
// DownloadSection

class DownloadSection : public OptionsPageBase {
 public:
  explicit DownloadSection(Profile* profile);
  virtual ~DownloadSection() {}

  GtkWidget* get_page_widget() const {
    return page_;
  }

 private:
  // Overridden from OptionsPageBase.
  virtual void NotifyPrefChanged(const std::wstring* pref_name);

  // Callbacks for the widgets.
  static void OnDownloadLocationChanged(GtkFileChooser* widget,
                                        DownloadSection* section);
  static void OnDownloadAskForSaveLocationChanged(GtkWidget* widget,
                                                  DownloadSection* section);
  static void OnResetFileHandlersClicked(GtkButton *button,
                                         DownloadSection* section);

  // The widgets for the download options.
  GtkWidget* download_location_button_;
  GtkWidget* download_ask_for_save_location_checkbox_;
  GtkWidget* reset_file_handlers_label_;
  GtkWidget* reset_file_handlers_button_;

  // The widget containing the options for this section.
  GtkWidget* page_;

  // Pref members.
  StringPrefMember default_download_location_;
  BooleanPrefMember ask_for_save_location_;
  StringPrefMember auto_open_files_;

  // Flag to ignore gtk callbacks while we are loading prefs, to avoid
  // then turning around and saving them again.
  bool initializing_;

  DISALLOW_COPY_AND_ASSIGN(DownloadSection);
};

DownloadSection::DownloadSection(Profile* profile)
    : OptionsPageBase(profile), initializing_(true) {
  page_ = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  // Download location options.
  download_location_button_ = gtk_file_chooser_button_new(
      // TODO(mattm): There doesn't seem to be a reasonable localized string for
      // the chooser title?  (Though no other file choosers have a title either,
      // bug 16890.)
      "",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
  g_signal_connect(download_location_button_, "selection-changed",
                   G_CALLBACK(OnDownloadLocationChanged), this);
  // Add the default download path to the list of shortcuts in the selector.
  FilePath default_download_path;
  if (!PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS,
                        &default_download_path)) {
    NOTREACHED();
  } else {
    if (!gtk_file_chooser_add_shortcut_folder(
        GTK_FILE_CHOOSER(download_location_button_),
        default_download_path.value().c_str(),
        NULL)) {
      NOTREACHED();
    }
  }

  GtkWidget* download_location_control = gtk_util::CreateLabeledControlsGroup(
      NULL,
      l10n_util::GetStringUTF8(
          IDS_OPTIONS_DOWNLOADLOCATION_BROWSE_TITLE).c_str(),
      download_location_button_,
      NULL);
  gtk_box_pack_start(GTK_BOX(page_), download_location_control,
                     FALSE, FALSE, 0);

  download_ask_for_save_location_checkbox_ = CreateCheckButtonWithWrappedLabel(
      IDS_OPTIONS_DOWNLOADLOCATION_ASKFORSAVELOCATION);
  gtk_box_pack_start(GTK_BOX(page_), download_ask_for_save_location_checkbox_,
                     FALSE, FALSE, 0);
  g_signal_connect(download_ask_for_save_location_checkbox_, "clicked",
                   G_CALLBACK(OnDownloadAskForSaveLocationChanged), this);

  // Option for resetting file handlers.
  reset_file_handlers_label_ = CreateWrappedLabel(
      IDS_OPTIONS_AUTOOPENFILETYPES_INFO);
  gtk_misc_set_alignment(GTK_MISC(reset_file_handlers_label_), 0, 0);
  gtk_box_pack_start(GTK_BOX(page_), reset_file_handlers_label_,
                     FALSE, FALSE, 0);

  reset_file_handlers_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(
          IDS_OPTIONS_AUTOOPENFILETYPES_RESETTODEFAULT).c_str());
  g_signal_connect(reset_file_handlers_button_, "clicked",
                   G_CALLBACK(OnResetFileHandlersClicked), this);
  // Stick it in an hbox so it doesn't expand to the whole width.
  GtkWidget* button_hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(button_hbox),
                     reset_file_handlers_button_,
                     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(page_),
                     OptionsLayoutBuilderGtk::IndentWidget(button_hbox),
                     FALSE, FALSE, 0);

  // Init prefs watchers.
  default_download_location_.Init(prefs::kDownloadDefaultDirectory,
                                  profile->GetPrefs(), this);
  ask_for_save_location_.Init(prefs::kPromptForDownload,
                              profile->GetPrefs(), this);
  auto_open_files_.Init(prefs::kDownloadExtensionsToOpen, profile->GetPrefs(),
                        this);

  NotifyPrefChanged(NULL);
}

void DownloadSection::NotifyPrefChanged(const std::wstring* pref_name) {
  initializing_ = true;
  if (!pref_name || *pref_name == prefs::kDownloadDefaultDirectory) {
    gtk_file_chooser_set_current_folder(
        GTK_FILE_CHOOSER(download_location_button_),
        FilePath::FromWStringHack(
            default_download_location_.GetValue()).value().c_str());
  }

  if (!pref_name || *pref_name == prefs::kPromptForDownload) {
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(download_ask_for_save_location_checkbox_),
        ask_for_save_location_.GetValue());
  }

  if (!pref_name || *pref_name == prefs::kDownloadExtensionsToOpen) {
    bool enabled =
        profile()->GetDownloadManager()->HasAutoOpenFileTypesRegistered();
    gtk_widget_set_sensitive(reset_file_handlers_label_, enabled);
    gtk_widget_set_sensitive(reset_file_handlers_button_, enabled);
  }
  initializing_ = false;
}

// static
void DownloadSection::OnDownloadLocationChanged(GtkFileChooser* widget,
                                                DownloadSection* section) {
  if (section->initializing_)
    return;

  gchar* folder = gtk_file_chooser_get_filename(widget);
  FilePath path(folder);
  g_free(folder);
  // Gtk seems to call this signal multiple times, so we only set the pref and
  // metric if something actually changed.
  if (path.ToWStringHack() != section->default_download_location_.GetValue()) {
    section->default_download_location_.SetValue(path.ToWStringHack());
    section->UserMetricsRecordAction(L"Options_SetDownloadDirectory",
                                     section->profile()->GetPrefs());
  }
}

// static
void DownloadSection::OnDownloadAskForSaveLocationChanged(
    GtkWidget* widget, DownloadSection* section) {
  if (section->initializing_)
    return;
  bool enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  if (enabled) {
    section->UserMetricsRecordAction(L"Options_AskForSaveLocation_Enable",
                                     section->profile()->GetPrefs());
  } else {
    section->UserMetricsRecordAction(L"Options_AskForSaveLocation_Disable",
                                     section->profile()->GetPrefs());
  }
  section->ask_for_save_location_.SetValue(enabled);
}

// static
void DownloadSection::OnResetFileHandlersClicked(GtkButton *button,
                                                 DownloadSection* section) {
  section->profile()->GetDownloadManager()->ResetAutoOpenFiles();
  section->UserMetricsRecordAction(L"Options_ResetAutoOpenFiles",
                                   section->profile()->GetPrefs());
}

///////////////////////////////////////////////////////////////////////////////
// NetworkSection

class NetworkSection : public OptionsPageBase {
 public:
  explicit NetworkSection(Profile* profile);
  virtual ~NetworkSection() {}

  GtkWidget* get_page_widget() const {
    return page_;
  }

 private:
  // The callback functions for invoking the proxy config dialog.
  static void OnChangeProxiesButtonClicked(GtkButton *button,
                                           NetworkSection* section);

  // The widget containing the options for this section.
  GtkWidget* page_;

  DISALLOW_COPY_AND_ASSIGN(NetworkSection);
};

NetworkSection::NetworkSection(Profile* profile)
    : OptionsPageBase(profile) {
  page_ = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  GtkWidget* proxy_description_label = CreateWrappedLabel(
      IDS_OPTIONS_PROXIES_LABEL);
  gtk_misc_set_alignment(GTK_MISC(proxy_description_label), 0, 0);
  gtk_box_pack_start(GTK_BOX(page_), proxy_description_label,
                     FALSE, FALSE, 0);

  GtkWidget* change_proxies_button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(
          IDS_OPTIONS_PROXIES_CONFIGURE_BUTTON).c_str());
  g_signal_connect(change_proxies_button, "clicked",
                   G_CALLBACK(OnChangeProxiesButtonClicked), this);
  // Stick it in an hbox so it doesn't expand to the whole width.
  GtkWidget* button_hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(button_hbox),
                     change_proxies_button,
                     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(page_),
                     OptionsLayoutBuilderGtk::IndentWidget(button_hbox),
                     FALSE, FALSE, 0);
}

// static
void NetworkSection::OnChangeProxiesButtonClicked(GtkButton *button,
                                                  NetworkSection* section) {
  section->UserMetricsRecordAction(L"Options_ChangeProxies", NULL);

  scoped_ptr<base::EnvironmentVariableGetter> env_getter(
      base::EnvironmentVariableGetter::Create());
  if (base::UseGnomeForSettings(env_getter.get())) {
    std::vector<std::string> argv;
    argv.push_back(kProxyConfigBinary);
    base::file_handle_mapping_vector no_files;
    base::environment_vector env;
    base::ProcessHandle handle;
    env.push_back(std::make_pair("GTK_PATH",
                                 getenv("CHROMIUM_SAVED_GTK_PATH")));
    if (!base::LaunchApp(argv, env, no_files, false, &handle)) {
      LOG(ERROR) << "OpenProxyConfigDialogTask failed";
      return;
    }
    ProcessWatcher::EnsureProcessGetsReaped(handle);
  } else {
    BrowserList::GetLastActive()->
        OpenURL(GURL(l10n_util::GetStringUTF8(IDS_LINUX_PROXY_CONFIG_URL)),
                GURL(), NEW_FOREGROUND_TAB, PageTransition::LINK);
  }
}

///////////////////////////////////////////////////////////////////////////////
// PrivacySection

class PrivacySection : public OptionsPageBase {
 public:
  explicit PrivacySection(Profile* profile);
  virtual ~PrivacySection() {}

  GtkWidget* get_page_widget() const {
    return page_;
  }

 private:
  // Overridden from OptionsPageBase.
  virtual void NotifyPrefChanged(const std::wstring* pref_name);

  // Try to make the the crash stats consent and the metrics upload
  // permission match the |reporting_enabled_checkbox_|.
  void ResolveMetricsReportingEnabled();

  // Inform the user that the browser must be restarted for changes to take
  // effect.
  void ShowRestartMessageBox() const;

  // The callback functions for the options widgets.
  static void OnLearnMoreLinkClicked(GtkButton *button,
                                     PrivacySection* privacy_section);
  static void OnEnableLinkDoctorChange(GtkWidget* widget,
                                       PrivacySection* options_window);
  static void OnEnableSuggestChange(GtkWidget* widget,
                                    PrivacySection* options_window);
  static void OnDNSPrefetchingChange(GtkWidget* widget,
                                     PrivacySection* options_window);
  static void OnSafeBrowsingChange(GtkWidget* widget,
                                   PrivacySection* options_window);
  static void OnLoggingChange(GtkWidget* widget,
                              PrivacySection* options_window);
  static void OnCookieBehaviorChanged(GtkComboBox* combo_box,
                                      PrivacySection* privacy_section);

  // The widget containing the options for this section.
  GtkWidget* page_;

  // The widgets for the privacy options.
  GtkWidget* enable_link_doctor_checkbox_;
  GtkWidget* enable_suggest_checkbox_;
  GtkWidget* enable_dns_prefetching_checkbox_;
  GtkWidget* enable_safe_browsing_checkbox_;
#if defined(GOOGLE_CHROME_BUILD)
  GtkWidget* reporting_enabled_checkbox_;
#endif
  GtkWidget* cookie_behavior_combobox_;

  // Preferences for this section:
  BooleanPrefMember alternate_error_pages_;
  BooleanPrefMember use_suggest_;
  BooleanPrefMember dns_prefetch_enabled_;
  BooleanPrefMember safe_browsing_;
  BooleanPrefMember enable_metrics_recording_;
  IntegerPrefMember cookie_behavior_;

  // Flag to ignore gtk callbacks while we are loading prefs, to avoid
  // then turning around and saving them again.
  bool initializing_;

  DISALLOW_COPY_AND_ASSIGN(PrivacySection);
};

PrivacySection::PrivacySection(Profile* profile)
    : OptionsPageBase(profile),
      initializing_(true) {
  page_ = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  GtkWidget* section_description_label = CreateWrappedLabel(
      IDS_OPTIONS_DISABLE_SERVICES);
  gtk_misc_set_alignment(GTK_MISC(section_description_label), 0, 0);
  gtk_box_pack_start(GTK_BOX(page_), section_description_label,
                     FALSE, FALSE, 0);

  GtkWidget* learn_more_link = gtk_chrome_link_button_new(
      l10n_util::GetStringUTF8(IDS_LEARN_MORE).c_str());
  // Stick it in an hbox so it doesn't expand to the whole width.
  GtkWidget* learn_more_hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(learn_more_hbox), learn_more_link,
                     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(page_), learn_more_hbox,
                     FALSE, FALSE, 0);
  g_signal_connect(learn_more_link, "clicked",
                   G_CALLBACK(OnLearnMoreLinkClicked), this);

  enable_link_doctor_checkbox_ = CreateCheckButtonWithWrappedLabel(
      IDS_OPTIONS_LINKDOCTOR_PREF);
  gtk_box_pack_start(GTK_BOX(page_), enable_link_doctor_checkbox_,
                     FALSE, FALSE, 0);
  g_signal_connect(enable_link_doctor_checkbox_, "clicked",
                   G_CALLBACK(OnEnableLinkDoctorChange), this);

  enable_suggest_checkbox_ = CreateCheckButtonWithWrappedLabel(
      IDS_OPTIONS_SUGGEST_PREF);
  gtk_box_pack_start(GTK_BOX(page_), enable_suggest_checkbox_,
                     FALSE, FALSE, 0);
  g_signal_connect(enable_suggest_checkbox_, "clicked",
                   G_CALLBACK(OnEnableSuggestChange), this);

  enable_dns_prefetching_checkbox_ = CreateCheckButtonWithWrappedLabel(
      IDS_NETWORK_DNS_PREFETCH_ENABLED_DESCRIPTION);
  gtk_box_pack_start(GTK_BOX(page_), enable_dns_prefetching_checkbox_,
                     FALSE, FALSE, 0);
  g_signal_connect(enable_dns_prefetching_checkbox_, "clicked",
                   G_CALLBACK(OnDNSPrefetchingChange), this);

  enable_safe_browsing_checkbox_ = CreateCheckButtonWithWrappedLabel(
      IDS_OPTIONS_SAFEBROWSING_ENABLEPROTECTION);
  gtk_box_pack_start(GTK_BOX(page_), enable_safe_browsing_checkbox_,
                     FALSE, FALSE, 0);
  g_signal_connect(enable_safe_browsing_checkbox_, "clicked",
                   G_CALLBACK(OnSafeBrowsingChange), this);

#if defined(GOOGLE_CHROME_BUILD)
  reporting_enabled_checkbox_ = CreateCheckButtonWithWrappedLabel(
      IDS_OPTIONS_ENABLE_LOGGING);
  gtk_box_pack_start(GTK_BOX(page_), reporting_enabled_checkbox_,
                     FALSE, FALSE, 0);
  g_signal_connect(reporting_enabled_checkbox_, "clicked",
                   G_CALLBACK(OnLoggingChange), this);
#endif

  cookie_behavior_combobox_ = gtk_combo_box_new_text();
  gtk_combo_box_append_text(
      GTK_COMBO_BOX(cookie_behavior_combobox_),
      l10n_util::GetStringUTF8(IDS_OPTIONS_COOKIES_ACCEPT_ALL_COOKIES).c_str());
  gtk_combo_box_append_text(
      GTK_COMBO_BOX(cookie_behavior_combobox_),
      l10n_util::GetStringUTF8(
          IDS_OPTIONS_COOKIES_RESTRICT_THIRD_PARTY_COOKIES).c_str());
  gtk_combo_box_append_text(
      GTK_COMBO_BOX(cookie_behavior_combobox_),
      l10n_util::GetStringUTF8(IDS_OPTIONS_COOKIES_BLOCK_ALL_COOKIES).c_str());
  g_signal_connect(G_OBJECT(cookie_behavior_combobox_), "changed",
                   G_CALLBACK(OnCookieBehaviorChanged), this);

  GtkWidget* cookie_controls = gtk_util::CreateLabeledControlsGroup(NULL,
      l10n_util::GetStringUTF8(IDS_OPTIONS_COOKIES_ACCEPT_LABEL).c_str(),
      cookie_behavior_combobox_,
      NULL);
  gtk_box_pack_start(GTK_BOX(page_), cookie_controls, FALSE, FALSE, 0);

  // TODO(mattm): show cookies button
  gtk_box_pack_start(GTK_BOX(page_),
                     gtk_label_new("TODO rest of the privacy options"),
                     FALSE, FALSE, 0);

  // Init member prefs so we can update the controls if prefs change.
  alternate_error_pages_.Init(prefs::kAlternateErrorPagesEnabled,
                              profile->GetPrefs(), this);
  use_suggest_.Init(prefs::kSearchSuggestEnabled,
                    profile->GetPrefs(), this);
  dns_prefetch_enabled_.Init(prefs::kDnsPrefetchingEnabled,
                             profile->GetPrefs(), this);
  safe_browsing_.Init(prefs::kSafeBrowsingEnabled, profile->GetPrefs(), this);
  enable_metrics_recording_.Init(prefs::kMetricsReportingEnabled,
                                 g_browser_process->local_state(), this);
  cookie_behavior_.Init(prefs::kCookieBehavior, profile->GetPrefs(), this);

  NotifyPrefChanged(NULL);
}

// static
void PrivacySection::OnLearnMoreLinkClicked(GtkButton *button,
                                            PrivacySection* privacy_section) {
  BrowserList::GetLastActive()->
      OpenURL(GURL(l10n_util::GetStringUTF8(IDS_LEARN_MORE_PRIVACY_URL)),
              GURL(), NEW_WINDOW, PageTransition::LINK);
}

// static
void PrivacySection::OnEnableLinkDoctorChange(GtkWidget* widget,
                                              PrivacySection* privacy_section) {
  if (privacy_section->initializing_)
    return;
  bool enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  privacy_section->UserMetricsRecordAction(
      enabled ?
      L"Options_LinkDoctorCheckbox_Enable" :
      L"Options_LinkDoctorCheckbox_Disable",
      privacy_section->profile()->GetPrefs());
  privacy_section->alternate_error_pages_.SetValue(enabled);
}

// static
void PrivacySection::OnEnableSuggestChange(GtkWidget* widget,
                                           PrivacySection* privacy_section) {
  if (privacy_section->initializing_)
    return;
  bool enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  privacy_section->UserMetricsRecordAction(
      enabled ?
      L"Options_UseSuggestCheckbox_Enable" :
      L"Options_UseSuggestCheckbox_Disable",
      privacy_section->profile()->GetPrefs());
  privacy_section->use_suggest_.SetValue(enabled);
}

// static
void PrivacySection::OnDNSPrefetchingChange(GtkWidget* widget,
                                           PrivacySection* privacy_section) {
  if (privacy_section->initializing_)
    return;
  bool enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  privacy_section->UserMetricsRecordAction(
      enabled ?
      L"Options_DnsPrefetchCheckbox_Enable" :
      L"Options_DnsPrefetchCheckbox_Disable",
      privacy_section->profile()->GetPrefs());
  privacy_section->dns_prefetch_enabled_.SetValue(enabled);
  chrome_browser_net::EnableDnsPrefetch(enabled);
}

// static
void PrivacySection::OnSafeBrowsingChange(GtkWidget* widget,
                                          PrivacySection* privacy_section) {
  if (privacy_section->initializing_)
    return;
  bool enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  privacy_section->UserMetricsRecordAction(
      enabled ?
      L"Options_SafeBrowsingCheckbox_Enable" :
      L"Options_SafeBrowsingCheckbox_Disable",
      privacy_section->profile()->GetPrefs());
  privacy_section->safe_browsing_.SetValue(enabled);
  SafeBrowsingService* safe_browsing_service =
      g_browser_process->resource_dispatcher_host()->safe_browsing_service();
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      safe_browsing_service, &SafeBrowsingService::OnEnable, enabled));
}

// static
void PrivacySection::OnLoggingChange(GtkWidget* widget,
                                     PrivacySection* privacy_section) {
  if (privacy_section->initializing_)
    return;
  bool enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  privacy_section->UserMetricsRecordAction(
      enabled ?
      L"Options_MetricsReportingCheckbox_Enable" :
      L"Options_MetricsReportingCheckbox_Disable",
      privacy_section->profile()->GetPrefs());
  // Prevent us from being called again by ResolveMetricsReportingEnabled
  // resetting the checkbox if there was a problem.
  g_signal_handlers_block_by_func(widget,
                                  reinterpret_cast<gpointer>(OnLoggingChange),
                                  privacy_section);
  privacy_section->ResolveMetricsReportingEnabled();
  if (enabled == gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
    privacy_section->ShowRestartMessageBox();
  g_signal_handlers_unblock_by_func(widget,
                                    reinterpret_cast<gpointer>(OnLoggingChange),
                                    privacy_section);
  privacy_section->enable_metrics_recording_.SetValue(enabled);
}

// static
void PrivacySection::OnCookieBehaviorChanged(GtkComboBox* combo_box,
                                             PrivacySection* privacy_section) {
  if (privacy_section->initializing_)
    return;
  net::CookiePolicy::Type cookie_policy =
      net::CookiePolicy::FromInt(gtk_combo_box_get_active(combo_box));
  const wchar_t* kUserMetrics[] = {
      L"Options_AllowAllCookies",
      L"Options_BlockThirdPartyCookies",
      L"Options_BlockAllCookies"
  };
  if (cookie_policy < 0 ||
      static_cast<size_t>(cookie_policy) >= arraysize(kUserMetrics)) {
    NOTREACHED();
    return;
  }
  privacy_section->UserMetricsRecordAction(
      kUserMetrics[cookie_policy], privacy_section->profile()->GetPrefs());
  privacy_section->cookie_behavior_.SetValue(cookie_policy);
}

void PrivacySection::NotifyPrefChanged(const std::wstring* pref_name) {
  initializing_ = true;
  if (!pref_name || *pref_name == prefs::kAlternateErrorPagesEnabled) {
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(enable_link_doctor_checkbox_),
        alternate_error_pages_.GetValue());
  }
  if (!pref_name || *pref_name == prefs::kSearchSuggestEnabled) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_suggest_checkbox_),
                                 use_suggest_.GetValue());
  }
  if (!pref_name || *pref_name == prefs::kDnsPrefetchingEnabled) {
    bool enabled = dns_prefetch_enabled_.GetValue();
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(enable_dns_prefetching_checkbox_), enabled);
    chrome_browser_net::EnableDnsPrefetch(enabled);
  }
  if (!pref_name || *pref_name == prefs::kSafeBrowsingEnabled) {
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(enable_safe_browsing_checkbox_),
        safe_browsing_.GetValue());
  }
#if defined(GOOGLE_CHROME_BUILD)
  if (!pref_name || *pref_name == prefs::kMetricsReportingEnabled) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(reporting_enabled_checkbox_),
                                 enable_metrics_recording_.GetValue());
    ResolveMetricsReportingEnabled();
  }
#endif
  if (!pref_name || *pref_name == prefs::kCookieBehavior) {
    gtk_combo_box_set_active(
        GTK_COMBO_BOX(cookie_behavior_combobox_),
        net::CookiePolicy::FromInt(cookie_behavior_.GetValue()));
  }
  initializing_ = false;
}

void PrivacySection::ResolveMetricsReportingEnabled() {
#if defined(GOOGLE_CHROME_BUILD)
  bool enabled = gtk_toggle_button_get_active(
      GTK_TOGGLE_BUTTON(reporting_enabled_checkbox_));

  enabled = OptionsUtil::ResolveMetricsReportingEnabled(enabled);

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(reporting_enabled_checkbox_),
                               enabled);
#endif
}

void PrivacySection::ShowRestartMessageBox() const {
  GtkWidget* dialog = gtk_message_dialog_new(
      GTK_WINDOW(gtk_widget_get_toplevel(page_)),
      static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL),
      GTK_MESSAGE_INFO,
      GTK_BUTTONS_OK,
      "%s",
      l10n_util::GetStringUTF8(IDS_OPTIONS_RESTART_REQUIRED).c_str());
  gtk_window_set_title(GTK_WINDOW(dialog),
      l10n_util::GetStringUTF8(IDS_PRODUCT_NAME).c_str());
  g_signal_connect_swapped(dialog, "response", G_CALLBACK(gtk_widget_destroy),
                           dialog);
  gtk_widget_show_all(dialog);
}

///////////////////////////////////////////////////////////////////////////////
// SecuritySection

class SecuritySection : public OptionsPageBase {
 public:
  explicit SecuritySection(Profile* profile);
  virtual ~SecuritySection() {}

  GtkWidget* get_page_widget() const {
    return page_;
  }

 private:
  // The callback functions for the options widgets.
  static void OnManageCertificatesClicked(GtkButton* button,
                                          SecuritySection* section);

  // The widget containing the options for this section.
  GtkWidget* page_;

  DISALLOW_COPY_AND_ASSIGN(SecuritySection);
};

SecuritySection::SecuritySection(Profile* profile)
    : OptionsPageBase(profile) {
  page_ = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  GtkWidget* manage_certificates_label = CreateWrappedLabel(
      IDS_OPTIONS_CERTIFICATES_LABEL);
  gtk_misc_set_alignment(GTK_MISC(manage_certificates_label), 0, 0);
  gtk_box_pack_start(GTK_BOX(page_), manage_certificates_label,
                     FALSE, FALSE, 0);

  // TODO(mattm): change this to a button to launch the system certificate
  // manager, when one exists.
  GtkWidget* manage_certificates_link = gtk_chrome_link_button_new(
      l10n_util::GetStringUTF8(IDS_OPTIONS_CERTIFICATES_MANAGE_BUTTON).c_str());
  // Stick it in an hbox so it doesn't expand to the whole width.
  GtkWidget* manage_certificates_hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(manage_certificates_hbox),
                     manage_certificates_link, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(page_), OptionsLayoutBuilderGtk::IndentWidget(
                         manage_certificates_hbox),
                     FALSE, FALSE, 0);
  g_signal_connect(manage_certificates_link, "clicked",
                   G_CALLBACK(OnManageCertificatesClicked), this);

  // TODO(mattm): add SSLConfigService options when that is ported to Linux
}

// static
void SecuritySection::OnManageCertificatesClicked(GtkButton* button,
                                                  SecuritySection* section) {
  BrowserList::GetLastActive()->
      OpenURL(GURL(l10n_util::GetStringUTF8(IDS_LINUX_CERTIFICATES_CONFIG_URL)),
              GURL(), NEW_WINDOW, PageTransition::LINK);
}

///////////////////////////////////////////////////////////////////////////////
// WebContentSection

class WebContentSection : public OptionsPageBase {
 public:
  explicit WebContentSection(Profile* profile);
  virtual ~WebContentSection() {}

  GtkWidget* get_page_widget() const {
    return page_;
  }

 private:
  // The callback functions for the options widgets.
  static void OnFontsAndLanguagesButtonClicked(GtkButton *button,
                                               WebContentSection* section);

  // The widget containing the options for this section.
  GtkWidget* page_;

  DISALLOW_COPY_AND_ASSIGN(WebContentSection);
};

WebContentSection::WebContentSection(Profile* profile)
    : OptionsPageBase(profile) {
  page_ = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  GtkWidget* fonts_and_languages_label = CreateWrappedLabel(
      IDS_OPTIONS_FONTSETTINGS_INFO);
  gtk_misc_set_alignment(GTK_MISC(fonts_and_languages_label), 0, 0);
  gtk_box_pack_start(GTK_BOX(page_), fonts_and_languages_label,
                     FALSE, FALSE, 0);

  GtkWidget* fonts_and_languages_button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(
          IDS_OPTIONS_FONTSETTINGS_CONFIGUREFONTS_BUTTON).c_str());
  g_signal_connect(fonts_and_languages_button, "clicked",
                   G_CALLBACK(OnFontsAndLanguagesButtonClicked), this);
  // Stick it in an hbox so it doesn't expand to the whole width.
  GtkWidget* button_hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(button_hbox),
                     fonts_and_languages_button,
                     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(page_),
                     OptionsLayoutBuilderGtk::IndentWidget(button_hbox),
                     FALSE, FALSE, 0);

  // TODO(mattm): gears options would go here if we supported gears
}

// static
void WebContentSection::OnFontsAndLanguagesButtonClicked(
    GtkButton *button, WebContentSection* section) {
  ShowFontsLanguagesWindow(GTK_WINDOW(gtk_widget_get_toplevel(section->page_)),
                           FONTS_ENCODING_PAGE,
                           section->profile());
}

///////////////////////////////////////////////////////////////////////////////
// AdvancedContentsGtk

AdvancedContentsGtk::AdvancedContentsGtk(Profile* profile)
    : profile_(profile) {
  Init();
}

AdvancedContentsGtk::~AdvancedContentsGtk() {
}

void AdvancedContentsGtk::Init() {
  OptionsLayoutBuilderGtk options_builder;

  network_section_.reset(new NetworkSection(profile_));
  options_builder.AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_OPTIONS_ADVANCED_SECTION_TITLE_NETWORK),
      network_section_->get_page_widget(), false);

  privacy_section_.reset(new PrivacySection(profile_));
  options_builder.AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_OPTIONS_ADVANCED_SECTION_TITLE_PRIVACY),
      privacy_section_->get_page_widget(), false);

  download_section_.reset(new DownloadSection(profile_));
  options_builder.AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_OPTIONS_DOWNLOADLOCATION_GROUP_NAME),
      download_section_->get_page_widget(), false);

  web_content_section_.reset(new WebContentSection(profile_));
  options_builder.AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_OPTIONS_ADVANCED_SECTION_TITLE_CONTENT),
      web_content_section_->get_page_widget(), false);

  security_section_.reset(new SecuritySection(profile_));
  options_builder.AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_OPTIONS_ADVANCED_SECTION_TITLE_SECURITY),
      security_section_->get_page_widget(), false);

  page_ = options_builder.get_page_widget();
}
