// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/input_method_api.h"

#include <set>
#include <string>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/dictionary_event_router.h"
#include "chrome/browser/chromeos/extensions/input_method_event_router.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/extensions/api/input_ime/input_ime_api.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chromeos/chromeos_switches.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_system.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/input_method_descriptor.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/keyboard/keyboard_util.h"

namespace {

// Prefix, which is used by XKB.
const char kXkbPrefix[] = "xkb:";

}  // namespace

namespace extensions {

ExtensionFunction::ResponseAction GetInputMethodConfigFunction::Run() {
#if !defined(OS_CHROMEOS)
  EXTENSION_FUNCTION_VALIDATE(false);
#else
  base::DictionaryValue* output = new base::DictionaryValue();
  output->SetBoolean(
      "isPhysicalKeyboardAutocorrectEnabled",
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDisablePhysicalKeyboardAutocorrect));
  return RespondNow(OneArgument(output));
#endif
}

ExtensionFunction::ResponseAction GetCurrentInputMethodFunction::Run() {
#if !defined(OS_CHROMEOS)
  EXTENSION_FUNCTION_VALIDATE(false);
#else
  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::Get();
  return RespondNow(OneArgument(new base::StringValue(
      manager->GetActiveIMEState()->GetCurrentInputMethod().id())));
#endif
}

ExtensionFunction::ResponseAction SetCurrentInputMethodFunction::Run() {
#if !defined(OS_CHROMEOS)
  EXTENSION_FUNCTION_VALIDATE(false);
#else
  std::string new_input_method;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &new_input_method));
  scoped_refptr<chromeos::input_method::InputMethodManager::State> ime_state =
      chromeos::input_method::InputMethodManager::Get()->GetActiveIMEState();
  const std::vector<std::string>& input_methods =
      ime_state->GetActiveInputMethodIds();
  for (size_t i = 0; i < input_methods.size(); ++i) {
    const std::string& input_method = input_methods[i];
    if (input_method == new_input_method) {
      ime_state->ChangeInputMethod(new_input_method, false /* show_message */);
      return RespondNow(NoArguments());
    }
  }
  return RespondNow(Error("Invalid input method id."));
#endif
}

ExtensionFunction::ResponseAction GetInputMethodsFunction::Run() {
#if !defined(OS_CHROMEOS)
  EXTENSION_FUNCTION_VALIDATE(false);
#else
  base::ListValue* output = new base::ListValue();
  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::Get();
  chromeos::input_method::InputMethodUtil* util = manager->GetInputMethodUtil();
  scoped_refptr<chromeos::input_method::InputMethodManager::State> ime_state =
      manager->GetActiveIMEState();
  scoped_ptr<chromeos::input_method::InputMethodDescriptors> input_methods =
      ime_state->GetActiveInputMethods();
  for (size_t i = 0; i < input_methods->size(); ++i) {
    const chromeos::input_method::InputMethodDescriptor& input_method =
        (*input_methods)[i];
    base::DictionaryValue* val = new base::DictionaryValue();
    val->SetString("id", input_method.id());
    val->SetString("name", util->GetInputMethodLongName(input_method));
    val->SetString("indicator", util->GetInputMethodShortName(input_method));
    output->Append(val);
  }
  return RespondNow(OneArgument(output));
#endif
}

ExtensionFunction::ResponseAction FetchAllDictionaryWordsFunction::Run() {
#if !defined(OS_CHROMEOS)
  EXTENSION_FUNCTION_VALIDATE(false);
#else
  SpellcheckService* spellcheck = SpellcheckServiceFactory::GetForContext(
      context_);
  if (!spellcheck) {
    return RespondNow(Error("Spellcheck service not available."));
  }
  SpellcheckCustomDictionary* dictionary = spellcheck->GetCustomDictionary();
  if (!dictionary->IsLoaded()) {
    return RespondNow(Error("Custom dictionary not loaded yet."));
  }

  const std::set<std::string>& words = dictionary->GetWords();
  base::ListValue* output = new base::ListValue();
  for (auto it = words.begin(); it != words.end(); ++it) {
    output->AppendString(*it);
  }
  return RespondNow(OneArgument(output));
#endif
}

