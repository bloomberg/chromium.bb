// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/options/advanced_options_handler.h"

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/dom_ui/options/dom_options_util.h"
#include "chrome/browser/dom_ui/options/options_managed_banner_handler.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service.h"
#include "chrome/browser/printing/cloud_print/cloud_print_setup_flow.h"
#include "chrome/browser/printing/cloud_print/cloud_print_url.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/remoting/setup_flow.h"
#include "chrome/browser/service/service_process_control.h"
#include "chrome/browser/service/service_process_control_manager.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/ui/options/options_util.h"
#include "chrome/browser/ui/options/options_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/dom_ui/options/advanced_options_utils.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/gears_integration.h"
#include "net/base/ssl_config_service_win.h"
#endif

AdvancedOptionsHandler::AdvancedOptionsHandler() {
#if defined(GOOGLE_CHROME_BUILD) && defined(OS_WIN)
  cloud_print_proxy_ui_enabled_ = true;
#elif !defined(OS_CHROMEOS)
  cloud_print_proxy_ui_enabled_ =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableCloudPrintProxy);
#endif
}

AdvancedOptionsHandler::~AdvancedOptionsHandler() {
}

void AdvancedOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  OptionsStringResource resources[] = {
    { "downloadLocationGroupName",
      IDS_OPTIONS_DOWNLOADLOCATION_GROUP_NAME },
    { "downloadLocationChangeButton",
      IDS_OPTIONS_DOWNLOADLOCATION_CHANGE_BUTTON },
    { "downloadLocationBrowseTitle",
      IDS_OPTIONS_DOWNLOADLOCATION_BROWSE_TITLE },
    { "downloadLocationBrowseWindowTitle",
      IDS_OPTIONS_DOWNLOADLOCATION_BROWSE_WINDOW_TITLE },
    { "downloadLocationAskForSaveLocation",
      IDS_OPTIONS_DOWNLOADLOCATION_ASKFORSAVELOCATION },
    { "autoOpenFileTypesInfo",
      IDS_OPTIONS_OPEN_FILE_TYPES_AUTOMATICALLY },
    { "autoOpenFileTypesResetToDefault",
      IDS_OPTIONS_AUTOOPENFILETYPES_RESETTODEFAULT },
    { "gearSettingsConfigureGearsButton",
      IDS_OPTIONS_GEARSSETTINGS_CONFIGUREGEARS_BUTTON },
    { "translateEnableTranslate",
      IDS_OPTIONS_TRANSLATE_ENABLE_TRANSLATE },
    { "certificatesManageButton",
      IDS_OPTIONS_CERTIFICATES_MANAGE_BUTTON },
    { "proxiesLabel",
      IDS_OPTIONS_PROXIES_LABEL },
    { "proxiesConfigureButton",
      IDS_OPTIONS_PROXIES_CONFIGURE_BUTTON },
    { "safeBrowsingEnableProtection",
      IDS_OPTIONS_SAFEBROWSING_ENABLEPROTECTION },
    { "sslGroupDescription",
      IDS_OPTIONS_SSL_GROUP_DESCRIPTION },
    { "sslCheckRevocation",
      IDS_OPTIONS_SSL_CHECKREVOCATION },
    { "sslUseSSL3",
      IDS_OPTIONS_SSL_USESSL3 },
    { "sslUseTLS1",
      IDS_OPTIONS_SSL_USETLS1 },
    { "networkDNSPrefetchEnabledDescription",
      IDS_NETWORK_DNS_PREFETCH_ENABLED_DESCRIPTION },
    { "privacyContentSettingsButton",
      IDS_OPTIONS_PRIVACY_CONTENT_SETTINGS_BUTTON },
    { "privacyClearDataButton",
      IDS_OPTIONS_PRIVACY_CLEAR_DATA_BUTTON },
    { "linkDoctorPref",
      IDS_OPTIONS_LINKDOCTOR_PREF },
    { "suggestPref",
      IDS_OPTIONS_SUGGEST_PREF },
    { "tabsToLinksPref",
      IDS_OPTIONS_TABS_TO_LINKS_PREF },
    { "fontSettingsInfo",
      IDS_OPTIONS_FONTSETTINGS_INFO },
    { "defaultZoomLevelLabel",
      IDS_OPTIONS_DEFAULT_ZOOM_LEVEL_LABEL },
    { "defaultFontSizeLabel",
      IDS_OPTIONS_DEFAULT_FONT_SIZE_LABEL },
    { "fontSizeLabelVerySmall",
      IDS_OPTIONS_FONT_SIZE_LABEL_VERY_SMALL },
    { "fontSizeLabelSmall",
      IDS_OPTIONS_FONT_SIZE_LABEL_SMALL },
    { "fontSizeLabelMedium",
      IDS_OPTIONS_FONT_SIZE_LABEL_MEDIUM },
    { "fontSizeLabelLarge",
      IDS_OPTIONS_FONT_SIZE_LABEL_LARGE },
    { "fontSizeLabelVeryLarge",
      IDS_OPTIONS_FONT_SIZE_LABEL_VERY_LARGE },
    { "fontSizeLabelCustom",
      IDS_OPTIONS_FONT_SIZE_LABEL_CUSTOM },
    { "fontSettingsCustomizeFontsButton",
      IDS_OPTIONS_FONTSETTINGS_CUSTOMIZE_FONTS_BUTTON },
    { "languageAndSpellCheckSettingsButton",
      IDS_OPTIONS_LANGUAGE_AND_SPELLCHECK_BUTTON, true },
    { "advancedSectionTitlePrivacy",
      IDS_OPTIONS_ADVANCED_SECTION_TITLE_PRIVACY, true },
    { "advancedSectionTitleContent",
      IDS_OPTIONS_ADVANCED_SECTION_TITLE_CONTENT, true },
    { "advancedSectionTitleSecurity",
      IDS_OPTIONS_ADVANCED_SECTION_TITLE_SECURITY, true },
    { "advancedSectionTitleNetwork",
      IDS_OPTIONS_ADVANCED_SECTION_TITLE_NETWORK, true },
    { "advancedSectionTitleTranslate",
      IDS_OPTIONS_ADVANCED_SECTION_TITLE_TRANSLATE, true },
    { "translateEnableTranslate",
      IDS_OPTIONS_TRANSLATE_ENABLE_TRANSLATE },
    { "enableLogging",
      IDS_OPTIONS_ENABLE_LOGGING },
    { "improveBrowsingExperience",
      IDS_OPTIONS_IMPROVE_BROWSING_EXPERIENCE },
    { "disableWebServices",
      IDS_OPTIONS_DISABLE_WEB_SERVICES },
#if !defined(OS_CHROMEOS)
    { "advancedSectionTitleCloudPrint",
      IDS_OPTIONS_ADVANCED_SECTION_TITLE_CLOUD_PRINT },
    { "cloudPrintProxyDisabledLabel",
      IDS_OPTIONS_CLOUD_PRINT_PROXY_DISABLED_LABEL },
    { "cloudPrintProxyDisabledButton",
      IDS_OPTIONS_CLOUD_PRINT_PROXY_DISABLED_BUTTON },
    { "cloudPrintProxyEnabledButton",
      IDS_OPTIONS_CLOUD_PRINT_PROXY_ENABLED_BUTTON },
    { "cloudPrintProxyEnabledManageButton",
      IDS_OPTIONS_CLOUD_PRINT_PROXY_ENABLED_MANAGE_BUTTON },
    { "cloudPrintProxyEnablingButton",
      IDS_OPTIONS_CLOUD_PRINT_PROXY_ENABLING_BUTTON },
#endif
#if defined(ENABLE_REMOTING)
    { "advancedSectionTitleRemoting",
      IDS_OPTIONS_ADVANCED_SECTION_TITLE_REMOTING },
    { "remotingSetupButton",
      IDS_OPTIONS_REMOTING_SETUP_BUTTON },
    { "remotingStopButton",
      IDS_OPTIONS_REMOTING_STOP_BUTTON },
#endif
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "advancedPage",
                IDS_OPTIONS_ADVANCED_TAB_LABEL);

  localized_strings->SetString("privacyLearnMoreURL",
      google_util::AppendGoogleLocaleParam(
          GURL(chrome::kPrivacyLearnMoreURL)).spec());
