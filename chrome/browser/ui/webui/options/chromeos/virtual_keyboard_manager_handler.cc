// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/virtual_keyboard_manager_handler.h"

#include <map>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/input_method/virtual_keyboard_selector.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace ime = ::chromeos::input_method;

namespace chromeos {

VirtualKeyboardManagerHandler::VirtualKeyboardManagerHandler() {
}

VirtualKeyboardManagerHandler::~VirtualKeyboardManagerHandler() {
}

void VirtualKeyboardManagerHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static const OptionsStringResource resources[] = {
    { "virtualKeyboardLayoutColumnTitle",
      IDS_OPTIONS_SETTINGS_LANGUAGES_VIRTUAL_KEYBOARD_LAYOUT_COLUMN_TITLE },
    { "virtualKeyboardKeyboardColumnTitle",
      IDS_OPTIONS_SETTINGS_LANGUAGES_VIRTUAL_KEYBOARD_KEYBOARD_COLUMN_TITLE },
    { "defaultVirtualKeyboard",
      IDS_OPTIONS_SETTINGS_LANGUAGES_DEFAULT_VIRTUAL_KEYBOARD },
  };
  RegisterStrings(localized_strings, resources, arraysize(resources));

  RegisterTitle(localized_strings, "virtualKeyboardPage",
                IDS_OPTIONS_SETTINGS_LANGUAGES_VIRTUAL_KEYBOARD_SETTINGS_TITLE);

  // Do not call GetVirtualKeyboardList() here since |web_ui_| is not ready yet.
}

void VirtualKeyboardManagerHandler::Initialize() {
}

void VirtualKeyboardManagerHandler::RegisterMessages() {
  DCHECK(web_ui_);
  // Register handler functions for chrome.send().
  web_ui_->RegisterMessageCallback("updateVirtualKeyboardList",
      NewCallback(
          this,
          &VirtualKeyboardManagerHandler::UpdateVirtualKeyboardList));
  web_ui_->RegisterMessageCallback("setVirtualKeyboardPreference",
      NewCallback(
          this,
          &VirtualKeyboardManagerHandler::SetVirtualKeyboardPreference));
  web_ui_->RegisterMessageCallback("clearVirtualKeyboardPreference",
      NewCallback(
          this,
          &VirtualKeyboardManagerHandler::ClearVirtualKeyboardPreference));
}

ListValue* VirtualKeyboardManagerHandler::GetVirtualKeyboardList() {
  DCHECK(web_ui_);
  ime::InputMethodManager* input_method =
      ime::InputMethodManager::GetInstance();

  // Get a multi map from layout name (e.g. "us(dvorak)"), to virtual keyboard
  // extension.
  const LayoutToKeyboard& layout_to_keyboard =
      input_method->GetLayoutNameToKeyboardMapping();
  const UrlToKeyboard& url_to_keyboard =
      input_method->GetUrlToKeyboardMapping();

  // Get the current pref values.
  PrefService* prefs = Profile::FromWebUI(web_ui_)->GetPrefs();
  DCHECK(prefs);
  const DictionaryValue* virtual_keyboard_pref =
      prefs->GetDictionary(prefs::kLanguagePreferredVirtualKeyboard);

  return CreateVirtualKeyboardList(
      layout_to_keyboard, url_to_keyboard, virtual_keyboard_pref);
}

void VirtualKeyboardManagerHandler::UpdateVirtualKeyboardList(
    const ListValue* args) {
  scoped_ptr<Value> virtual_keyboards(GetVirtualKeyboardList());
  DCHECK(virtual_keyboards.get());
  web_ui_->CallJavascriptFunction(
      "VirtualKeyboardManager.updateVirtualKeyboardList", *virtual_keyboards);
}

void VirtualKeyboardManagerHandler::SetVirtualKeyboardPreference(
    const ListValue* args) {
  DCHECK(web_ui_);
  std::string layout, url;
  if (!args || !args->GetString(0, &layout) || !args->GetString(1, &url)) {
    LOG(ERROR) << "SetVirtualKeyboardPreference: Invalid argument";
    return;
  }

  // Validate args.
  ime::InputMethodManager* input_method =
      ime::InputMethodManager::GetInstance();
  if (!ValidateUrl(input_method->GetUrlToKeyboardMapping(), layout, url)) {
    LOG(ERROR) << "SetVirtualKeyboardPreference: Invalid args: "
               << "layout=" << layout << ", url=" << url;
    return;
  }

  PrefService* prefs = Profile::FromWebUI(web_ui_)->GetPrefs();
  DCHECK(prefs);
  {
    DictionaryPrefUpdate updater(
        prefs, prefs::kLanguagePreferredVirtualKeyboard);
    DictionaryValue* pref_value = updater.Get();
    pref_value->SetWithoutPathExpansion(layout, new StringValue(url));
  }
  Preferences::UpdateVirturalKeyboardPreference(prefs);
}

