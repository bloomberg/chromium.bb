// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/options/advanced_options_handler.h"

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/dom_ui/options/options_managed_banner_handler.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/options_util.h"
#include "chrome/browser/options_window.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service.h"
#include "chrome/browser/printing/cloud_print/cloud_print_setup_flow.h"
#include "chrome/browser/printing/cloud_print/cloud_print_url.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/chrome_switches.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/dom_ui/options/advanced_options_utils.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/gears_integration.h"
#include "net/base/ssl_config_service_win.h"
#endif

AdvancedOptionsHandler::AdvancedOptionsHandler() {
}

AdvancedOptionsHandler::~AdvancedOptionsHandler() {
}

void AdvancedOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  localized_strings->SetString("privacyLearnMoreURL",
      l10n_util::GetStringUTF16(IDS_LEARN_MORE_PRIVACY_URL));
  localized_strings->SetString("privacyLearnMoreLabel",
      l10n_util::GetStringUTF16(IDS_OPTIONS_LEARN_MORE_LABEL));
  localized_strings->SetString("downloadLocationGroupName",
      l10n_util::GetStringUTF16(IDS_OPTIONS_DOWNLOADLOCATION_GROUP_NAME));
  localized_strings->SetString("downloadLocationBrowseButton",
      l10n_util::GetStringUTF16(IDS_OPTIONS_DOWNLOADLOCATION_BROWSE_BUTTON));
  localized_strings->SetString("downloadLocationBrowseTitle",
      l10n_util::GetStringUTF16(IDS_OPTIONS_DOWNLOADLOCATION_BROWSE_TITLE));
  localized_strings->SetString("downloadLocationBrowseWindowTitle",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_DOWNLOADLOCATION_BROWSE_WINDOW_TITLE));
  localized_strings->SetString("downloadLocationAskForSaveLocation",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_DOWNLOADLOCATION_ASKFORSAVELOCATION));
  localized_strings->SetString("autoOpenFileTypesInfo",
      l10n_util::GetStringUTF16(IDS_OPTIONS_AUTOOPENFILETYPES_INFO));
  localized_strings->SetString("autoOpenFileTypesResetToDefault",
      l10n_util::GetStringUTF16(IDS_OPTIONS_AUTOOPENFILETYPES_RESETTODEFAULT));
  localized_strings->SetString("gearSettingsGroupName",
      l10n_util::GetStringUTF16(IDS_OPTIONS_GEARSSETTINGS_GROUP_NAME));
  localized_strings->SetString("gearSettingsConfigureGearsButton",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_GEARSSETTINGS_CONFIGUREGEARS_BUTTON));
  localized_strings->SetString("translateEnableTranslate",
      l10n_util::GetStringUTF16(IDS_OPTIONS_TRANSLATE_ENABLE_TRANSLATE));
  localized_strings->SetString("certificatesLabel",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CERTIFICATES_LABEL));
  localized_strings->SetString("certificatesManageButton",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CERTIFICATES_MANAGE_BUTTON));
  localized_strings->SetString("proxiesLabel",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PROXIES_LABEL));
  localized_strings->SetString("proxiesConfigureButton",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PROXIES_CONFIGURE_BUTTON));
  localized_strings->SetString("safeBrowsingEnableProtection",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SAFEBROWSING_ENABLEPROTECTION));
  localized_strings->SetString("sslGroupDescription",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SSL_GROUP_DESCRIPTION));
  localized_strings->SetString("sslUseSSL2",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SSL_USESSL2));
  localized_strings->SetString("sslCheckRevocation",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SSL_CHECKREVOCATION));
  localized_strings->SetString("sslUseSSL3",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SSL_USESSL3));
  localized_strings->SetString("sslUseTLS1",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SSL_USETLS1));
  localized_strings->SetString("networkDNSPrefetchEnabledDescription",
      l10n_util::GetStringUTF16(IDS_NETWORK_DNS_PREFETCH_ENABLED_DESCRIPTION));
  localized_strings->SetString("privacyContentSettingsButton",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PRIVACY_CONTENT_SETTINGS_BUTTON));
  localized_strings->SetString("privacyClearDataButton",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PRIVACY_CLEAR_DATA_BUTTON));
  localized_strings->SetString("linkDoctorPref",
      l10n_util::GetStringUTF16(IDS_OPTIONS_LINKDOCTOR_PREF));
  localized_strings->SetString("suggestPref",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SUGGEST_PREF));
  localized_strings->SetString("tabsToLinksPref",
      l10n_util::GetStringUTF16(IDS_OPTIONS_TABS_TO_LINKS_PREF));
  localized_strings->SetString("fontSettingsInfo",
      l10n_util::GetStringUTF16(IDS_OPTIONS_FONTSETTINGS_INFO));
  localized_strings->SetString("fontSettingsConfigureFontsOnlyButton",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_FONTSETTINGS_CONFIGUREFONTSONLY_BUTTON));
  localized_strings->SetString("advancedSectionTitlePrivacy",
      l10n_util::GetStringUTF16(IDS_OPTIONS_ADVANCED_SECTION_TITLE_PRIVACY));
  localized_strings->SetString("advancedSectionTitleContent",
      l10n_util::GetStringUTF16(IDS_OPTIONS_ADVANCED_SECTION_TITLE_CONTENT));
  localized_strings->SetString("advancedSectionTitleSecurity",
      l10n_util::GetStringUTF16(IDS_OPTIONS_ADVANCED_SECTION_TITLE_SECURITY));
  localized_strings->SetString("advancedSectionTitleNetwork",
      l10n_util::GetStringUTF16(IDS_OPTIONS_ADVANCED_SECTION_TITLE_NETWORK));
  localized_strings->SetString("advancedSectionTitleTranslate",
      l10n_util::GetStringUTF16(IDS_OPTIONS_ADVANCED_SECTION_TITLE_TRANSLATE));
  localized_strings->SetString("translateEnableTranslate",
      l10n_util::GetStringUTF16(IDS_OPTIONS_TRANSLATE_ENABLE_TRANSLATE));
  // Add ChromeApps preferences if background mode is runtime-enabled.
  bool background_mode_enabled = CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBackgroundMode);
  localized_strings->SetString("enable-background-mode",
      background_mode_enabled ? "true" : "false");
  localized_strings->SetString("advancedSectionTitleChromeApps",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_ADVANCED_SECTION_TITLE_CHROME_APPS));
  localized_strings->SetString("chromeAppsEnableBackgroundMode",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_CHROME_APPS_ENABLE_BACKGROUND_MODE));
  localized_strings->SetString("chromeAppsLearnMoreBackgroundModeLabel",
      l10n_util::GetStringUTF16(IDS_OPTIONS_LEARN_MORE_LABEL));
  localized_strings->SetString("chromeAppsLearnMoreBackgroundModeURL",
      l10n_util::GetStringUTF16(IDS_LEARN_MORE_BACKGROUND_MODE_URL));
