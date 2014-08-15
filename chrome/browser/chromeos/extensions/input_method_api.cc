// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/input_method_api.h"

#include "base/lazy_instance.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/input_method_event_router.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/extensions/api/input_ime/input_ime_api.h"
#include "chromeos/ime/extension_ime_util.h"
#include "chromeos/ime/input_method_descriptor.h"
#include "chromeos/ime/input_method_manager.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/value_builder.h"

namespace {

// Prefix, which is used by XKB.
const char kXkbPrefix[] = "xkb:";

}  // namespace

namespace extensions {

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

// static
const char InputMethodAPI::kOnInputMethodChanged[] =
    "inputMethodPrivate.onChanged";

InputMethodAPI::InputMethodAPI(content::BrowserContext* context)
    : context_(context) {
  EventRouter::Get(context_)->RegisterObserver(this, kOnInputMethodChanged);
  ExtensionFunctionRegistry* registry =
      ExtensionFunctionRegistry::GetInstance();
  registry->RegisterFunction<GetCurrentInputMethodFunction>();
  registry->RegisterFunction<SetCurrentInputMethodFunction>();
  registry->RegisterFunction<GetInputMethodsFunction>();
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
  // UnregisterObserver may have already been called in OnListenerAdded,
  // but it is safe to call it more than once.
  EventRouter::Get(context_)->UnregisterObserver(this);
}

void InputMethodAPI::OnListenerAdded(
    const extensions::EventListenerInfo& details) {
  DCHECK(!input_method_event_router_.get());
  input_method_event_router_.reset(
      new chromeos::ExtensionInputMethodEventRouter(context_));
  EventRouter::Get(context_)->UnregisterObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<InputMethodAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<InputMethodAPI>*
InputMethodAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

}  // namespace extensions
