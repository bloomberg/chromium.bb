// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/advanced_options_handler.h"

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_page_zoom.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service_factory.h"
#include "chrome/browser/printing/cloud_print/cloud_print_setup_flow.h"
#include "chrome/browser/printing/cloud_print/cloud_print_url.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/service/service_process_control.h"
#include "chrome/browser/ui/options/options_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_zoom.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/printing/cloud_print/cloud_print_setup_handler.h"
#include "chrome/browser/ui/webui/options/advanced_options_utils.h"
#endif

using content::DownloadManager;
using content::OpenURLParams;
using content::Referrer;
using content::UserMetricsAction;

AdvancedOptionsHandler::AdvancedOptionsHandler() {

#if(!defined(GOOGLE_CHROME_BUILD) && defined(OS_WIN))
  // On Windows, we need the PDF plugin which is only guaranteed to exist on
  // Google Chrome builds. Use a command-line switch for Windows non-Google
  //  Chrome builds.
  cloud_print_connector_ui_enabled_ =
      CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableCloudPrintProxy);
#elif(!defined(OS_CHROMEOS))
  // Always enabled for Mac, Linux and Google Chrome Windows builds.
  // Never enabled for Chrome OS, we don't even need to indicate it.
  cloud_print_connector_ui_enabled_ = true;
#endif
}

AdvancedOptionsHandler::~AdvancedOptionsHandler() {
  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (select_folder_dialog_.get())
    select_folder_dialog_->ListenerDestroyed();
}

void AdvancedOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
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
    { "translateEnableTranslate",
      IDS_OPTIONS_TRANSLATE_ENABLE_TRANSLATE },
    { "certificatesManageButton",
      IDS_OPTIONS_CERTIFICATES_MANAGE_BUTTON },
    { "proxiesLabel",
      IDS_OPTIONS_PROXIES_LABEL },
#if !defined(OS_CHROMEOS)
    { "proxiesConfigureButton",
      IDS_OPTIONS_PROXIES_CONFIGURE_BUTTON },
#endif
    { "safeBrowsingEnableProtection",
      IDS_OPTIONS_SAFEBROWSING_ENABLEPROTECTION },
    { "sslGroupDescription",
      IDS_OPTIONS_SSL_GROUP_DESCRIPTION },
    { "sslCheckRevocation",
      IDS_OPTIONS_SSL_CHECKREVOCATION },
    { "networkPredictionEnabledDescription",
      IDS_NETWORK_PREDICTION_ENABLED_DESCRIPTION },
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
    { "defaultZoomFactorLabel",
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
      IDS_OPTIONS_LANGUAGE_AND_SPELLCHECK_BUTTON },
    { "advancedSectionTitlePrivacy",
      IDS_OPTIONS_ADVANCED_SECTION_TITLE_PRIVACY },
    { "advancedSectionTitleContent",
      IDS_OPTIONS_ADVANCED_SECTION_TITLE_CONTENT },
    { "advancedSectionTitleSecurity",
      IDS_OPTIONS_ADVANCED_SECTION_TITLE_SECURITY },
    { "advancedSectionTitleNetwork",
      IDS_OPTIONS_ADVANCED_SECTION_TITLE_NETWORK },
    { "advancedSectionTitleTranslate",
      IDS_OPTIONS_ADVANCED_SECTION_TITLE_TRANSLATE },
    { "translateEnableTranslate",
      IDS_OPTIONS_TRANSLATE_ENABLE_TRANSLATE },
    { "enableLogging",
      IDS_OPTIONS_ENABLE_LOGGING },
    { "improveBrowsingExperience",
      IDS_OPTIONS_IMPROVE_BROWSING_EXPERIENCE },
    { "disableWebServices",
      IDS_OPTIONS_DISABLE_WEB_SERVICES },
    { "advancedSectionTitleCloudPrint",
      IDS_GOOGLE_CLOUD_PRINT },
#if !defined(OS_CHROMEOS)
    { "cloudPrintConnectorEnabledManageButton",
      IDS_OPTIONS_CLOUD_PRINT_CONNECTOR_ENABLED_MANAGE_BUTTON},
    { "cloudPrintConnectorEnablingButton",
      IDS_OPTIONS_CLOUD_PRINT_CONNECTOR_ENABLING_BUTTON },