#if !defined(OS_CHROMEOS)
  // Add the cloud print proxy management ui section if it's been runtime
  // enabled.
  localized_strings->SetString(
      "enable-cloud-print-proxy",
      cloud_print_proxy_ui_enabled_ ? "true" : "false" );
#endif
}

void AdvancedOptionsHandler::Initialize() {
  DCHECK(dom_ui_);
  SetupMetricsReportingCheckbox();
  SetupMetricsReportingSettingVisibility();
  SetupFontSizeLabel();
  SetupDownloadLocationPath();
  SetupAutoOpenFileTypesDisabledAttribute();
  SetupProxySettingsSection();
#if defined(OS_WIN)
  SetupSSLConfigSettings();
#endif
#if !defined(OS_CHROMEOS)
  if (cloud_print_proxy_ui_enabled_) {
    SetupCloudPrintProxySection();
    RefreshCloudPrintStatusFromService();
  } else {
    RemoveCloudPrintProxySection();
  }
#endif
#if defined(ENABLE_REMOTING) && !defined(OS_CHROMEOS)
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableRemoting)) {
    RemoveRemotingSection();
  } else {
    remoting_options_handler_.Init(dom_ui_);
  }
#endif

  banner_handler_.reset(
      new OptionsManagedBannerHandler(dom_ui_,
                                      ASCIIToUTF16("AdvancedOptions"),
                                      OPTIONS_PAGE_ADVANCED));
}

