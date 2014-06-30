// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/cros_language_options_handler.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/rtl.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chromeos/ime/component_extension_ime_manager.h"
#include "chromeos/ime/extension_ime_util.h"
#include "chromeos/ime/input_method_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using base::UserMetricsAction;

namespace {
// TODO(zork): Remove this blacklist when fonts are added to Chrome OS.
// see: crbug.com/240586

bool IsBlacklisted(const std::string& language_code) {
  return language_code == "si";  // Sinhala
}

}  // namespace

namespace chromeos {
namespace options {

const char kVendorOtherLanguagesListDivider[] =
    "VENDOR_OTHER_LANGUAGES_LIST_DIVIDER";

CrosLanguageOptionsHandler::CrosLanguageOptionsHandler()
    : composition_extension_appended_(false),
      is_page_initialized_(false) {
  input_method::InputMethodManager::Get()->GetComponentExtensionIMEManager()->
      AddObserver(this);
}

CrosLanguageOptionsHandler::~CrosLanguageOptionsHandler() {
  input_method::InputMethodManager::Get()->GetComponentExtensionIMEManager()->
      RemoveObserver(this);
}

void CrosLanguageOptionsHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  ::options::LanguageOptionsHandlerCommon::GetLocalizedValues(
      localized_strings);

  RegisterTitle(localized_strings, "languagePage",
                IDS_OPTIONS_SETTINGS_LANGUAGES_AND_INPUT_DIALOG_TITLE);
  localized_strings->SetString("okButton", l10n_util::GetStringUTF16(IDS_OK));
  localized_strings->SetString("configure",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_LANGUAGES_CONFIGURE));
  localized_strings->SetString("inputMethod",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_LANGUAGES_INPUT_METHOD));
  localized_strings->SetString("pleaseAddAnotherInputMethod",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_PLEASE_ADD_ANOTHER_INPUT_METHOD));
  localized_strings->SetString("inputMethodInstructions",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_INPUT_METHOD_INSTRUCTIONS));
  localized_strings->SetString("switchInputMethodsHint",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_SWITCH_INPUT_METHODS_HINT));
  localized_strings->SetString("selectPreviousInputMethodHint",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_SELECT_PREVIOUS_INPUT_METHOD_HINT));
  localized_strings->SetString("restartButton",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_SIGN_OUT_BUTTON));
  localized_strings->SetString("extensionImeLable",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_INPUT_METHOD_EXTENSION_IME));
  localized_strings->SetString("extensionImeDescription",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_INPUT_METHOD_EXTENSION_DESCRIPTION));
  localized_strings->SetString("noInputMethods",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_NO_INPUT_METHODS));

  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  // GetSupportedInputMethods() never return NULL.
  scoped_ptr<input_method::InputMethodDescriptors> descriptors(
      manager->GetSupportedInputMethods());
  localized_strings->Set("languageList", GetAcceptLanguageList(*descriptors));
  localized_strings->Set("inputMethodList", GetInputMethodList(*descriptors));

  input_method::InputMethodDescriptors ext_ime_descriptors;
  manager->GetInputMethodExtensions(&ext_ime_descriptors);

  base::ListValue* ext_ime_list = ConvertInputMethodDescriptorsToIMEList(
      ext_ime_descriptors);
  AddImeProvider(ext_ime_list);
  localized_strings->Set("extensionImeList", ext_ime_list);

  ComponentExtensionIMEManager* component_extension_manager =
      input_method::InputMethodManager::Get()
          ->GetComponentExtensionIMEManager();
  if (component_extension_manager->IsInitialized()) {
    localized_strings->Set(
        "componentExtensionImeList",
        ConvertInputMethodDescriptorsToIMEList(
            component_extension_manager->GetAllIMEAsInputMethodDescriptor()));
    composition_extension_appended_ = true;
  } else {
    // If component extension IME manager is not ready for use, it will be
    // added in |InitializePage()|.
    localized_strings->Set("componentExtensionImeList",
                           new base::ListValue());
  }
}

