// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/font_settings_handler.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/options/font_settings_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/font_list_async.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

const char kAdvancedFontSettingsExtensionId[] =
    "caclkomlalccbpcdllchkeecicepbmbm";

}  // namespace


namespace options {

FontSettingsHandler::FontSettingsHandler()
    : extension_registry_observer_(this),
      weak_ptr_factory_(this) {
}

FontSettingsHandler::~FontSettingsHandler() {
}

void FontSettingsHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    { "fontSettingsStandard",
      IDS_FONT_LANGUAGE_SETTING_FONT_SELECTOR_STANDARD_LABEL },
    { "fontSettingsSerif",
      IDS_FONT_LANGUAGE_SETTING_FONT_SELECTOR_SERIF_LABEL },
    { "fontSettingsSansSerif",
      IDS_FONT_LANGUAGE_SETTING_FONT_SELECTOR_SANS_SERIF_LABEL },
    { "fontSettingsFixedWidth",
      IDS_FONT_LANGUAGE_SETTING_FONT_SELECTOR_FIXED_WIDTH_LABEL },
    { "fontSettingsMinimumSize",
      IDS_FONT_LANGUAGE_SETTING_MINIMUM_FONT_SIZE_TITLE },
    { "fontSettings",
      IDS_FONT_LANGUAGE_SETTING_FONT_SUB_DIALOG_TITLE },
    { "fontSettingsSizeTiny",
      IDS_FONT_LANGUAGE_SETTING_FONT_SIZE_TINY },
    { "fontSettingsSizeHuge",
      IDS_FONT_LANGUAGE_SETTING_FONT_SIZE_HUGE },
    { "fontSettingsLoremIpsum",
      IDS_FONT_LANGUAGE_SETTING_LOREM_IPSUM },
    { "advancedFontSettingsOptions",
      IDS_FONT_LANGUAGE_SETTING_ADVANCED_FONT_SETTINGS_OPTIONS }
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "fontSettingsPage",
                IDS_FONT_LANGUAGE_SETTING_FONT_TAB_TITLE);
  localized_strings->SetString("fontSettingsPlaceholder",
      l10n_util::GetStringUTF16(
          IDS_FONT_LANGUAGE_SETTING_PLACEHOLDER));

  GURL install_url(extension_urls::GetWebstoreItemDetailURLPrefix());
  localized_strings->SetString("advancedFontSettingsInstall",
      l10n_util::GetStringFUTF16(
          IDS_FONT_LANGUAGE_SETTING_ADVANCED_FONT_SETTINGS_INSTALL,
          base::UTF8ToUTF16(
              install_url.Resolve(kAdvancedFontSettingsExtensionId).spec())));
}

void FontSettingsHandler::InitializeHandler() {
  Profile* profile = Profile::FromWebUI(web_ui());
  extension_registry_observer_.Add(extensions::ExtensionRegistry::Get(profile));
}

void FontSettingsHandler::InitializePage() {
  DCHECK(web_ui());
  SetUpStandardFontSample();
  SetUpSerifFontSample();
  SetUpSansSerifFontSample();
  SetUpFixedFontSample();
  SetUpMinimumFontSample();
  NotifyAdvancedFontSettingsAvailability();
}

