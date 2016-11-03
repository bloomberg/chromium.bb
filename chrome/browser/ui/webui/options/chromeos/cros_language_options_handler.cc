// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/cros_language_options_handler.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/chromeos/login/l10n_util.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_url_handlers.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"

using base::UserMetricsAction;

namespace chromeos {
namespace options {

CrosLanguageOptionsHandler::CrosLanguageOptionsHandler() {
}

CrosLanguageOptionsHandler::~CrosLanguageOptionsHandler() {
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
  localized_strings->SetString("activateImeMenu",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_ACTIVATE_IME_MENU));

  // GetSupportedInputMethods() never returns NULL.
  localized_strings->Set("languageList", GetAcceptLanguageList().release());
  localized_strings->Set("inputMethodList", GetInputMethodList());

  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  input_method::InputMethodDescriptors ext_ime_descriptors;
  manager->GetActiveIMEState()->GetInputMethodExtensions(&ext_ime_descriptors);

  base::ListValue* ext_ime_list = ConvertInputMethodDescriptorsToIMEList(
      ext_ime_descriptors);
  AddImeProvider(ext_ime_list);
  localized_strings->Set("extensionImeList", ext_ime_list);

  ComponentExtensionIMEManager* component_extension_manager =
      input_method::InputMethodManager::Get()
          ->GetComponentExtensionIMEManager();
  localized_strings->Set(
      "componentExtensionImeList",
      ConvertInputMethodDescriptorsToIMEList(
          component_extension_manager->GetAllIMEAsInputMethodDescriptor()));
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
base::ListValue* CrosLanguageOptionsHandler::GetInputMethodList() {
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  // GetSupportedInputMethods() never return NULL.
  std::unique_ptr<input_method::InputMethodDescriptors> descriptors(
      manager->GetSupportedInputMethods());

  base::ListValue* input_method_list = new base::ListValue();

  for (size_t i = 0; i < descriptors->size(); ++i) {
    const input_method::InputMethodDescriptor& descriptor =
        (*descriptors)[i];
    const std::string display_name =
        manager->GetInputMethodUtil()->GetInputMethodDisplayNameFromId(
            descriptor.id());
    auto dictionary = base::MakeUnique<base::DictionaryValue>();
    dictionary->SetString("id", descriptor.id());
    dictionary->SetString("displayName", display_name);

    // One input method can be associated with multiple languages, hence
    // we use a dictionary here.
    base::DictionaryValue* languages = new base::DictionaryValue();
    for (size_t i = 0; i < descriptor.language_codes().size(); ++i) {
      languages->SetBoolean(descriptor.language_codes().at(i), true);
    }
    dictionary->Set("languageCodeSet", languages);

    input_method_list->Append(std::move(dictionary));
  }

  return input_method_list;
}

base::ListValue*
    CrosLanguageOptionsHandler::ConvertInputMethodDescriptorsToIMEList(
        const input_method::InputMethodDescriptors& descriptors) {
  input_method::InputMethodUtil* util =
      input_method::InputMethodManager::Get()->GetInputMethodUtil();
  std::unique_ptr<base::ListValue> ime_ids_list(new base::ListValue());
  for (size_t i = 0; i < descriptors.size(); ++i) {
    const input_method::InputMethodDescriptor& descriptor = descriptors[i];
    std::unique_ptr<base::DictionaryValue> dictionary(
        new base::DictionaryValue());
    dictionary->SetString("id", descriptor.id());
    dictionary->SetString(
        "displayName", util->GetLocalizedDisplayName(descriptor));
    dictionary->SetString("optionsPage", descriptor.options_page_url().spec());
    std::unique_ptr<base::DictionaryValue> language_codes(
        new base::DictionaryValue());
    for (size_t i = 0; i < descriptor.language_codes().size(); ++i)
      language_codes->SetBoolean(descriptor.language_codes().at(i), true);
    dictionary->Set("languageCodeSet", language_codes.release());
    ime_ids_list->Append(std::move(dictionary));
  }
  return ime_ids_list.release();
}

void CrosLanguageOptionsHandler::SetApplicationLocale(
    const std::string& language_code) {
  Profile* profile = Profile::FromWebUI(web_ui());
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  // Secondary users and public session users cannot change the locale.
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  if (user &&
      user->GetAccountId() == user_manager->GetPrimaryUser()->GetAccountId() &&
      user->GetType() != user_manager::USER_TYPE_PUBLIC_ACCOUNT) {
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
      input_method::InputMethodManager::Get()
          ->GetActiveIMEState()
          ->GetInputMethodFromId(input_method_id);
  if (!ime)
    return;

  Browser* browser = chrome::FindBrowserWithWebContents(
      web_ui()->GetWebContents());
  content::OpenURLParams params(ime->options_page_url(), content::Referrer(),
                                WindowOpenDisposition::SINGLETON_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  browser->OpenURL(params);
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