void CrosLanguageOptionsHandler::RegisterMessages() {
  ::options::LanguageOptionsHandlerCommon::RegisterMessages();

  web_ui()->RegisterMessageCallback("inputMethodDisable",
      base::Bind(&CrosLanguageOptionsHandler::InputMethodDisableCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("inputMethodEnable",
      base::Bind(&CrosLanguageOptionsHandler::InputMethodEnableCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("inputMethodOptionsOpen",
      base::Bind(&CrosLanguageOptionsHandler::InputMethodOptionsOpenCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("uiLanguageRestart",
      base::Bind(&CrosLanguageOptionsHandler::RestartCallback,
                 base::Unretained(this)));
}

// static
base::ListValue* CrosLanguageOptionsHandler::GetInputMethodList(
    const input_method::InputMethodDescriptors& descriptors) {
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();

  base::ListValue* input_method_list = new base::ListValue();

  for (size_t i = 0; i < descriptors.size(); ++i) {
    const input_method::InputMethodDescriptor& descriptor =
        descriptors[i];
    const std::string display_name =
        manager->GetInputMethodUtil()->GetInputMethodDisplayNameFromId(
            descriptor.id());
    base::DictionaryValue* dictionary = new base::DictionaryValue();
    dictionary->SetString("id", descriptor.id());
    dictionary->SetString("displayName", display_name);

    // One input method can be associated with multiple languages, hence
    // we use a dictionary here.
    base::DictionaryValue* languages = new base::DictionaryValue();
    for (size_t i = 0; i < descriptor.language_codes().size(); ++i) {
      languages->SetBoolean(descriptor.language_codes().at(i), true);
    }
    dictionary->Set("languageCodeSet", languages);

    input_method_list->Append(dictionary);
  }

  return input_method_list;
}

// static
base::ListValue* CrosLanguageOptionsHandler::GetLanguageListInternal(
    const input_method::InputMethodDescriptors& descriptors,
    const std::vector<std::string>& base_language_codes,
    const bool insert_divider) {
  const std::string app_locale = g_browser_process->GetApplicationLocale();

  std::set<std::string> language_codes;
  // Collect the language codes from the supported input methods.
  for (size_t i = 0; i < descriptors.size(); ++i) {
    const input_method::InputMethodDescriptor& descriptor = descriptors[i];
    const std::vector<std::string>& languages =
        descriptor.language_codes();
    for (size_t i = 0; i < languages.size(); ++i)
      language_codes.insert(languages[i]);
  }

  const StartupCustomizationDocument* startup_manifest =
      StartupCustomizationDocument::GetInstance();

  const std::vector<std::string>& configured_locales =
      startup_manifest->configured_locales();

  // Languages sort order.
  std::map<std::string, int /* index */> language_index;
  for (size_t i = 0; i < configured_locales.size(); ++i) {
    language_index[configured_locales[i]] = i;
  }

  // Map of display name -> {language code, native_display_name}.
  // In theory, we should be able to create a map that is sorted by
  // display names using ICU comparator, but doing it is hard, thus we'll
  // use an auxiliary vector to achieve the same result.
  typedef std::pair<std::string, base::string16> LanguagePair;
  typedef std::map<base::string16, LanguagePair> LanguageMap;
  LanguageMap language_map;

  // The auxiliary vector mentioned above. (except vendor locales)
  std::vector<base::string16> display_names;

  // Separate vector of vendor locales.
  std::vector<base::string16> configured_locales_display_names(
      configured_locales.size());

  size_t configured_locales_count = 0;

  // Build the list of display names, and build the language map.

  // The list of configured locales might have entries not in
  // base_language_codes. If there are unsupported language variants,
  // but they resolve to backup locale within base_language_codes, also
  // add them to the list.
  for (std::map<std::string, int>::const_iterator iter = language_index.begin();
       iter != language_index.end();
       ++iter) {
    const std::string& language_id = iter->first;
    const int language_idx = iter->second;

    const size_t dash_pos = language_id.find_first_of('-');

    // Ignore non-specific codes.
    if (dash_pos == std::string::npos || dash_pos == 0)
      continue;

    if (std::find(base_language_codes.begin(),
                  base_language_codes.end(),
                  language_id) != base_language_codes.end()) {
      // Language is supported. No need to replace
      continue;
    }
    std::string resolved_locale;
    if (!l10n_util::CheckAndResolveLocale(language_id, &resolved_locale))
      continue;

    if (std::find(base_language_codes.begin(),
                  base_language_codes.end(),
                  resolved_locale) == base_language_codes.end()) {
      // Resolved locale is not supported.
      continue;
    }

    const base::string16 display_name =
        l10n_util::GetDisplayNameForLocale(language_id, app_locale, true);
    const base::string16 native_display_name =
        l10n_util::GetDisplayNameForLocale(
            language_id, language_id, true);

    language_map[display_name] =
        std::make_pair(language_id, native_display_name);

    configured_locales_display_names[language_idx] = display_name;
    ++configured_locales_count;
  }

  // Translate language codes, generated from input methods.
  for (std::set<std::string>::const_iterator iter = language_codes.begin();
       iter != language_codes.end(); ++iter) {
     // Exclude the language which is not in |base_langauge_codes| even it has
     // input methods.
    if (std::find(base_language_codes.begin(),
                  base_language_codes.end(),
                  *iter) == base_language_codes.end()) {
      continue;
    }

    const base::string16 display_name =
        l10n_util::GetDisplayNameForLocale(*iter, app_locale, true);
    const base::string16 native_display_name =
        l10n_util::GetDisplayNameForLocale(*iter, *iter, true);

    language_map[display_name] =
        std::make_pair(*iter, native_display_name);

    const std::map<std::string, int>::const_iterator index_pos =
        language_index.find(*iter);
    if (index_pos != language_index.end()) {
      base::string16& stored_display_name =
          configured_locales_display_names[index_pos->second];
      if (stored_display_name.empty()) {
        stored_display_name = display_name;
        ++configured_locales_count;
      }
    } else {
      display_names.push_back(display_name);
    }
  }
  DCHECK_EQ(display_names.size() + configured_locales_count,
            language_map.size());

  // Build the list of display names, and build the language map.
  for (size_t i = 0; i < base_language_codes.size(); ++i) {
    // Skip this language if it was already added.
    if (language_codes.find(base_language_codes[i]) != language_codes.end())
      continue;

    // TODO(zork): Remove this blacklist when fonts are added to Chrome OS.
    // see: crbug.com/240586
    if (IsBlacklisted(base_language_codes[i]))
      continue;

    base::string16 display_name =
        l10n_util::GetDisplayNameForLocale(
            base_language_codes[i], app_locale, false);
    base::string16 native_display_name =
        l10n_util::GetDisplayNameForLocale(
            base_language_codes[i], base_language_codes[i], false);
    language_map[display_name] =
        std::make_pair(base_language_codes[i], native_display_name);

    const std::map<std::string, int>::const_iterator index_pos =
        language_index.find(base_language_codes[i]);
    if (index_pos != language_index.end()) {
      configured_locales_display_names[index_pos->second] = display_name;
      ++configured_locales_count;
    } else {
      display_names.push_back(display_name);
    }
  }

  // Sort display names using locale specific sorter.
  l10n_util::SortStrings16(app_locale, &display_names);
  // Concatenate configured_locales_display_names and display_names.
  // Insert special divider in between.
  std::vector<base::string16> out_display_names;
  for (size_t i = 0; i < configured_locales_display_names.size(); ++i) {
    if (configured_locales_display_names[i].size() == 0)
      continue;
    out_display_names.push_back(configured_locales_display_names[i]);
  }

  base::string16 divider16;
  if (insert_divider) {
    divider16 = base::ASCIIToUTF16(
        insert_divider ? "" : kVendorOtherLanguagesListDivider);
    out_display_names.push_back(divider16);
  }

  std::copy(display_names.begin(),
            display_names.end(),
            std::back_inserter(out_display_names));

  // Build the language list from the language map.
  base::ListValue* language_list = new base::ListValue();
  for (size_t i = 0; i < out_display_names.size(); ++i) {
    // Sets the directionality of the display language name.
    base::string16 display_name(out_display_names[i]);
    if (insert_divider && display_name == divider16) {
      // Insert divider.
      base::DictionaryValue* dictionary = new base::DictionaryValue();
      dictionary->SetString("code", kVendorOtherLanguagesListDivider);
      language_list->Append(dictionary);
      continue;
    }
    bool markup_removal =
        base::i18n::UnadjustStringForLocaleDirection(&display_name);
    DCHECK(markup_removal);
    bool has_rtl_chars = base::i18n::StringContainsStrongRTLChars(display_name);
    std::string directionality = has_rtl_chars ? "rtl" : "ltr";

    const LanguagePair& pair = language_map[out_display_names[i]];
    base::DictionaryValue* dictionary = new base::DictionaryValue();
    dictionary->SetString("code", pair.first);
    dictionary->SetString("displayName", out_display_names[i]);
    dictionary->SetString("textDirection", directionality);
    dictionary->SetString("nativeDisplayName", pair.second);
    language_list->Append(dictionary);
  }

  return language_list;
}

// static
base::ListValue* CrosLanguageOptionsHandler::GetAcceptLanguageList(
    const input_method::InputMethodDescriptors& descriptors) {
  // Collect the language codes from the supported accept-languages.
  const std::string app_locale = g_browser_process->GetApplicationLocale();
  std::vector<std::string> accept_language_codes;
  l10n_util::GetAcceptLanguagesForLocale(app_locale, &accept_language_codes);
  return GetLanguageListInternal(descriptors, accept_language_codes, false);
}

// static
base::ListValue* CrosLanguageOptionsHandler::GetUILanguageList(
    const input_method::InputMethodDescriptors& descriptors) {
  // Collect the language codes from the available locales.
  return GetLanguageListInternal(
      descriptors, l10n_util::GetAvailableLocales(), true);
}

base::ListValue*
    CrosLanguageOptionsHandler::ConvertInputMethodDescriptorsToIMEList(
        const input_method::InputMethodDescriptors& descriptors) {
  scoped_ptr<base::ListValue> ime_ids_list(new base::ListValue());
  for (size_t i = 0; i < descriptors.size(); ++i) {
    const input_method::InputMethodDescriptor& descriptor = descriptors[i];
    scoped_ptr<base::DictionaryValue> dictionary(new base::DictionaryValue());
    dictionary->SetString("id", descriptor.id());
    dictionary->SetString("displayName", descriptor.name());
    dictionary->SetString("optionsPage", descriptor.options_page_url().spec());
    scoped_ptr<base::DictionaryValue> language_codes(
        new base::DictionaryValue());
    for (size_t i = 0; i < descriptor.language_codes().size(); ++i)
      language_codes->SetBoolean(descriptor.language_codes().at(i), true);
    dictionary->Set("languageCodeSet", language_codes.release());
    ime_ids_list->Append(dictionary.release());
  }
  return ime_ids_list.release();
}

base::string16 CrosLanguageOptionsHandler::GetProductName() {
  return l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_OS_NAME);
}

void CrosLanguageOptionsHandler::SetApplicationLocale(
    const std::string& language_code) {
  Profile* profile = Profile::FromWebUI(web_ui());
  UserManager* user_manager = UserManager::Get();

  // Only the primary user can change the locale.
  User* user = ProfileHelper::Get()->GetUserByProfile(profile);
  if (user && user->email() == user_manager->GetPrimaryUser()->email()) {
    profile->ChangeAppLocale(language_code,
                             Profile::APP_LOCALE_CHANGED_VIA_SETTINGS);
  }
}

void CrosLanguageOptionsHandler::RestartCallback(const base::ListValue* args) {
  content::RecordAction(UserMetricsAction("LanguageOptions_SignOut"));
  chrome::AttemptUserExit();
}

void CrosLanguageOptionsHandler::InputMethodDisableCallback(
    const base::ListValue* args) {
  const std::string input_method_id =
      base::UTF16ToASCII(ExtractStringValue(args));
  const std::string action = base::StringPrintf(
      "LanguageOptions_DisableInputMethod_%s", input_method_id.c_str());
  content::RecordComputedAction(action);
}

void CrosLanguageOptionsHandler::InputMethodEnableCallback(
    const base::ListValue* args) {
  const std::string input_method_id =
      base::UTF16ToASCII(ExtractStringValue(args));
  const std::string action = base::StringPrintf(
      "LanguageOptions_EnableInputMethod_%s", input_method_id.c_str());
  content::RecordComputedAction(action);
}

void CrosLanguageOptionsHandler::InputMethodOptionsOpenCallback(
    const base::ListValue* args) {
  const std::string input_method_id =
      base::UTF16ToASCII(ExtractStringValue(args));
  const std::string extension_id =
      extension_ime_util::GetExtensionIDFromInputMethodID(input_method_id);
  if (extension_id.empty())
    return;

  const input_method::InputMethodDescriptor* ime =
      input_method::InputMethodManager::Get()->GetInputMethodFromId(
          input_method_id);
  if (!ime)
    return;

  Browser* browser = chrome::FindBrowserWithWebContents(
      web_ui()->GetWebContents());
  content::OpenURLParams params(ime->options_page_url(),
      content::Referrer(),
      SINGLETON_TAB,
      content::PAGE_TRANSITION_LINK,
      false);
  browser->OpenURL(params);
  browser->window()->Show();
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  web_contents->GetDelegate()->ActivateContents(web_contents);
}

void CrosLanguageOptionsHandler::OnImeComponentExtensionInitialized() {
  if (composition_extension_appended_ || !is_page_initialized_) {
    // If an option page is not ready to call JavaScript, appending component
    // extension IMEs will be done in InitializePage function later.
    return;
  }

  ComponentExtensionIMEManager* manager =
      input_method::InputMethodManager::Get()
          ->GetComponentExtensionIMEManager();

  DCHECK(manager->IsInitialized());
  scoped_ptr<base::ListValue> ime_list(
      ConvertInputMethodDescriptorsToIMEList(
          manager->GetAllIMEAsInputMethodDescriptor()));
  web_ui()->CallJavascriptFunction(
      "options.LanguageOptions.onComponentManagerInitialized",
      *ime_list);
  composition_extension_appended_ = true;
}

void CrosLanguageOptionsHandler::InitializePage() {
  is_page_initialized_ = true;
  if (composition_extension_appended_)
    return;

  ComponentExtensionIMEManager* component_extension_manager =
      input_method::InputMethodManager::Get()
          ->GetComponentExtensionIMEManager();
  if (!component_extension_manager->IsInitialized()) {
    // If the component extension IME manager is not available yet, append the
    // component extension list in |OnInitialized()|.
    return;
  }

  scoped_ptr<base::ListValue> ime_list(
      ConvertInputMethodDescriptorsToIMEList(
          component_extension_manager->GetAllIMEAsInputMethodDescriptor()));
  web_ui()->CallJavascriptFunction(
      "options.LanguageOptions.onComponentManagerInitialized",
      *ime_list);
  composition_extension_appended_ = true;
}

void CrosLanguageOptionsHandler::AddImeProvider(base::ListValue* list) {
  Profile* profile = Profile::FromWebUI(web_ui());
  const extensions::ExtensionSet& enabled_extensions =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  for (size_t i = 0; i < list->GetSize(); ++i) {
    base::DictionaryValue* entry;
    list->GetDictionary(i, &entry);

    std::string input_method_id;
    entry->GetString("id", &input_method_id);

    std::string extension_id =
        extension_ime_util::GetExtensionIDFromInputMethodID(input_method_id);
    const extensions::Extension* extension =
        enabled_extensions.GetByID(extension_id);
    if (extension)
      entry->SetString("extensionName", extension->name());
  }
}

}  // namespace options
}  // namespace chromeos