void FontSettingsHandler::RegisterMessages() {
  // Perform validation for saved fonts.
  PrefService* pref_service = Profile::FromWebUI(web_ui())->GetPrefs();
  FontSettingsUtilities::ValidateSavedFonts(pref_service);

  // Register for preferences that we need to observe manually.
  standard_font_.Init(prefs::kWebKitStandardFontFamily,
                      pref_service,
                      base::Bind(&FontSettingsHandler::SetUpStandardFontSample,
                                 base::Unretained(this)));
  serif_font_.Init(prefs::kWebKitSerifFontFamily,
                   pref_service,
                   base::Bind(&FontSettingsHandler::SetUpSerifFontSample,
                              base::Unretained(this)));
  sans_serif_font_.Init(
      prefs::kWebKitSansSerifFontFamily,
      pref_service,
      base::Bind(&FontSettingsHandler::SetUpSansSerifFontSample,
                 base::Unretained(this)));

  base::Closure callback = base::Bind(
      &FontSettingsHandler::SetUpFixedFontSample, base::Unretained(this));

  fixed_font_.Init(prefs::kWebKitFixedFontFamily, pref_service, callback);
  default_fixed_font_size_.Init(prefs::kWebKitDefaultFixedFontSize,
                                pref_service, callback);
  default_font_size_.Init(
      prefs::kWebKitDefaultFontSize,
      pref_service,
      base::Bind(&FontSettingsHandler::OnWebKitDefaultFontSizeChanged,
                 base::Unretained(this)));
  minimum_font_size_.Init(
      prefs::kWebKitMinimumFontSize,
      pref_service,
      base::Bind(&FontSettingsHandler::SetUpMinimumFontSample,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback("fetchFontsData",
      base::Bind(&FontSettingsHandler::HandleFetchFontsData,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("openAdvancedFontSettingsOptions",
      base::Bind(&FontSettingsHandler::HandleOpenAdvancedFontSettingsOptions,
                 base::Unretained(this)));
}

void FontSettingsHandler::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  NotifyAdvancedFontSettingsAvailability();
}

void FontSettingsHandler::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionInfo::Reason reason) {
  NotifyAdvancedFontSettingsAvailability();
}

void FontSettingsHandler::HandleFetchFontsData(const base::ListValue* args) {
  content::GetFontListAsync(
      base::Bind(&FontSettingsHandler::FontsListHasLoaded,
                 weak_ptr_factory_.GetWeakPtr()));
}

void FontSettingsHandler::FontsListHasLoaded(
    std::unique_ptr<base::ListValue> list) {
  // Selects the directionality for the fonts in the given list.
  for (size_t i = 0; i < list->GetSize(); i++) {
    base::ListValue* font;
    bool has_font = list->GetList(i, &font);
    DCHECK(has_font);
    base::string16 value;
    bool has_value = font->GetString(1, &value);
    DCHECK(has_value);
    bool has_rtl_chars = base::i18n::StringContainsStrongRTLChars(value);
    font->AppendString(has_rtl_chars ? "rtl" : "ltr");
  }

  base::ListValue selected_values;
  selected_values.AppendString(FontSettingsUtilities::MaybeGetLocalizedFontName(
      standard_font_.GetValue()));
  selected_values.AppendString(
      FontSettingsUtilities::MaybeGetLocalizedFontName(serif_font_.GetValue()));
  selected_values.AppendString(FontSettingsUtilities::MaybeGetLocalizedFontName(
      sans_serif_font_.GetValue()));
  selected_values.AppendString(
      FontSettingsUtilities::MaybeGetLocalizedFontName(fixed_font_.GetValue()));

  web_ui()->CallJavascriptFunctionUnsafe(
      "FontSettings.setFontsData", *list.get(), selected_values);
}

void FontSettingsHandler::SetUpStandardFontSample() {
  base::StringValue font_value(
      FontSettingsUtilities::ResolveFontList(standard_font_.GetValue()));
  base::Value size_value(default_font_size_.GetValue());
  web_ui()->CallJavascriptFunctionUnsafe("FontSettings.setUpStandardFontSample",
                                         font_value, size_value);
}

void FontSettingsHandler::SetUpSerifFontSample() {
  base::StringValue font_value(
      FontSettingsUtilities::ResolveFontList(serif_font_.GetValue()));
  base::Value size_value(default_font_size_.GetValue());
  web_ui()->CallJavascriptFunctionUnsafe("FontSettings.setUpSerifFontSample",
                                         font_value, size_value);
}

void FontSettingsHandler::SetUpSansSerifFontSample() {
  base::StringValue font_value(
      FontSettingsUtilities::ResolveFontList(sans_serif_font_.GetValue()));
  base::Value size_value(default_font_size_.GetValue());
  web_ui()->CallJavascriptFunctionUnsafe(
      "FontSettings.setUpSansSerifFontSample", font_value, size_value);
}

void FontSettingsHandler::SetUpFixedFontSample() {
  base::StringValue font_value(
      FontSettingsUtilities::ResolveFontList(fixed_font_.GetValue()));
  base::Value size_value(default_fixed_font_size_.GetValue());
  web_ui()->CallJavascriptFunctionUnsafe("FontSettings.setUpFixedFontSample",
                                         font_value, size_value);
}

void FontSettingsHandler::SetUpMinimumFontSample() {
  base::Value size_value(minimum_font_size_.GetValue());
  web_ui()->CallJavascriptFunctionUnsafe("FontSettings.setUpMinimumFontSample",
                                         size_value);
}

const extensions::Extension*
FontSettingsHandler::GetAdvancedFontSettingsExtension() {
  Profile* profile = Profile::FromWebUI(web_ui());
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!service->IsExtensionEnabled(kAdvancedFontSettingsExtensionId))
    return NULL;
  return service->GetInstalledExtension(kAdvancedFontSettingsExtensionId);
}

void FontSettingsHandler::NotifyAdvancedFontSettingsAvailability() {
  web_ui()->CallJavascriptFunctionUnsafe(
      "FontSettings.notifyAdvancedFontSettingsAvailability",
      base::Value(GetAdvancedFontSettingsExtension() != NULL));
}

void FontSettingsHandler::HandleOpenAdvancedFontSettingsOptions(
    const base::ListValue* args) {
  const extensions::Extension* extension = GetAdvancedFontSettingsExtension();
  if (!extension)
    return;
  extensions::ExtensionTabUtil::OpenOptionsPage(extension,
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents()));
}

void FontSettingsHandler::OnWebKitDefaultFontSizeChanged() {
  SetUpStandardFontSample();
  SetUpSerifFontSample();
  SetUpSansSerifFontSample();
}

}  // namespace options