#if !defined(OS_CHROMEOS)
  // Add the cloud print proxy management ui section if it's been runtime
  // enabled.
  bool cloud_print_enabled = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableCloudPrintProxy);
  localized_strings->SetString("enable-cloud-print-proxy",
                               cloud_print_enabled ? "true" : "false");
  localized_strings->SetString("advancedSectionTitleCloudPrint",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_ADVANCED_SECTION_TITLE_CLOUD_PRINT));
  localized_strings->SetString("cloudPrintProxyDisabledLabel",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CLOUD_PRINT_PROXY_DISABLED_LABEL));
  localized_strings->SetString("cloudPrintProxyDisabledButton",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CLOUD_PRINT_PROXY_DISABLED_BUTTON));
  localized_strings->SetString("cloudPrintProxyEnabledButton",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CLOUD_PRINT_PROXY_ENABLED_BUTTON));
  localized_strings->SetString("cloudPrintProxyEnabledManageButton",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_CLOUD_PRINT_PROXY_ENABLED_MANAGE_BUTTON));
  localized_strings->SetString("cloudPrintProxyEnablingButton",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CLOUD_PRINT_PROXY_ENABLING_BUTTON));
#endif
  localized_strings->SetString("enableLogging",
      l10n_util::GetStringUTF16(IDS_OPTIONS_ENABLE_LOGGING));
  localized_strings->SetString("disableServices",
      l10n_util::GetStringUTF16(IDS_OPTIONS_DISABLE_SERVICES));
  localized_strings->SetString("optionsReset",
      l10n_util::GetStringUTF16(IDS_OPTIONS_RESET));
  localized_strings->SetString("optionsResetMessage",
      l10n_util::GetStringUTF16(IDS_OPTIONS_RESET_MESSAGE));
  localized_strings->SetString("optionsResetOkLabel",
      l10n_util::GetStringUTF16(IDS_OPTIONS_RESET_OKLABEL));
  localized_strings->SetString("optionsResetCancelLabel",
      l10n_util::GetStringUTF16(IDS_OPTIONS_RESET_CANCELLABEL));
  localized_strings->SetString("optionsRestartRequired",
      l10n_util::GetStringUTF16(IDS_OPTIONS_RESTART_REQUIRED));
}