void VirtualKeyboardManagerHandler::ClearVirtualKeyboardPreference(
    const ListValue* args) {
  DCHECK(web_ui_);
  std::string layout;
  if (!args || !args->GetString(0, &layout)) {
    LOG(ERROR) << "ClearVirtualKeyboardPreference: Invalid argument";
    return;
  }

  // Validate |layout|.
  ime::InputMethodManager* input_method =
      ime::InputMethodManager::GetInstance();
  if (!input_method->GetLayoutNameToKeyboardMapping().count(layout)) {
    LOG(ERROR) << "ClearVirtualKeyboardPreference: Invalid layout: " << layout;
    return;
  }

  PrefService* prefs = Profile::FromWebUI(web_ui_)->GetPrefs();
  DCHECK(prefs);
  {
    DictionaryPrefUpdate updater(
        prefs, prefs::kLanguagePreferredVirtualKeyboard);
    DictionaryValue* pref_value = updater.Get();
    pref_value->RemoveWithoutPathExpansion(layout, NULL);
  }
  Preferences::UpdateVirturalKeyboardPreference(prefs);
}

// static
bool VirtualKeyboardManagerHandler::ValidateUrl(
    const UrlToKeyboard& url_to_keyboard,
    const std::string& layout,
    const std::string& url) {
  UrlToKeyboard::const_iterator iter = url_to_keyboard.find(GURL(url));
  if (iter == url_to_keyboard.end() ||
      !iter->second->supported_layouts().count(layout)) {
    return false;
  }
  return true;
}

// static
ListValue* VirtualKeyboardManagerHandler::CreateVirtualKeyboardList(
    const LayoutToKeyboard& layout_to_keyboard,
    const UrlToKeyboard& url_to_keyboard,
    const DictionaryValue* virtual_keyboard_pref) {
  ListValue* layout_list = new ListValue;

  // |dictionary| points to an element in the |layout_list|. One dictionary
  // element is created for one layout.
  DictionaryValue* dictionary = NULL;

  LayoutToKeyboard::const_iterator i;
  for (i = layout_to_keyboard.begin(); i != layout_to_keyboard.end(); ++i) {
    const std::string& layout_id = i->first;

    std::string string_value;
    // Check the "layout" value in the current dictionary.
    if (dictionary) {
      dictionary->GetString("layout", &string_value);
    }

    if (string_value != layout_id) {
      // New layout is found. Add the layout to |layout_list|.
      dictionary = new DictionaryValue;
      layout_list->Append(dictionary);

      // Set layout id as well as its human readable form.
      const ime::InputMethodDescriptor* desc =
          ime::GetInputMethodDescriptorFromXkbId(layout_id);
      const std::string layout_name = desc ?
          ime::GetInputMethodDisplayNameFromId(desc->id()) : layout_id;
      dictionary->SetString("layout", layout_id);
      dictionary->SetString("layoutName", layout_name);

      // Check if the layout is in user pref.
      if (virtual_keyboard_pref &&
          virtual_keyboard_pref->GetString(layout_id, &string_value) &&
          ValidateUrl(url_to_keyboard, layout_id, string_value)) {
        dictionary->SetString("preferredKeyboard", string_value);
      }
      dictionary->Set("supportedKeyboards", new ListValue);
    }

    ListValue* supported_keyboards = NULL;
    dictionary->GetList("supportedKeyboards", &supported_keyboards);
    DCHECK(supported_keyboards);

    DictionaryValue* virtual_keyboard = new DictionaryValue;
    virtual_keyboard->SetString("name", i->second->name());
    virtual_keyboard->SetBoolean("isSystem", i->second->is_system());
    virtual_keyboard->SetString("url", i->second->url().spec());
    supported_keyboards->Append(virtual_keyboard);
  }

  return layout_list;
}

}  // namespace chromeos