#endif
#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
    { "advancedSectionTitleBackground",
      IDS_OPTIONS_ADVANCED_SECTION_TITLE_BACKGROUND },
    { "backgroundModeCheckbox",
      IDS_OPTIONS_BACKGROUND_ENABLE_BACKGROUND_MODE },
#endif
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterCloudPrintStrings(localized_strings);
  RegisterTitle(localized_strings, "advancedPage",
                IDS_OPTIONS_ADVANCED_TAB_LABEL);

  localized_strings->SetString("privacyLearnMoreURL",
      google_util::AppendGoogleLocaleParam(
          GURL(chrome::kPrivacyLearnMoreURL)).spec());

#if defined(OS_CHROMEOS)
  localized_strings->SetString("cloudPrintLearnMoreURL",
      google_util::AppendGoogleLocaleParam(
          GURL(chrome::kCloudPrintLearnMoreURL)).spec());
#endif
}

void AdvancedOptionsHandler::RegisterCloudPrintStrings(
    DictionaryValue* localized_strings) {
#if defined(OS_CHROMEOS)
  localized_strings->SetString("cloudPrintChromeosOptionLabel",
      l10n_util::GetStringFUTF16(
      IDS_CLOUD_PRINT_CHROMEOS_OPTION_LABEL,
      l10n_util::GetStringUTF16(IDS_GOOGLE_CLOUD_PRINT)));
  localized_strings->SetString("cloudPrintChromeosOptionButton",
      l10n_util::GetStringFUTF16(
      IDS_CLOUD_PRINT_CHROMEOS_OPTION_BUTTON,
      l10n_util::GetStringUTF16(IDS_GOOGLE_CLOUD_PRINT)));
#else
  localized_strings->SetString("cloudPrintConnectorDisabledLabel",
      l10n_util::GetStringFUTF16(
      IDS_OPTIONS_CLOUD_PRINT_CONNECTOR_DISABLED_LABEL,
      l10n_util::GetStringUTF16(IDS_GOOGLE_CLOUD_PRINT)));
  localized_strings->SetString("cloudPrintConnectorDisabledButton",
      l10n_util::GetStringFUTF16(
      IDS_OPTIONS_CLOUD_PRINT_CONNECTOR_DISABLED_BUTTON,
      l10n_util::GetStringUTF16(IDS_GOOGLE_CLOUD_PRINT)));
  localized_strings->SetString("cloudPrintConnectorEnabledButton",
      l10n_util::GetStringFUTF16(
      IDS_OPTIONS_CLOUD_PRINT_CONNECTOR_ENABLED_BUTTON,
      l10n_util::GetStringUTF16(IDS_GOOGLE_CLOUD_PRINT)));
#endif
}

void AdvancedOptionsHandler::Initialize() {
  DCHECK(web_ui());
  SetupMetricsReportingCheckbox();
  SetupMetricsReportingSettingVisibility();
  SetupFontSizeSelector();
  SetupPageZoomSelector();
  SetupAutoOpenFileTypesDisabledAttribute();
  SetupProxySettingsSection();
  SetupSSLConfigSettings();
#if !defined(OS_CHROMEOS)
  if (cloud_print_connector_ui_enabled_) {
    SetupCloudPrintConnectorSection();
    RefreshCloudPrintStatusFromService();
  } else {
    RemoveCloudPrintConnectorSection();
  }
#endif
#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
  SetupBackgroundModeSettings();
#endif

}