void AdvancedOptionsHandler::Initialize() {
  DCHECK(dom_ui_);
  SetupMetricsReportingCheckbox(false);
  SetupDownloadLocationPath();
  SetupAutoOpenFileTypesDisabledAttribute();
  SetupProxySettingsSection();
#if defined(OS_WIN)
  SetupSSLConfigSettings();
#endif
#if !defined(OS_CHROMEOS)
  SetupCloudPrintProxySection();
  RefreshCloudPrintStatusFromService();
#endif
  banner_handler_.reset(
      new OptionsManagedBannerHandler(dom_ui_,
                                      ASCIIToUTF16("AdvancedOptions"),
                                      OPTIONS_PAGE_ADVANCED));
}

DOMMessageHandler* AdvancedOptionsHandler::Attach(DOMUI* dom_ui) {
  // Call through to superclass.
  DOMMessageHandler* handler = OptionsPageUIHandler::Attach(dom_ui);

  // Register for preferences that we need to observe manually.  These have
  // special behaviors that aren't handled by the standard prefs UI.
  DCHECK(dom_ui_);
  PrefService* prefs = dom_ui_->GetProfile()->GetPrefs();
#if !defined(OS_CHROMEOS)
  enable_metrics_recording_.Init(prefs::kMetricsReportingEnabled,
                                 g_browser_process->local_state(), this);
  cloud_print_proxy_email_.Init(prefs::kCloudPrintEmail, prefs, this);
#endif
  default_download_location_.Init(prefs::kDownloadDefaultDirectory,
                                  prefs, this);
  auto_open_files_.Init(prefs::kDownloadExtensionsToOpen, prefs, this);
  proxy_prefs_.reset(
      PrefSetObserver::CreateProxyPrefSetObserver(prefs, this));

  // Return result from the superclass.
  return handler;
}

void AdvancedOptionsHandler::RegisterMessages() {
  // Setup handlers specific to this panel.
  DCHECK(dom_ui_);
  dom_ui_->RegisterMessageCallback("selectDownloadLocation",
      NewCallback(this,
                  &AdvancedOptionsHandler::HandleSelectDownloadLocation));
  dom_ui_->RegisterMessageCallback("autoOpenFileTypesAction",
      NewCallback(this,
                  &AdvancedOptionsHandler::HandleAutoOpenButton));
  dom_ui_->RegisterMessageCallback("resetToDefaults",
      NewCallback(this,
                  &AdvancedOptionsHandler::HandleResetToDefaults));
#if !defined(OS_CHROMEOS)
  dom_ui_->RegisterMessageCallback("metricsReportingCheckboxAction",
      NewCallback(this,
                  &AdvancedOptionsHandler::HandleMetricsReportingCheckbox));
#endif
#if !defined(USE_NSS)
  dom_ui_->RegisterMessageCallback("showManageSSLCertificates",
      NewCallback(this,
                  &AdvancedOptionsHandler::ShowManageSSLCertificates));
#endif
#if !defined(OS_CHROMEOS)
  dom_ui_->RegisterMessageCallback("showCloudPrintSetupDialog",
      NewCallback(this,
                  &AdvancedOptionsHandler::ShowCloudPrintSetupDialog));
  dom_ui_->RegisterMessageCallback("disableCloudPrintProxy",
      NewCallback(this,
                  &AdvancedOptionsHandler::HandleDisableCloudPrintProxy));
  dom_ui_->RegisterMessageCallback("showCloudPrintManagePage",
      NewCallback(this,
                  &AdvancedOptionsHandler::ShowCloudPrintManagePage));
  dom_ui_->RegisterMessageCallback("showNetworkProxySettings",
      NewCallback(this,
                  &AdvancedOptionsHandler::ShowNetworkProxySettings));
#endif

#if defined(OS_WIN)
  // Setup Windows specific callbacks.
  dom_ui_->RegisterMessageCallback("checkRevocationCheckboxAction",
      NewCallback(this,
                  &AdvancedOptionsHandler::HandleCheckRevocationCheckbox));
  dom_ui_->RegisterMessageCallback("useSSL2CheckboxAction",
      NewCallback(this,
                  &AdvancedOptionsHandler::HandleUseSSL2Checkbox));
  dom_ui_->RegisterMessageCallback("useSSL3CheckboxAction",
      NewCallback(this,
                  &AdvancedOptionsHandler::HandleUseSSL3Checkbox));
  dom_ui_->RegisterMessageCallback("useTLS1CheckboxAction",
      NewCallback(this,
                  &AdvancedOptionsHandler::HandleUseTLS1Checkbox));
  dom_ui_->RegisterMessageCallback("showGearsSettings",
      NewCallback(this,
                  &AdvancedOptionsHandler::HandleShowGearsSettings));
#endif
}