WebUIMessageHandler* AdvancedOptionsHandler::Attach(DOMUI* dom_ui) {
  // Call through to superclass.
  WebUIMessageHandler* handler = OptionsPageUIHandler::Attach(dom_ui);

  // Register for preferences that we need to observe manually.  These have
  // special behaviors that aren't handled by the standard prefs UI.
  DCHECK(dom_ui_);
  PrefService* prefs = dom_ui_->GetProfile()->GetPrefs();
#if !defined(OS_CHROMEOS)
  enable_metrics_recording_.Init(prefs::kMetricsReportingEnabled,
                                 g_browser_process->local_state(), this);
  cloud_print_proxy_email_.Init(prefs::kCloudPrintEmail, prefs, this);
  cloud_print_proxy_enabled_.Init(prefs::kCloudPrintProxyEnabled, prefs, this);
#endif
  default_download_location_.Init(prefs::kDownloadDefaultDirectory,
                                  prefs, this);
  auto_open_files_.Init(prefs::kDownloadExtensionsToOpen, prefs, this);
  default_font_size_.Init(prefs::kWebKitDefaultFontSize, prefs, this);
  default_fixed_font_size_.Init(prefs::kWebKitDefaultFixedFontSize, prefs,
                                this);
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
  dom_ui_->RegisterMessageCallback("defaultFontSizeAction",
      NewCallback(this, &AdvancedOptionsHandler::HandleDefaultFontSize));
#if !defined(OS_CHROMEOS)
  dom_ui_->RegisterMessageCallback("metricsReportingCheckboxAction",
      NewCallback(this,
                  &AdvancedOptionsHandler::HandleMetricsReportingCheckbox));
#endif
#if !defined(USE_NSS) && !defined(USE_OPENSSL)
  dom_ui_->RegisterMessageCallback("showManageSSLCertificates",
      NewCallback(this,
                  &AdvancedOptionsHandler::ShowManageSSLCertificates));
#endif
#if !defined(OS_CHROMEOS)
  if (cloud_print_proxy_ui_enabled_) {
    dom_ui_->RegisterMessageCallback("showCloudPrintSetupDialog",
        NewCallback(this,
                    &AdvancedOptionsHandler::ShowCloudPrintSetupDialog));
    dom_ui_->RegisterMessageCallback("disableCloudPrintProxy",
        NewCallback(this,
                    &AdvancedOptionsHandler::HandleDisableCloudPrintProxy));
    dom_ui_->RegisterMessageCallback("showCloudPrintManagePage",
        NewCallback(this,
                    &AdvancedOptionsHandler::ShowCloudPrintManagePage));
  }
  dom_ui_->RegisterMessageCallback("showNetworkProxySettings",
      NewCallback(this,
                  &AdvancedOptionsHandler::ShowNetworkProxySettings));
#endif
#if defined(ENABLE_REMOTING) && !defined(OS_CHROMEOS)
  dom_ui_->RegisterMessageCallback("showRemotingSetupDialog",
      NewCallback(this,
                  &AdvancedOptionsHandler::ShowRemotingSetupDialog));
  dom_ui_->RegisterMessageCallback("disableRemoting",
      NewCallback(this,
                  &AdvancedOptionsHandler::DisableRemoting));
#endif
#if defined(OS_WIN)
  // Setup Windows specific callbacks.
  dom_ui_->RegisterMessageCallback("checkRevocationCheckboxAction",
      NewCallback(this,
                  &AdvancedOptionsHandler::HandleCheckRevocationCheckbox));
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
    } else if ((*pref_name == prefs::kCloudPrintEmail) ||
               (*pref_name == prefs::kCloudPrintProxyEnabled)) {
#if !defined(OS_CHROMEOS)
      if (cloud_print_proxy_ui_enabled_)
        SetupCloudPrintProxySection();
#endif
    } else if (*pref_name == prefs::kWebKitDefaultFontSize ||
               *pref_name == prefs::kWebKitDefaultFixedFontSize) {
      SetupFontSizeLabel();
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
  if (cloud_print_proxy_ui_enabled_)
    SetupCloudPrintProxySection();
#endif
}

