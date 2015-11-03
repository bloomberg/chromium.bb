// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/language_settings_private/language_settings_private_api.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/i18n/rtl.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/language_settings_private/language_settings_private_delegate.h"
#include "chrome/browser/extensions/api/language_settings_private/language_settings_private_delegate_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/common/extensions/api/language_settings_private.h"
#include "chrome/common/spellcheck_common.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_collator.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#endif

namespace extensions {

namespace language_settings_private = api::language_settings_private;

#if defined(OS_CHROMEOS)
using chromeos::input_method::InputMethodDescriptors;
using chromeos::input_method::InputMethodManager;
using chromeos::input_method::InputMethodUtil;
#endif

LanguageSettingsPrivateGetLanguageListFunction::
    LanguageSettingsPrivateGetLanguageListFunction() {
}

LanguageSettingsPrivateGetLanguageListFunction::
    ~LanguageSettingsPrivateGetLanguageListFunction() {
}

ExtensionFunction::ResponseAction
LanguageSettingsPrivateGetLanguageListFunction::Run() {
  // Collect the language codes from the supported accept-languages.
  const std::string app_locale = g_browser_process->GetApplicationLocale();
  std::vector<std::string> language_codes;
  l10n_util::GetAcceptLanguagesForLocale(app_locale, &language_codes);

  // Map of display name -> {language code, native display name}.
  typedef std::pair<std::string, base::string16> LanguagePair;
  typedef std::map<base::string16, LanguagePair,
      l10n_util::StringComparator<base::string16>> LanguageMap;

  // Collator used to sort display names in the current locale.
  UErrorCode error = U_ZERO_ERROR;
  scoped_ptr<icu::Collator> collator(icu::Collator::createInstance(
      icu::Locale(app_locale.c_str()), error));
  if (U_FAILURE(error))
    collator.reset();
  LanguageMap language_map(
      l10n_util::StringComparator<base::string16>(collator.get()));

  // Build the list of display names and the language map.
  for (const auto& code : language_codes) {
    base::string16 display_name = l10n_util::GetDisplayNameForLocale(
        code, app_locale, false);
    base::string16 native_display_name = l10n_util::GetDisplayNameForLocale(
        code, code, false);
    language_map[display_name] = std::make_pair(code, native_display_name);
  }

  // Get the list of available locales (display languages) and convert to a set.
  const std::vector<std::string>& locales = l10n_util::GetAvailableLocales();
  const base::hash_set<std::string> locale_set(
      locales.begin(), locales.end());

  // Get the list of spell check languages and convert to a set.
  std::vector<std::string> spellcheck_languages;
  chrome::spellcheck_common::SpellCheckLanguages(&spellcheck_languages);
  const base::hash_set<std::string> spellcheck_language_set(
      spellcheck_languages.begin(), spellcheck_languages.end());

  // Get the list of translatable languages and convert to a set.
  std::vector<std::string> translate_languages;
  translate::TranslateDownloadManager::GetSupportedLanguages(
      &translate_languages);
  const base::hash_set<std::string> translate_language_set(
      translate_languages.begin(), translate_languages.end());

  // Build the language list from the language map.
  scoped_ptr<base::ListValue> language_list(new base::ListValue);
  for (const auto& entry : language_map) {
    const base::string16& display_name = entry.first;
    const LanguagePair& pair = entry.second;

    language_settings_private::Language language;
    language.code = pair.first;

    base::string16 adjusted_display_name(display_name);
    base::i18n::AdjustStringForLocaleDirection(&adjusted_display_name);
    language.display_name = base::UTF16ToUTF8(adjusted_display_name);

    base::string16 adjusted_native_display_name(pair.second);
    base::i18n::AdjustStringForLocaleDirection(&adjusted_native_display_name);
    language.native_display_name =
        base::UTF16ToUTF8(adjusted_native_display_name);

    // Set optional fields only if they differ from the default.
    if (base::i18n::StringContainsStrongRTLChars(display_name))
      language.display_name_rtl.reset(new bool(true));
    if (locale_set.count(pair.first) > 0)
      language.supports_ui.reset(new bool(true));
    if (spellcheck_language_set.count(pair.first) > 0)
      language.supports_spellcheck.reset(new bool(true));
    if (translate_language_set.count(pair.first) > 0)
      language.supports_translate.reset(new bool(true));

    language_list->Append(language.ToValue());
  }
  return RespondNow(OneArgument(language_list.release()));
}

LanguageSettingsPrivateSetLanguageListFunction::
    LanguageSettingsPrivateSetLanguageListFunction()
    : chrome_details_(this) {
}

LanguageSettingsPrivateSetLanguageListFunction::
    ~LanguageSettingsPrivateSetLanguageListFunction() {
}

ExtensionFunction::ResponseAction
LanguageSettingsPrivateSetLanguageListFunction::Run() {
  scoped_ptr<language_settings_private::SetLanguageList::Params> parameters =
      language_settings_private::SetLanguageList::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  scoped_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(
          chrome_details_.GetProfile()->GetPrefs());
  translate_prefs->UpdateLanguageList(parameters->language_codes);

  return RespondNow(NoArguments());
}

LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction::
    LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction() {
}

LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction::
    ~LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction() {
}

ExtensionFunction::ResponseAction
LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction::Run() {
  LanguageSettingsPrivateDelegate* delegate =
      LanguageSettingsPrivateDelegateFactory::GetForBrowserContext(
          browser_context());

  scoped_ptr<base::ListValue> return_list(new base::ListValue());
  for (const auto& status : delegate->GetHunspellDictionaryStatuses())
    return_list->Append(status->ToValue());
  return RespondNow(OneArgument(return_list.release()));
}

LanguageSettingsPrivateGetSpellcheckWordsFunction::
    LanguageSettingsPrivateGetSpellcheckWordsFunction() {
}

LanguageSettingsPrivateGetSpellcheckWordsFunction::
    ~LanguageSettingsPrivateGetSpellcheckWordsFunction() {
}

ExtensionFunction::ResponseAction
LanguageSettingsPrivateGetSpellcheckWordsFunction::Run() {
  SpellcheckService* service =
      SpellcheckServiceFactory::GetForContext(browser_context());
  SpellcheckCustomDictionary* dictionary = service->GetCustomDictionary();

  if (dictionary->IsLoaded())
    return RespondNow(OneArgument(GetSpellcheckWords().release()));

  dictionary->AddObserver(this);
  AddRef();  // Balanced in OnCustomDictionaryLoaded().
  return RespondLater();
}

void
LanguageSettingsPrivateGetSpellcheckWordsFunction::OnCustomDictionaryLoaded() {
  SpellcheckService* service =
      SpellcheckServiceFactory::GetForContext(browser_context());
  service->GetCustomDictionary()->RemoveObserver(this);
  Respond(OneArgument(GetSpellcheckWords().release()));
  Release();
}

void
LanguageSettingsPrivateGetSpellcheckWordsFunction::OnCustomDictionaryChanged(
    const SpellcheckCustomDictionary::Change& dictionary_change) {
  NOTREACHED() << "SpellcheckCustomDictionary::Observer: "
                  "OnCustomDictionaryChanged() called before "
                  "OnCustomDictionaryLoaded()";
}

scoped_ptr<base::ListValue>
LanguageSettingsPrivateGetSpellcheckWordsFunction::GetSpellcheckWords() const {
  SpellcheckService* service =
      SpellcheckServiceFactory::GetForContext(browser_context());
  SpellcheckCustomDictionary* dictionary = service->GetCustomDictionary();
  DCHECK(dictionary->IsLoaded());

  // TODO(michaelpg): Sort using app locale.
  scoped_ptr<base::ListValue> word_list(new base::ListValue());
  const std::set<std::string>& words = dictionary->GetWords();
  for (const std::string& word : words)
    word_list->AppendString(word);
  return word_list.Pass();
}

LanguageSettingsPrivateAddSpellcheckWordFunction::
    LanguageSettingsPrivateAddSpellcheckWordFunction() {
}

LanguageSettingsPrivateAddSpellcheckWordFunction::
    ~LanguageSettingsPrivateAddSpellcheckWordFunction() {
}

ExtensionFunction::ResponseAction
LanguageSettingsPrivateAddSpellcheckWordFunction::Run() {
  scoped_ptr<language_settings_private::AddSpellcheckWord::Params> params =
      language_settings_private::AddSpellcheckWord::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  SpellcheckService* service =
      SpellcheckServiceFactory::GetForContext(browser_context());
  bool success = service->GetCustomDictionary()->AddWord(params->word);

  return RespondNow(OneArgument(new base::FundamentalValue(success)));
}

LanguageSettingsPrivateRemoveSpellcheckWordFunction::
    LanguageSettingsPrivateRemoveSpellcheckWordFunction() {
}

LanguageSettingsPrivateRemoveSpellcheckWordFunction::
    ~LanguageSettingsPrivateRemoveSpellcheckWordFunction() {
}

ExtensionFunction::ResponseAction
LanguageSettingsPrivateRemoveSpellcheckWordFunction::Run() {
  scoped_ptr<language_settings_private::RemoveSpellcheckWord::Params> params =
      language_settings_private::RemoveSpellcheckWord::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  SpellcheckService* service =
      SpellcheckServiceFactory::GetForContext(browser_context());
  bool success = service->GetCustomDictionary()->RemoveWord(params->word);

  return RespondNow(OneArgument(new base::FundamentalValue(success)));
}

LanguageSettingsPrivateGetTranslateTargetLanguageFunction::
    LanguageSettingsPrivateGetTranslateTargetLanguageFunction()
    : chrome_details_(this) {
}

LanguageSettingsPrivateGetTranslateTargetLanguageFunction::
    ~LanguageSettingsPrivateGetTranslateTargetLanguageFunction() {
}

ExtensionFunction::ResponseAction
LanguageSettingsPrivateGetTranslateTargetLanguageFunction::Run() {
  return RespondNow(OneArgument(new base::StringValue(
      TranslateService::GetTargetLanguage(
          chrome_details_.GetProfile()->GetPrefs()))));
}

#if defined(OS_CHROMEOS)
// Populates the vector of input methods using information in the list of
// descriptors. Used for languageSettingsPrivate.getInputMethodLists().
void PopulateInputMethodListFromDescriptors(
    const InputMethodDescriptors& descriptors,
    std::vector<linked_ptr<language_settings_private::InputMethod>>*
        input_methods) {
  InputMethodManager* manager = InputMethodManager::Get();
  InputMethodUtil* util = manager->GetInputMethodUtil();
  scoped_refptr<InputMethodManager::State> ime_state =
      manager->GetActiveIMEState();
  if (!ime_state.get())
    return;

  // Get the list of enabled IDs and convert to a set for membership testing.
  const std::vector<std::string>& active_ids_list =
      ime_state->GetActiveInputMethodIds();
  const std::set<std::string> active_ids(
      active_ids_list.begin(), active_ids_list.end());

  for (const auto& descriptor : descriptors) {
    linked_ptr<language_settings_private::InputMethod> input_method(
        new language_settings_private::InputMethod());
    input_method->id = descriptor.id();
    input_method->display_name = util->GetLocalizedDisplayName(descriptor);
    input_method->language_codes = descriptor.language_codes();
    bool enabled = active_ids.find(input_method->id) != active_ids.end();
    if (enabled)
      input_method->enabled.reset(new bool(true));
    if (descriptor.options_page_url().is_valid())
      input_method->has_options_page.reset(new bool(true));
    input_methods->push_back(input_method);
  }
}
#endif

LanguageSettingsPrivateGetInputMethodListsFunction::
    LanguageSettingsPrivateGetInputMethodListsFunction() {
}

LanguageSettingsPrivateGetInputMethodListsFunction::
    ~LanguageSettingsPrivateGetInputMethodListsFunction() {
}

ExtensionFunction::ResponseAction
LanguageSettingsPrivateGetInputMethodListsFunction::Run() {
#if !defined(OS_CHROMEOS)
  EXTENSION_FUNCTION_VALIDATE(false);
  return RespondNow(NoArguments());
#else
  language_settings_private::InputMethodLists input_method_lists;
  InputMethodManager* manager = InputMethodManager::Get();

  chromeos::ComponentExtensionIMEManager* component_extension_manager =
      manager->GetComponentExtensionIMEManager();
  PopulateInputMethodListFromDescriptors(
      component_extension_manager->GetAllIMEAsInputMethodDescriptor(),
      &input_method_lists.component_extension_imes);

  scoped_refptr<InputMethodManager::State> ime_state =
      manager->GetActiveIMEState();
  if (ime_state.get()) {
    InputMethodDescriptors ext_ime_descriptors;
    ime_state->GetInputMethodExtensions(&ext_ime_descriptors);
    PopulateInputMethodListFromDescriptors(
        ext_ime_descriptors,
        &input_method_lists.third_party_extension_imes);
  }

  return RespondNow(OneArgument(input_method_lists.ToValue()));
#endif
}

LanguageSettingsPrivateAddInputMethodFunction::
    LanguageSettingsPrivateAddInputMethodFunction() {
}

LanguageSettingsPrivateAddInputMethodFunction::
    ~LanguageSettingsPrivateAddInputMethodFunction() {
}

ExtensionFunction::ResponseAction
LanguageSettingsPrivateAddInputMethodFunction::Run() {
  return RespondNow(OneArgument(new base::FundamentalValue(true)));
}

LanguageSettingsPrivateRemoveInputMethodFunction::
    LanguageSettingsPrivateRemoveInputMethodFunction() {
}

LanguageSettingsPrivateRemoveInputMethodFunction::
    ~LanguageSettingsPrivateRemoveInputMethodFunction() {
}

ExtensionFunction::ResponseAction
LanguageSettingsPrivateRemoveInputMethodFunction::Run() {
  return RespondNow(OneArgument(new base::FundamentalValue(true)));
}

}  // namespace extensions