void AdvancedOptionsHandler::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED) {
    std::string* pref_name = Details<std::string>(details).ptr();
    if (*pref_name == prefs::kDownloadDefaultDirectory) {
      SetupDownloadLocationPath();
    } else if (*pref_name == prefs::kDownloadExtensionsToOpen) {
      SetupAutoOpenFileTypesDisabledAttribute();
    } else if (proxy_prefs_->IsObserved(*pref_name)) {
      SetupProxySettingsSection();
    } else if (*pref_name == prefs::kCloudPrintEmail) {
#if !defined(OS_CHROMEOS)
      SetupCloudPrintProxySection();
#endif
    }
  }
}

void AdvancedOptionsHandler::HandleSelectDownloadLocation(
    const ListValue* args) {
  PrefService* pref_service = dom_ui_->GetProfile()->GetPrefs();
  select_folder_dialog_ = SelectFileDialog::Create(this);
  select_folder_dialog_->SelectFile(
      SelectFileDialog::SELECT_FOLDER,
      l10n_util::GetStringUTF16(IDS_OPTIONS_DOWNLOADLOCATION_BROWSE_TITLE),
      pref_service->GetFilePath(prefs::kDownloadDefaultDirectory),
      NULL, 0, FILE_PATH_LITERAL(""),
      dom_ui_->tab_contents()->view()->GetTopLevelNativeWindow(), NULL);
}

void AdvancedOptionsHandler::FileSelected(const FilePath& path, int index,
                                          void* params) {
  UserMetricsRecordAction(UserMetricsAction("Options_SetDownloadDirectory"));
  default_download_location_.SetValue(path);
  SetupDownloadLocationPath();
}

void AdvancedOptionsHandler::OnDialogClosed() {
#if !defined(OS_CHROMEOS)
  SetupCloudPrintProxySection();
#endif
}

void AdvancedOptionsHandler::HandleAutoOpenButton(const ListValue* args) {
  UserMetricsRecordAction(UserMetricsAction("Options_ResetAutoOpenFiles"));
  DownloadManager* manager = dom_ui_->GetProfile()->GetDownloadManager();
  if (manager)
    manager->download_prefs()->ResetAutoOpen();
}

void AdvancedOptionsHandler::HandleResetToDefaults(const ListValue* args) {
  OptionsUtil::ResetToDefaults(dom_ui_->GetProfile());
}

void AdvancedOptionsHandler::HandleMetricsReportingCheckbox(
    const ListValue* args) {
#if defined(GOOGLE_CHROME_BUILD) && !defined(OS_CHROMEOS)
  std::string checked_str = WideToUTF8(ExtractStringValue(args));
  bool enabled = (checked_str == "true");
  UserMetricsRecordAction(
      enabled ?
          UserMetricsAction("Options_MetricsReportingCheckbox_Enable") :
          UserMetricsAction("Options_MetricsReportingCheckbox_Disable"));
  bool is_enabled = OptionsUtil::ResolveMetricsReportingEnabled(enabled);
  enable_metrics_recording_.SetValue(is_enabled);

  bool user_changed = (enabled == is_enabled);
  SetupMetricsReportingCheckbox(user_changed);
#endif
}

#if defined(OS_WIN)
void AdvancedOptionsHandler::HandleCheckRevocationCheckbox(
    const ListValue* args) {
  std::string checked_str = WideToUTF8(ExtractStringValue(args));
  bool enabled = (checked_str == "true");
  std::string metric =
      (enabled ? "Options_CheckCertRevocation_Enable"
               : "Options_CheckCertRevocation_Disable");
  UserMetricsRecordAction(UserMetricsAction(metric.c_str()));
  net::SSLConfigServiceWin::SetRevCheckingEnabled(enabled);
}