void AdvancedOptionsHandler::RegisterMessages() {
  // Register for preferences that we need to observe manually.  These have
  // special behaviors that aren't handled by the standard prefs UI.
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
#if !defined(OS_CHROMEOS)
  enable_metrics_recording_.Init(prefs::kMetricsReportingEnabled,
                                 g_browser_process->local_state(), this);
  cloud_print_connector_email_.Init(prefs::kCloudPrintEmail, prefs, this);
  cloud_print_connector_enabled_.Init(prefs::kCloudPrintProxyEnabled,
                                      prefs,
                                      this);
#endif

  rev_checking_enabled_.Init(prefs::kCertRevocationCheckingEnabled,
                             g_browser_process->local_state(), this);

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
  background_mode_enabled_.Init(prefs::kBackgroundModeEnabled,
                                g_browser_process->local_state(),
                                this);
#endif

  auto_open_files_.Init(prefs::kDownloadExtensionsToOpen, prefs, this);
  default_font_size_.Init(prefs::kWebKitGlobalDefaultFontSize, prefs, this);
  default_zoom_level_.Init(prefs::kDefaultZoomLevel, prefs, this);
#if !defined(OS_CHROMEOS)
  proxy_prefs_.reset(
      PrefSetObserver::CreateProxyPrefSetObserver(prefs, this));
#endif  // !defined(OS_CHROMEOS)

  // Setup handlers specific to this panel.
  web_ui()->RegisterMessageCallback("selectDownloadLocation",
      base::Bind(&AdvancedOptionsHandler::HandleSelectDownloadLocation,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("autoOpenFileTypesAction",
      base::Bind(&AdvancedOptionsHandler::HandleAutoOpenButton,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("defaultFontSizeAction",
      base::Bind(&AdvancedOptionsHandler::HandleDefaultFontSize,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("defaultZoomFactorAction",
      base::Bind(&AdvancedOptionsHandler::HandleDefaultZoomFactor,
                 base::Unretained(this)));
#if !defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback("metricsReportingCheckboxAction",
      base::Bind(&AdvancedOptionsHandler::HandleMetricsReportingCheckbox,
                 base::Unretained(this)));
#endif
#if !defined(USE_NSS) && !defined(USE_OPENSSL)
  web_ui()->RegisterMessageCallback("showManageSSLCertificates",
      base::Bind(&AdvancedOptionsHandler::ShowManageSSLCertificates,
                 base::Unretained(this)));
#endif
  web_ui()->RegisterMessageCallback("showCloudPrintManagePage",
      base::Bind(&AdvancedOptionsHandler::ShowCloudPrintManagePage,
                 base::Unretained(this)));
#if !defined(OS_CHROMEOS)
  if (cloud_print_connector_ui_enabled_) {
    web_ui()->RegisterMessageCallback("showCloudPrintSetupDialog",
        base::Bind(&AdvancedOptionsHandler::ShowCloudPrintSetupDialog,
                   base::Unretained(this)));
    web_ui()->RegisterMessageCallback("disableCloudPrintConnector",
        base::Bind(&AdvancedOptionsHandler::HandleDisableCloudPrintConnector,
                   base::Unretained(this)));
  }
  web_ui()->RegisterMessageCallback("showNetworkProxySettings",
      base::Bind(&AdvancedOptionsHandler::ShowNetworkProxySettings,
                 base::Unretained(this)));
#endif
  web_ui()->RegisterMessageCallback("checkRevocationCheckboxAction",
      base::Bind(&AdvancedOptionsHandler::HandleCheckRevocationCheckbox,
                 base::Unretained(this)));
#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback("backgroundModeAction",
      base::Bind(&AdvancedOptionsHandler::HandleBackgroundModeCheckbox,
                 base::Unretained(this)));
#endif
}

void AdvancedOptionsHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_PREF_CHANGED) {
    std::string* pref_name = content::Details<std::string>(details).ptr();
    if (*pref_name == prefs::kDownloadExtensionsToOpen) {
      SetupAutoOpenFileTypesDisabledAttribute();
#if !defined(OS_CHROMEOS)
    } else if (proxy_prefs_->IsObserved(*pref_name)) {
      SetupProxySettingsSection();
#endif  // !defined(OS_CHROMEOS)
    } else if ((*pref_name == prefs::kCloudPrintEmail) ||
               (*pref_name == prefs::kCloudPrintProxyEnabled)) {
#if !defined(OS_CHROMEOS)
      if (cloud_print_connector_ui_enabled_)
        SetupCloudPrintConnectorSection();
#endif
    } else if (*pref_name == prefs::kWebKitGlobalDefaultFontSize) {
      SetupFontSizeSelector();
    } else if (*pref_name == prefs::kDefaultZoomLevel) {
      SetupPageZoomSelector();
#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
    } else if (*pref_name == prefs::kBackgroundModeEnabled) {
      SetupBackgroundModeSettings();
#endif
    }
  }
}

void AdvancedOptionsHandler::HandleSelectDownloadLocation(
    const ListValue* args) {
  PrefService* pref_service = Profile::FromWebUI(web_ui())->GetPrefs();
  select_folder_dialog_ = SelectFileDialog::Create(this);
  select_folder_dialog_->SelectFile(
      SelectFileDialog::SELECT_FOLDER,
      l10n_util::GetStringUTF16(IDS_OPTIONS_DOWNLOADLOCATION_BROWSE_TITLE),
      pref_service->GetFilePath(prefs::kDownloadDefaultDirectory),
      NULL, 0, FILE_PATH_LITERAL(""), web_ui()->GetWebContents(),
      web_ui()->GetWebContents()->GetView()->GetTopLevelNativeWindow(), NULL);
}

void AdvancedOptionsHandler::FileSelected(const FilePath& path, int index,
                                          void* params) {
  content::RecordAction(UserMetricsAction("Options_SetDownloadDirectory"));
  PrefService* pref_service = Profile::FromWebUI(web_ui())->GetPrefs();
  pref_service->SetFilePath(prefs::kDownloadDefaultDirectory, path);
}

void AdvancedOptionsHandler::OnCloudPrintSetupClosed() {
#if !defined(OS_CHROMEOS)
  if (cloud_print_connector_ui_enabled_)
    SetupCloudPrintConnectorSection();
#endif
}

void AdvancedOptionsHandler::HandleAutoOpenButton(const ListValue* args) {
  content::RecordAction(UserMetricsAction("Options_ResetAutoOpenFiles"));
  DownloadManager* manager =
      web_ui()->GetWebContents()->GetBrowserContext()->GetDownloadManager();
  if (manager)
    DownloadPrefs::FromDownloadManager(manager)->ResetAutoOpen();
}

void AdvancedOptionsHandler::HandleMetricsReportingCheckbox(
    const ListValue* args) {
#if defined(GOOGLE_CHROME_BUILD) && !defined(OS_CHROMEOS)
  std::string checked_str = UTF16ToUTF8(ExtractStringValue(args));
  bool enabled = checked_str == "true";
  content::RecordAction(
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
      SetupFontSizeSelector();
    }
  }
}

void AdvancedOptionsHandler::HandleDefaultZoomFactor(const ListValue* args) {
  double zoom_factor;
  if (ExtractDoubleValue(args, &zoom_factor)) {
    default_zoom_level_.SetValue(
        WebKit::WebView::zoomFactorToZoomLevel(zoom_factor));
  }
}

void AdvancedOptionsHandler::HandleCheckRevocationCheckbox(
    const ListValue* args) {
  std::string checked_str = UTF16ToUTF8(ExtractStringValue(args));
  bool enabled = checked_str == "true";
  content::RecordAction(
      enabled ?
      UserMetricsAction("Options_CheckCertRevocation_Enable") :
          UserMetricsAction("Options_CheckCertRevocation_Disable"));
  rev_checking_enabled_.SetValue(enabled);
}

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
void AdvancedOptionsHandler::HandleBackgroundModeCheckbox(
    const ListValue* args) {
  std::string checked_str = UTF16ToUTF8(ExtractStringValue(args));
  bool enabled = checked_str == "true";
  content::RecordAction(enabled ?
                        UserMetricsAction("Options_BackgroundMode_Enable") :
      UserMetricsAction("Options_BackgroundMode_Disable"));
  background_mode_enabled_.SetValue(enabled);
}

void AdvancedOptionsHandler::SetupBackgroundModeSettings() {
    base::FundamentalValue checked(background_mode_enabled_.GetValue());
    web_ui()->CallJavascriptFunction(
        "options.AdvancedOptions.SetBackgroundModeCheckboxState", checked);
}
#endif

#if !defined(OS_CHROMEOS)
void AdvancedOptionsHandler::ShowNetworkProxySettings(const ListValue* args) {
  content::RecordAction(UserMetricsAction("Options_ShowProxySettings"));
  AdvancedOptionsUtilities::ShowNetworkProxySettings(
      web_ui()->GetWebContents());
}
#endif

#if !defined(USE_NSS) && !defined(USE_OPENSSL)
void AdvancedOptionsHandler::ShowManageSSLCertificates(const ListValue* args) {
  content::RecordAction(UserMetricsAction("Options_ManageSSLCertificates"));
  AdvancedOptionsUtilities::ShowManageSSLCertificates(
      web_ui()->GetWebContents());
}
#endif

void AdvancedOptionsHandler::ShowCloudPrintManagePage(const ListValue* args) {
  content::RecordAction(UserMetricsAction("Options_ManageCloudPrinters"));
  // Open a new tab in the current window for the management page.
  Profile* profile = Profile::FromWebUI(web_ui());
  OpenURLParams params(
      CloudPrintURL(profile).GetCloudPrintServiceManageURL(), Referrer(),
      NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_LINK, false);
  web_ui()->GetWebContents()->OpenURL(params);
}

#if !defined(OS_CHROMEOS)
void AdvancedOptionsHandler::ShowCloudPrintSetupDialog(const ListValue* args) {
  content::RecordAction(UserMetricsAction("Options_EnableCloudPrintProxy"));
  // Open the connector enable page in the current tab.
  Profile* profile = Profile::FromWebUI(web_ui());
  OpenURLParams params(
      CloudPrintURL(profile).GetCloudPrintServiceEnableURL(
          CloudPrintProxyServiceFactory::GetForProfile(profile)->proxy_id()),
      Referrer(), CURRENT_TAB, content::PAGE_TRANSITION_LINK, false);
  web_ui()->GetWebContents()->OpenURL(params);
}

void AdvancedOptionsHandler::HandleDisableCloudPrintConnector(
    const ListValue* args) {
  content::RecordAction(
      UserMetricsAction("Options_DisableCloudPrintProxy"));
  CloudPrintProxyServiceFactory::GetForProfile(Profile::FromWebUI(web_ui()))->
      DisableForUser();
}

void AdvancedOptionsHandler::RefreshCloudPrintStatusFromService() {
  if (cloud_print_connector_ui_enabled_)
    CloudPrintProxyServiceFactory::GetForProfile(Profile::FromWebUI(web_ui()))->
        RefreshStatusFromService();
}

void AdvancedOptionsHandler::SetupCloudPrintConnectorSection() {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!CloudPrintProxyServiceFactory::GetForProfile(profile)) {
    cloud_print_connector_ui_enabled_ = false;
    RemoveCloudPrintConnectorSection();
    return;
  }

  bool cloud_print_connector_allowed =
      !cloud_print_connector_enabled_.IsManaged() ||
      cloud_print_connector_enabled_.GetValue();
  base::FundamentalValue allowed(cloud_print_connector_allowed);

  std::string email;
  if (profile->GetPrefs()->HasPrefPath(prefs::kCloudPrintEmail) &&
      cloud_print_connector_allowed) {
    email = profile->GetPrefs()->GetString(prefs::kCloudPrintEmail);
  }
  base::FundamentalValue disabled(email.empty());

  string16 label_str;
  if (email.empty()) {
    label_str = l10n_util::GetStringFUTF16(
        IDS_OPTIONS_CLOUD_PRINT_CONNECTOR_DISABLED_LABEL,
        l10n_util::GetStringUTF16(IDS_GOOGLE_CLOUD_PRINT));
  } else {
    label_str = l10n_util::GetStringFUTF16(
        IDS_OPTIONS_CLOUD_PRINT_CONNECTOR_ENABLED_LABEL,
        l10n_util::GetStringUTF16(IDS_GOOGLE_CLOUD_PRINT),
        UTF8ToUTF16(email));
  }
  StringValue label(label_str);

  web_ui()->CallJavascriptFunction(
      "options.AdvancedOptions.SetupCloudPrintConnectorSection",
      disabled, label, allowed);
}