ExtensionFunction::ResponseAction AddWordToDictionaryFunction::Run() {
#if !defined(OS_CHROMEOS)
  EXTENSION_FUNCTION_VALIDATE(false);
#else
  std::string word;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &word));
  SpellcheckService* spellcheck = SpellcheckServiceFactory::GetForContext(
      context_);
  if (!spellcheck) {
    return RespondNow(Error("Spellcheck service not available."));
  }
  SpellcheckCustomDictionary* dictionary = spellcheck->GetCustomDictionary();
  if (!dictionary->IsLoaded()) {
    return RespondNow(Error("Custom dictionary not loaded yet."));
  }

  if (dictionary->AddWord(word))
    return RespondNow(NoArguments());
  // Invalid words:
  // - Already in the dictionary.
  // - Not a UTF8 string.
  // - Longer than 99 bytes (MAX_CUSTOM_DICTIONARY_WORD_BYTES).
  // - Leading/trailing whitespace.
  // - Empty.
  return RespondNow(Error("Unable to add invalid word to dictionary."));
#endif
}

ExtensionFunction::ResponseAction GetEncryptSyncEnabledFunction::Run() {
#if !defined(OS_CHROMEOS)
  EXTENSION_FUNCTION_VALIDATE(false);
#else
  ProfileSyncService* profile_sync_service =
      ProfileSyncServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  if (!profile_sync_service)
    return RespondNow(Error("Sync service is not ready for current profile."));
  scoped_ptr<base::Value> ret(new base::FundamentalValue(
      profile_sync_service->IsEncryptEverythingEnabled()));
  return RespondNow(OneArgument(ret.Pass()));
#endif
}

// static
const char InputMethodAPI::kOnDictionaryChanged[] =
    "inputMethodPrivate.onDictionaryChanged";

// static
const char InputMethodAPI::kOnDictionaryLoaded[] =
    "inputMethodPrivate.onDictionaryLoaded";

// static
const char InputMethodAPI::kOnInputMethodChanged[] =
    "inputMethodPrivate.onChanged";

InputMethodAPI::InputMethodAPI(content::BrowserContext* context)
    : context_(context) {
  EventRouter::Get(context_)->RegisterObserver(this, kOnInputMethodChanged);
  EventRouter::Get(context_)->RegisterObserver(this, kOnDictionaryChanged);
  EventRouter::Get(context_)->RegisterObserver(this, kOnDictionaryLoaded);
  ExtensionFunctionRegistry* registry =
      ExtensionFunctionRegistry::GetInstance();
  registry->RegisterFunction<GetInputMethodConfigFunction>();
  registry->RegisterFunction<GetCurrentInputMethodFunction>();
  registry->RegisterFunction<SetCurrentInputMethodFunction>();
  registry->RegisterFunction<GetInputMethodsFunction>();
  registry->RegisterFunction<FetchAllDictionaryWordsFunction>();
  registry->RegisterFunction<AddWordToDictionaryFunction>();
  registry->RegisterFunction<GetEncryptSyncEnabledFunction>();
}

InputMethodAPI::~InputMethodAPI() {
}

// static
std::string InputMethodAPI::GetInputMethodForXkb(const std::string& xkb_id) {
  std::string xkb_prefix =
      chromeos::extension_ime_util::GetInputMethodIDByEngineID(kXkbPrefix);
  size_t prefix_length = xkb_prefix.length();
  DCHECK(xkb_id.substr(0, prefix_length) == xkb_prefix);
  return xkb_id.substr(prefix_length);
}

void InputMethodAPI::Shutdown() {
  EventRouter::Get(context_)->UnregisterObserver(this);
}

void InputMethodAPI::OnListenerAdded(
    const extensions::EventListenerInfo& details) {
  if (details.event_name == kOnInputMethodChanged) {
    if (!input_method_event_router_.get()) {
      input_method_event_router_.reset(
          new chromeos::ExtensionInputMethodEventRouter(context_));
    }
  } else if (details.event_name == kOnDictionaryChanged ||
             details.event_name == kOnDictionaryLoaded) {
    if (!dictionary_event_router_.get()) {
      dictionary_event_router_.reset(
          new chromeos::ExtensionDictionaryEventRouter(context_));
    }
    if (details.event_name == kOnDictionaryLoaded) {
      dictionary_event_router_->DispatchLoadedEventIfLoaded();
    }
  }
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<InputMethodAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<InputMethodAPI>*
InputMethodAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

}  // namespace extensions