void AdvancedOptionsHandler::HandleUseSSL2Checkbox(const ListValue* args) {
  std::string checked_str = WideToUTF8(ExtractStringValue(args));
  bool enabled = (checked_str == "true");
  std::string metric =
      (enabled ? "Options_SSL2_Enable" : "Options_SSL2_Disable");
  UserMetricsRecordAction(UserMetricsAction(metric.c_str()));
  net::SSLConfigServiceWin::SetSSL2Enabled(enabled);
}

void AdvancedOptionsHandler::HandleUseSSL3Checkbox(const ListValue* args) {
  std::string checked_str = WideToUTF8(ExtractStringValue(args));
  bool enabled = (checked_str == "true");
  std::string metric =
      (enabled ? "Options_SSL3_Enable" : "Options_SSL3_Disable");
  UserMetricsRecordAction(UserMetricsAction(metric.c_str()));
  net::SSLConfigServiceWin::SetSSL3Enabled(enabled);
}

void AdvancedOptionsHandler::HandleUseTLS1Checkbox(const ListValue* args) {
  std::string checked_str = WideToUTF8(ExtractStringValue(args));
  bool enabled = (checked_str == "true");
  std::string metric =
      (enabled ? "Options_TLS1_Enable" : "Options_TLS1_Disable");
  UserMetricsRecordAction(UserMetricsAction(metric.c_str()));
  net::SSLConfigServiceWin::SetTLS1Enabled(enabled);
}

void AdvancedOptionsHandler::HandleShowGearsSettings(const ListValue* args) {
  UserMetricsRecordAction(UserMetricsAction("Options_GearsSettings"));
  GearsSettingsPressed(
      dom_ui_->tab_contents()->view()->GetTopLevelNativeWindow());
}
#endif

#if !defined(OS_CHROMEOS)
void AdvancedOptionsHandler::ShowNetworkProxySettings(const ListValue* args) {
  UserMetricsRecordAction(UserMetricsAction("Options_ShowProxySettings"));
  AdvancedOptionsUtilities::ShowNetworkProxySettings(dom_ui_->tab_contents());
}
#endif

#if !defined(USE_NSS)
void AdvancedOptionsHandler::ShowManageSSLCertificates(const ListValue* args) {
  UserMetricsRecordAction(UserMetricsAction("Options_ManageSSLCertificates"));
  AdvancedOptionsUtilities::ShowManageSSLCertificates(dom_ui_->tab_contents());
}
#endif

#if !defined(OS_CHROMEOS)
void AdvancedOptionsHandler::ShowCloudPrintSetupDialog(const ListValue* args) {
  UserMetricsRecordAction(UserMetricsAction("Options_EnableCloudPrintProxy"));
  CloudPrintSetupFlow::OpenDialog(
      dom_ui_->GetProfile(), this,
      dom_ui_->tab_contents()->GetMessageBoxRootWindow());
}

void AdvancedOptionsHandler::HandleDisableCloudPrintProxy(
    const ListValue* args) {
  UserMetricsRecordAction(UserMetricsAction("Options_DisableCloudPrintProxy"));
  dom_ui_->GetProfile()->GetCloudPrintProxyService()->DisableForUser();
}

void AdvancedOptionsHandler::ShowCloudPrintManagePage(const ListValue* args) {
  UserMetricsRecordAction(UserMetricsAction("Options_ManageCloudPrinters"));
  // Open a new tab in the current window for the management page.
  dom_ui_->tab_contents()->OpenURL(
      CloudPrintURL(dom_ui_->GetProfile()).GetCloudPrintServiceManageURL(),
      GURL(), NEW_FOREGROUND_TAB, PageTransition::LINK);
}

void AdvancedOptionsHandler::RefreshCloudPrintStatusFromService() {
  DCHECK(dom_ui_);
  dom_ui_->GetProfile()->GetCloudPrintProxyService()->
      RefreshStatusFromService();
}

void AdvancedOptionsHandler::SetupCloudPrintProxySection() {
  std::string email;
  if (dom_ui_->GetProfile()->GetPrefs()->HasPrefPath(prefs::kCloudPrintEmail))
    email = dom_ui_->GetProfile()->GetPrefs()->GetString(
        prefs::kCloudPrintEmail);
  FundamentalValue disabled(email.empty());

  string16 label_str;
  if (email.empty()) {
    label_str = l10n_util::GetStringUTF16(
        IDS_OPTIONS_CLOUD_PRINT_PROXY_DISABLED_LABEL);
  } else {
    label_str = l10n_util::GetStringFUTF16(
        IDS_OPTIONS_CLOUD_PRINT_PROXY_ENABLED_LABEL, UTF8ToUTF16(email));
  }
  StringValue label(label_str);

  dom_ui_->CallJavascriptFunction(
      L"options.AdvancedOptions.SetupCloudPrintProxySection",
      disabled, label);
}
#endif