void AdvancedOptionsHandler::RemoveCloudPrintConnectorSection() {
  web_ui()->CallJavascriptFunction(
      "options.AdvancedOptions.RemoveCloudPrintConnectorSection");
}

#endif

void AdvancedOptionsHandler::SetupMetricsReportingCheckbox() {
#if defined(GOOGLE_CHROME_BUILD) && !defined(OS_CHROMEOS)
  base::FundamentalValue checked(enable_metrics_recording_.GetValue());
  base::FundamentalValue disabled(enable_metrics_recording_.IsManaged());
  web_ui()->CallJavascriptFunction(
      "options.AdvancedOptions.SetMetricsReportingCheckboxState", checked,
      disabled);
#endif
}

void AdvancedOptionsHandler::SetupMetricsReportingSettingVisibility() {
#if defined(GOOGLE_CHROME_BUILD) && defined(OS_CHROMEOS)
  // Don't show the reporting setting if we are in the guest mode.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kGuestSession)) {
    base::FundamentalValue visible(false);
    web_ui()->CallJavascriptFunction(
        "options.AdvancedOptions.SetMetricsReportingSettingVisibility",
        visible);
  }
#endif
}

void AdvancedOptionsHandler::SetupFontSizeSelector() {
  // We're only interested in integer values, so convert to int.
  base::FundamentalValue font_size(default_font_size_.GetValue());
  web_ui()->CallJavascriptFunction(
      "options.AdvancedOptions.SetFontSize", font_size);
}

