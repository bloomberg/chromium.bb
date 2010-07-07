// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/advanced_options_handler.h"

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "base/callback.h"
#include "base/values.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

AdvancedOptionsHandler::AdvancedOptionsHandler() {
}

AdvancedOptionsHandler::~AdvancedOptionsHandler() {
}

void AdvancedOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  localized_strings->SetString(L"privacyLearnMoreURL",
      l10n_util::GetString(IDS_LEARN_MORE_PRIVACY_URL));
  localized_strings->SetString(L"privacyLearnMoreLabel",
      l10n_util::GetString(IDS_OPTIONS_LEARN_MORE_LABEL));
  localized_strings->SetString(L"downloadLocationGroupName",
      l10n_util::GetString(IDS_OPTIONS_DOWNLOADLOCATION_GROUP_NAME));
  localized_strings->SetString(L"downloadLocationBrowseButton",
      l10n_util::GetString(IDS_OPTIONS_DOWNLOADLOCATION_BROWSE_BUTTON));
  localized_strings->SetString(L"downloadLocationBrowseTitle",
      l10n_util::GetString(IDS_OPTIONS_DOWNLOADLOCATION_BROWSE_TITLE));
  localized_strings->SetString(L"downloadLocationBrowseWindowTitle",
      l10n_util::GetString(IDS_OPTIONS_DOWNLOADLOCATION_BROWSE_WINDOW_TITLE));
  localized_strings->SetString(L"downloadLocationAskForSaveLocation",
      l10n_util::GetString(IDS_OPTIONS_DOWNLOADLOCATION_ASKFORSAVELOCATION));
  localized_strings->SetString(L"autoOpenFileTypesInfo",
      l10n_util::GetString(IDS_OPTIONS_AUTOOPENFILETYPES_INFO));
  localized_strings->SetString(L"autoOpenFileTypesResetToDefault",
      l10n_util::GetString(IDS_OPTIONS_AUTOOPENFILETYPES_RESETTODEFAULT));
  localized_strings->SetString(L"gearSettingsGroupName",
      l10n_util::GetString(IDS_OPTIONS_GEARSSETTINGS_GROUP_NAME));
  localized_strings->SetString(L"gearSettingsConfigureGearsButton",
      l10n_util::GetString(IDS_OPTIONS_GEARSSETTINGS_CONFIGUREGEARS_BUTTON));
  localized_strings->SetString(L"translateEnableTranslate",
      l10n_util::GetString(IDS_OPTIONS_TRANSLATE_ENABLE_TRANSLATE));
  localized_strings->SetString(L"certificatesLabel",
      l10n_util::GetString(IDS_OPTIONS_CERTIFICATES_LABEL));
  localized_strings->SetString(L"certificatesManageButton",
      l10n_util::GetString(IDS_OPTIONS_CERTIFICATES_MANAGE_BUTTON));
  localized_strings->SetString(L"proxiesLabel",
      l10n_util::GetString(IDS_OPTIONS_PROXIES_LABEL));
  localized_strings->SetString(L"proxiesConfigureButton",
      l10n_util::GetString(IDS_OPTIONS_PROXIES_CONFIGURE_BUTTON));
  localized_strings->SetString(L"safeBrowsingEnableProtection",
      l10n_util::GetString(IDS_OPTIONS_SAFEBROWSING_ENABLEPROTECTION));
  localized_strings->SetString(L"sslGroupDescription",
      l10n_util::GetString(IDS_OPTIONS_SSL_GROUP_DESCRIPTION));
  localized_strings->SetString(L"sslUseSsl2",
      l10n_util::GetString(IDS_OPTIONS_SSL_USESSL2));
  localized_strings->SetString(L"sslCheckRevocation",
      l10n_util::GetString(IDS_OPTIONS_SSL_CHECKREVOCATION));
  localized_strings->SetString(L"sslUseSsl3",
      l10n_util::GetString(IDS_OPTIONS_SSL_USESSL3));
  localized_strings->SetString(L"sslUseTsl1",
      l10n_util::GetString(IDS_OPTIONS_SSL_USETLS1));
  localized_strings->SetString(L"networkDNSPrefetchEnabledDescription",
      l10n_util::GetString(IDS_NETWORK_DNS_PREFETCH_ENABLED_DESCRIPTION));
  localized_strings->SetString(L"privacyContentSettingsButton",
      l10n_util::GetString(IDS_OPTIONS_PRIVACY_CONTENT_SETTINGS_BUTTON));
  localized_strings->SetString(L"privacyClearDataButton",
      l10n_util::GetString(IDS_OPTIONS_PRIVACY_CLEAR_DATA_BUTTON));
  localized_strings->SetString(L"linkDoctorPref",
      l10n_util::GetString(IDS_OPTIONS_LINKDOCTOR_PREF));
  localized_strings->SetString(L"suggestPref",
      l10n_util::GetString(IDS_OPTIONS_SUGGEST_PREF));
  localized_strings->SetString(L"tabsToLinksPref",
      l10n_util::GetString(IDS_OPTIONS_TABS_TO_LINKS_PREF));
  localized_strings->SetString(L"fontSettingsInfo",
      l10n_util::GetString(IDS_OPTIONS_FONTSETTINGS_INFO));
  localized_strings->SetString(L"fontSettingsConfigureFontsOnlyButton",
      l10n_util::GetString(IDS_OPTIONS_FONTSETTINGS_CONFIGUREFONTSONLY_BUTTON));
  localized_strings->SetString(L"advancedSectionTitlePrivacy",
      l10n_util::GetString(IDS_OPTIONS_ADVANCED_SECTION_TITLE_PRIVACY));
  localized_strings->SetString(L"advancedSectionTitleContent",
      l10n_util::GetString(IDS_OPTIONS_ADVANCED_SECTION_TITLE_CONTENT));
  localized_strings->SetString(L"advancedSectionTitleSecurity",
      l10n_util::GetString(IDS_OPTIONS_ADVANCED_SECTION_TITLE_SECURITY));
  localized_strings->SetString(L"advancedSectionTitleNetwork",
      l10n_util::GetString(IDS_OPTIONS_ADVANCED_SECTION_TITLE_NETWORK));
  localized_strings->SetString(L"advancedSectionTitleTranslate",
      l10n_util::GetString(IDS_OPTIONS_ADVANCED_SECTION_TITLE_TRANSLATE));
  localized_strings->SetString(L"translateEnableTranslate",
      l10n_util::GetString(IDS_OPTIONS_TRANSLATE_ENABLE_TRANSLATE));
  localized_strings->SetString(L"enableLogging",
      l10n_util::GetString(IDS_OPTIONS_ENABLE_LOGGING));
  localized_strings->SetString(L"disableServices",
      l10n_util::GetString(IDS_OPTIONS_DISABLE_SERVICES));
}

void AdvancedOptionsHandler::RegisterMessages() {
#if defined(OS_MACOSX)
  dom_ui_->RegisterMessageCallback("showNetworkProxySettings",
      NewCallback(this,
                  &AdvancedOptionsHandler::ShowNetworkProxySettings));
  dom_ui_->RegisterMessageCallback("showManageSSLCertificates",
      NewCallback(this,
                  &AdvancedOptionsHandler::ShowManageSSLCertificates));
#endif
}

#if defined(OS_MACOSX)
void AdvancedOptionsHandler::ShowNetworkProxySettings(const Value* value) {
  AdvancedOptionsUtilities::ShowNetworkProxySettings();
}

void AdvancedOptionsHandler::ShowManageSSLCertificates(const Value* value) {
  AdvancedOptionsUtilities::ShowManageSSLCertificates();
}
#endif