void AdvancedOptionsHandler::SetupMetricsReportingCheckbox(bool user_changed) {
#if defined(GOOGLE_CHROME_BUILD) && !defined(OS_CHROMEOS)
  FundamentalValue checked(enable_metrics_recording_.GetValue());
  FundamentalValue disabled(enable_metrics_recording_.IsManaged());
  FundamentalValue user_has_changed(user_changed);
  dom_ui_->CallJavascriptFunction(
      L"options.AdvancedOptions.SetMetricsReportingCheckboxState", checked,
      disabled, user_has_changed);
#endif
}

void AdvancedOptionsHandler::SetupDownloadLocationPath() {
  StringValue value(default_download_location_.GetValue().value());
  dom_ui_->CallJavascriptFunction(
      L"options.AdvancedOptions.SetDownloadLocationPath", value);
}

void AdvancedOptionsHandler::SetupAutoOpenFileTypesDisabledAttribute() {
  // Set the enabled state for the AutoOpenFileTypesResetToDefault button.
  // We enable the button if the user has any auto-open file types registered.
  DownloadManager* manager = dom_ui_->GetProfile()->GetDownloadManager();
  bool disabled = !(manager && manager->download_prefs()->IsAutoOpenUsed());
  FundamentalValue value(disabled);
  dom_ui_->CallJavascriptFunction(
      L"options.AdvancedOptions.SetAutoOpenFileTypesDisabledAttribute", value);
}

void AdvancedOptionsHandler::SetupProxySettingsSection() {
  // Disable the button if proxy settings are managed by a sysadmin or
  // overridden by an extension.
  PrefService* pref_service = dom_ui_->GetProfile()->GetPrefs();
  const PrefService::Preference* proxy_server =
      pref_service->FindPreference(prefs::kProxyServer);
  bool is_extension_controlled = (proxy_server &&
                                  proxy_server->IsExtensionControlled());

  FundamentalValue disabled(proxy_prefs_->IsManaged() ||
                            is_extension_controlled);

  // Get the appropriate info string to describe the button.
  string16 label_str;
  if (is_extension_controlled) {
    label_str = l10n_util::GetStringUTF16(IDS_OPTIONS_EXTENSION_PROXIES_LABEL);
  } else {
    label_str = l10n_util::GetStringFUTF16(IDS_OPTIONS_SYSTEM_PROXIES_LABEL,
        l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  }
  StringValue label(label_str);

  dom_ui_->CallJavascriptFunction(
      L"options.AdvancedOptions.SetupProxySettingsSection", disabled, label);
}

#if defined(OS_WIN)
void AdvancedOptionsHandler::SetupSSLConfigSettings() {
  bool checkRevocationSetting = false;
  bool useSSL2Setting = false;
  bool useSSL3Setting = false;
  bool useTLS1Setting = false;
  bool disabled = false;

  net::SSLConfig config;
  if (net::SSLConfigServiceWin::GetSSLConfigNow(&config)) {
    checkRevocationSetting = config.rev_checking_enabled;
    useSSL2Setting = config.ssl2_enabled;
    useSSL3Setting = config.ssl3_enabled;
    useTLS1Setting = config.tls1_enabled;
  } else {
    disabled = true;
  }
  FundamentalValue disabledValue(disabled);
  FundamentalValue checkRevocationValue(checkRevocationSetting);
  dom_ui_->CallJavascriptFunction(
      L"options.AdvancedOptions.SetCheckRevocationCheckboxState",
      checkRevocationValue, disabledValue);
  FundamentalValue useSSL2Value(useSSL2Setting);
  dom_ui_->CallJavascriptFunction(
      L"options.AdvancedOptions.SetUseSSL2CheckboxState",
      useSSL2Value, disabledValue);
  FundamentalValue useSSL3Value(useSSL3Setting);
  dom_ui_->CallJavascriptFunction(
      L"options.AdvancedOptions.SetUseSSL3CheckboxState",
      useSSL3Value, disabledValue);
  FundamentalValue useTLS1Value(useTLS1Setting);
  dom_ui_->CallJavascriptFunction(
      L"options.AdvancedOptions.SetUseTLS1CheckboxState",
      useTLS1Value, disabledValue);
}
#endif