void AdvancedOptionsHandler::SetupPageZoomSelector() {
  PrefService* pref_service = Profile::FromWebUI(web_ui())->GetPrefs();
  double default_zoom_level = pref_service->GetDouble(prefs::kDefaultZoomLevel);
  double default_zoom_factor =
      WebKit::WebView::zoomLevelToZoomFactor(default_zoom_level);

  // Generate a vector of zoom factors from an array of known presets along with
  // the default factor added if necessary.
  std::vector<double> zoom_factors =
      chrome_page_zoom::PresetZoomFactors(default_zoom_factor);

  // Iterate through the zoom factors and and build the contents of the
  // selector that will be sent to the javascript handler.
  // Each item in the list has the following parameters:
  // 1. Title (string).
  // 2. Value (double).
  // 3. Is selected? (bool).
  ListValue zoom_factors_value;
  for (std::vector<double>::const_iterator i = zoom_factors.begin();
       i != zoom_factors.end(); ++i) {
    ListValue* option = new ListValue();
    double factor = *i;
    int percent = static_cast<int>(factor * 100 + 0.5);
    option->Append(Value::CreateStringValue(
        l10n_util::GetStringFUTF16Int(IDS_ZOOM_PERCENT, percent)));
    option->Append(Value::CreateDoubleValue(factor));
    bool selected = content::ZoomValuesEqual(factor, default_zoom_factor);
    option->Append(Value::CreateBooleanValue(selected));
    zoom_factors_value.Append(option);
  }

  web_ui()->CallJavascriptFunction(
      "options.AdvancedOptions.SetupPageZoomSelector", zoom_factors_value);
}

