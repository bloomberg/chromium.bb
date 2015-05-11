// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/md_settings_localized_strings_provider.h"

#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {

// Note that md_settings.html contains a <script> tag which imports a script of
// the following name. These names must be kept in sync.
const char kLocalizedStringsFile[] = "strings.js";

void AddA11yStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString(
      "a11yPageTitle", IDS_MD_SETTINGS_ACCESSIBILITY_PAGE_TITLE);
  html_source->AddLocalizedString(
      "accessibilityMoreFeaturesLink",
      IDS_SETTINGS_ACCESSIBILITY_MORE_FEATURES_LINK);
  html_source->AddLocalizedString(
      "accessibilityOptionsInMenuLabel",
      IDS_SETTINGS_ACCESSIBILITY_OPTIONS_IN_MENU_LABEL);
  html_source->AddLocalizedString(
      "accessibilityLargeMouseCursorLabel",
      IDS_SETTINGS_ACCESSIBILITY_LARGE_MOUSE_CURSOR_LABEL);
  html_source->AddLocalizedString(
      "accessibilityHighContrastLabel",
      IDS_SETTINGS_ACCESSIBILITY_HIGH_CONTRAST_LABEL);
  html_source->AddLocalizedString(
      "accessibilityStickyKeysLabel",
      IDS_SETTINGS_ACCESSIBILITY_STICKY_KEYS_LABEL);
  html_source->AddLocalizedString(
      "accessibilityStickyKeysSublabel",
      IDS_SETTINGS_ACCESSIBILITY_STICKY_KEYS_SUBLABEL);
  html_source->AddLocalizedString(
      "accessibilityChromeVoxLabel",
      IDS_SETTINGS_ACCESSIBILITY_CHROMEVOX_LABEL);
  html_source->AddLocalizedString(
      "accessibilityChromeVoxSublabel",
      IDS_SETTINGS_ACCESSIBILITY_CHROMEVOX_SUBLABEL);
  html_source->AddLocalizedString(
      "accessibilityScreenMagnifierLabel",
      IDS_SETTINGS_ACCESSIBILITY_SCREEN_MAGNIFIER_LABEL);
  html_source->AddLocalizedString(
      "accessibilityTapDraggingLabel",
      IDS_SETTINGS_ACCESSIBILITY_TAP_DRAGGING_LABEL);
  html_source->AddLocalizedString(
      "accessibilityClickOnStopLabel",
      IDS_SETTINGS_ACCESSIBILITY_CLICK_ON_STOP_LABEL);
  html_source->AddLocalizedString(
      "accessibilityDelayBeforeClickLabel",
      IDS_SETTINGS_ACCESSIBILITY_DELAY_BEFORE_CLICK_LABEL);
  html_source->AddLocalizedString(
      "accessibilityDelayBeforeClickExtremelyShort",
      IDS_SETTINGS_ACCESSIBILITY_DELAY_BEFORE_CLICK_EXTREMELY_SHORT);
  html_source->AddLocalizedString(
      "accessibilityDelayBeforeClickVeryShort",
      IDS_SETTINGS_ACCESSIBILITY_DELAY_BEFORE_CLICK_VERY_SHORT);
  html_source->AddLocalizedString(
      "accessibilityDelayBeforeClickShort",
      IDS_SETTINGS_ACCESSIBILITY_DELAY_BEFORE_CLICK_SHORT);
  html_source->AddLocalizedString(
      "accessibilityDelayBeforeClickLong",
      IDS_SETTINGS_ACCESSIBILITY_DELAY_BEFORE_CLICK_LONG);
  html_source->AddLocalizedString(
      "accessibilityDelayBeforeClickVeryLong",
      IDS_SETTINGS_ACCESSIBILITY_DELAY_BEFORE_CLICK_VERY_LONG);
  html_source->AddLocalizedString(
      "accessibilityOnScreenKeyboardLabel",
      IDS_SETTINGS_ACCESSIBILITY_ON_SCREEN_KEYBOARD_LABEL);
}

void AddDownloadsStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString(
      "downloadsPageTitle", IDS_MD_SETTINGS_DOWNLOADS_PAGE_TITLE);
  html_source->AddLocalizedString(
      "downloadsLocationLabel", IDS_SETTINGS_DOWNLOADS_LOCATION_LABEL);
  html_source->AddLocalizedString(
      "downloadsChangeLocationButton",
      IDS_SETTINGS_DOWNLOADS_CHANGE_LOCATION_BUTTON);
  html_source->AddLocalizedString(
      "downloadsPromptForDownloadLabel",
      IDS_SETTINGS_DOWNLOADS_PROMPT_FOR_DOWNLOAD_LABEL);
}

void AddDateTimeStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString(
      "dateTimePageTitle",
      IDS_SETTINGS_DATE_TIME_PAGE_TITLE);
  html_source->AddLocalizedString(
      "dateTimeTimeZoneLabel",
      IDS_SETTINGS_DATE_TIME_TIME_ZONE_LABEL);
  html_source->AddLocalizedString(
      "dateTime24HourClockLabel",
      IDS_SETTINGS_DATE_TIME_24_HOUR_CLOCK_LABEL);
  html_source->AddLocalizedString(
      "dateTimeAutomaticallySet",
      IDS_SETTINGS_DATE_TIME_AUTOMATICALLY_SET);
}

#if defined(OS_CHROMEOS)
void AddInternetStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString(
      "internetPageTitle", IDS_SETTINGS_INTERNET_PAGE_TITLE);
  html_source->AddLocalizedString(
      "internetDetailPageTitle", IDS_SETTINGS_INTERNET_DETAIL_PAGE_TITLE);
}
#endif

void AddSearchStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString("searchPageTitle",
                                  IDS_SETTINGS_SEARCH_PAGE_TITLE);
  html_source->AddLocalizedString("searchExplanation",
                                  IDS_SETTINGS_SEARCH_EXPLANATION);
  html_source->AddLocalizedString("searchManageButtonLabel",
                                  IDS_SETTINGS_SEARCH_MANAGE_BUTTON_LABEL);
  html_source->AddLocalizedString("searchOkGoogleLabel",
                                  IDS_SETTINGS_SEARCH_OK_GOOGLE_LABEL);
  html_source->AddLocalizedString(
      "searchOkGoogleLearnMoreLink",
      IDS_SETTINGS_SEARCH_OK_GOOGLE_LEARN_MORE_LINK);
  html_source->AddLocalizedString(
      "searchOkGoogleDescriptionLabel",
      IDS_SETTINGS_SEARCH_OK_GOOGLE_DESCRIPTION_LABEL);
}

void AddSearchEnginesStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString("searchEnginesPageTitle",
                                  IDS_SETTINGS_SEARCH_ENGINES_PAGE_TITLE);
  html_source->AddLocalizedString(
      "searchEnginesAddSearchEngineLabel",
      IDS_SETTINGS_SEARCH_ENGINES_ADD_SEARCH_ENGINE_LABEL);
  html_source->AddLocalizedString("searchEnginesLabel",
                                  IDS_SETTINGS_SEARCH_ENGINES_LABEL);
  html_source->AddLocalizedString(
      "searchEnginesOtherLabel",
      IDS_SETTINGS_SEARCH_ENGINES_OTHER_ENGINES_LABEL);
  html_source->AddLocalizedString("searchEnginesDomainLabel",
                                  IDS_SETTINGS_SEARCH_ENGINES_DOMAIN_LABEL);
  html_source->AddLocalizedString("searchEnginesKeywordLabel",
                                  IDS_SETTINGS_SEARCH_ENGINES_KEYWORD_LABEL);
  html_source->AddLocalizedString("searchEnginesQueryURLLabel",
                                  IDS_SETTINGS_SEARCH_ENGINES_QUERY_URL_LABEL);
  html_source->AddLocalizedString("searchEnginesAddButtonLabel",
                                  IDS_SETTINGS_SEARCH_ENGINES_ADD_BUTTON_LABEL);
}

}  // namespace

namespace settings {

void AddLocalizedStrings(content::WebUIDataSource* html_source) {
  AddA11yStrings(html_source);
  AddDownloadsStrings(html_source);
  AddDateTimeStrings(html_source);
#if defined(OS_CHROMEOS)
  AddInternetStrings(html_source);
#endif
  AddSearchStrings(html_source);
  AddSearchEnginesStrings(html_source);
  html_source->SetJsonPath(kLocalizedStringsFile);
}

}  // namespace settings