void AdvancedOptionsHandler::HandleAutoOpenButton(const ListValue* args) {
  UserMetricsRecordAction(UserMetricsAction("Options_ResetAutoOpenFiles"));
  DownloadManager* manager = dom_ui_->GetProfile()->GetDownloadManager();
  if (manager)
    manager->download_prefs()->ResetAutoOpen();
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
  SetupMetricsReportingCheckbox();
#endif
}

void AdvancedOptionsHandler::HandleDefaultFontSize(const ListValue* args) {
  int font_size;
  if (ExtractIntegerValue(args, &font_size)) {
    if (font_size > 0) {
      default_font_size_.SetValue(font_size);
      default_fixed_font_size_.SetValue(font_size);
      SetupFontSizeLabel();
    }
  }
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

#if !defined(USE_NSS) && !defined(USE_OPENSSL)
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
  if (cloud_print_proxy_ui_enabled_)
    dom_ui_->GetProfile()->GetCloudPrintProxyService()->
        RefreshStatusFromService();
}

void AdvancedOptionsHandler::SetupCloudPrintProxySection() {
  if (NULL == dom_ui_->GetProfile()->GetCloudPrintProxyService()) {
    cloud_print_proxy_ui_enabled_ = false;
    RemoveCloudPrintProxySection();
    return;
  }

  bool cloud_print_proxy_allowed =
      !cloud_print_proxy_enabled_.IsManaged() ||
      cloud_print_proxy_enabled_.GetValue();
  FundamentalValue allowed(cloud_print_proxy_allowed);

  std::string email;
  if (dom_ui_->GetProfile()->GetPrefs()->HasPrefPath(prefs::kCloudPrintEmail) &&
      cloud_print_proxy_allowed) {
    email = dom_ui_->GetProfile()->GetPrefs()->GetString(
        prefs::kCloudPrintEmail);
  }
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
      disabled, label, allowed);
}

void AdvancedOptionsHandler::RemoveCloudPrintProxySection() {
  dom_ui_->CallJavascriptFunction(
      L"options.AdvancedOptions.RemoveCloudPrintProxySection");
}

#endif

#if defined(ENABLE_REMOTING) && !defined(OS_CHROMEOS)
void AdvancedOptionsHandler::RemoveRemotingSection() {
  dom_ui_->CallJavascriptFunction(
      L"options.AdvancedOptions.RemoveRemotingSection");
}

void AdvancedOptionsHandler::ShowRemotingSetupDialog(const ListValue* args) {
  remoting::SetupFlow::OpenSetupDialog(dom_ui_->GetProfile());
}

void AdvancedOptionsHandler::DisableRemoting(const ListValue* args) {
  ServiceProcessControl* process_control =
      ServiceProcessControlManager::GetInstance()->GetProcessControl(
          dom_ui_->GetProfile());
  if (!process_control || !process_control->is_connected())
    return;
  process_control->DisableRemotingHost();
}
#endif

void AdvancedOptionsHandler::SetupMetricsReportingCheckbox() {
#if defined(GOOGLE_CHROME_BUILD) && !defined(OS_CHROMEOS)
  FundamentalValue checked(enable_metrics_recording_.GetValue());
  FundamentalValue disabled(enable_metrics_recording_.IsManaged());
  dom_ui_->CallJavascriptFunction(
      L"options.AdvancedOptions.SetMetricsReportingCheckboxState", checked,
      disabled);
#endif
}

void AdvancedOptionsHandler::SetupMetricsReportingSettingVisibility() {
#if defined(GOOGLE_CHROME_BUILD) && defined(OS_CHROMEOS)
  // Don't show the reporting setting if we are in the guest mode.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kGuestSession)) {
    FundamentalValue visible(false);
    dom_ui_->CallJavascriptFunction(
        L"options.AdvancedOptions.SetMetricsReportingSettingVisibility",
        visible);
  }
#endif
}

void AdvancedOptionsHandler::SetupFontSizeLabel() {
  // We're only interested in integer values, so convert to int.
  FundamentalValue fixed_font_size(default_fixed_font_size_.GetValue());
  FundamentalValue font_size(default_font_size_.GetValue());
  dom_ui_->CallJavascriptFunction(
      L"options.AdvancedOptions.SetFontSize", fixed_font_size,
      font_size);
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
  const PrefService::Preference* proxy_config =
      pref_service->FindPreference(prefs::kProxy);
  bool is_extension_controlled = (proxy_config &&
                                  proxy_config->IsExtensionControlled());

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
  bool useSSL3Setting = false;
  bool useTLS1Setting = false;
  bool disabled = false;

  net::SSLConfig config;
  if (net::SSLConfigServiceWin::GetSSLConfigNow(&config)) {
    checkRevocationSetting = config.rev_checking_enabled;
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