void AdvancedOptionsHandler::SetupAutoOpenFileTypesDisabledAttribute() {
  // Set the enabled state for the AutoOpenFileTypesResetToDefault button.
  // We enable the button if the user has any auto-open file types registered.
  DownloadManager* manager =
      web_ui()->GetWebContents()->GetBrowserContext()->GetDownloadManager();
  bool disabled = !(manager &&
      DownloadPrefs::FromDownloadManager(manager)->IsAutoOpenUsed());
  base::FundamentalValue value(disabled);
  web_ui()->CallJavascriptFunction(
      "options.AdvancedOptions.SetAutoOpenFileTypesDisabledAttribute", value);
}

void AdvancedOptionsHandler::SetupProxySettingsSection() {
#if !defined(OS_CHROMEOS)
  // Disable the button if proxy settings are managed by a sysadmin or
  // overridden by an extension.
  PrefService* pref_service = Profile::FromWebUI(web_ui())->GetPrefs();
  const PrefService::Preference* proxy_config =
      pref_service->FindPreference(prefs::kProxy);
  bool is_extension_controlled = (proxy_config &&
                                  proxy_config->IsExtensionControlled());

  base::FundamentalValue disabled(proxy_prefs_->IsManaged() ||
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

  web_ui()->CallJavascriptFunction(
      "options.AdvancedOptions.SetupProxySettingsSection", disabled, label);
#endif  // !defined(OS_CHROMEOS)
}

void AdvancedOptionsHandler::SetupSSLConfigSettings() {
  {
    base::FundamentalValue checked(rev_checking_enabled_.GetValue());
    base::FundamentalValue disabled(rev_checking_enabled_.IsManaged());
    web_ui()->CallJavascriptFunction(
        "options.AdvancedOptions.SetCheckRevocationCheckboxState", checked,
        disabled);
  }
}
