// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/input_method_api.h"

#include "base/lazy_instance.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/input_method_event_router.h"
#include "chrome/browser/extensions/api/input_ime/input_ime_api.h"
#include "chromeos/ime/extension_ime_util.h"
#include "chromeos/ime/input_method_manager.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_system.h"

namespace {

// Prefix, which is used by XKB.
const char kXkbPrefix[] = "xkb:";

}  // namespace

namespace extensions {

GetInputMethodFunction::GetInputMethodFunction() {
}

GetInputMethodFunction::~GetInputMethodFunction() {
}

bool GetInputMethodFunction::RunImpl() {
#if !defined(OS_CHROMEOS)
  NOTREACHED();
  return false;
#else
  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::Get();
  const std::string input_method = InputMethodAPI::GetInputMethodForXkb(
      manager->GetCurrentInputMethod().id());
  SetResult(base::Value::CreateStringValue(input_method));
  return true;
#endif
}

StartImeFunction::StartImeFunction() {
}

StartImeFunction::~StartImeFunction() {
}

bool StartImeFunction::RunImpl() {
#if !defined(OS_CHROMEOS)
  NOTREACHED();
  return false;
#else
  chromeos::InputMethodEngineInterface* engine =
      InputImeEventRouter::GetInstance()->GetActiveEngine(extension_id());
  if (engine)
    engine->NotifyImeReady();
  return true;
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
  registry->RegisterFunction<GetInputMethodFunction>();
  registry->RegisterFunction<StartImeFunction>();
}

InputMethodAPI::~InputMethodAPI() {
}

// static
std::string InputMethodAPI::GetInputMethodForXkb(const std::string& xkb_id) {
  std::string xkb_prefix =
      chromeos::extension_ime_util::GetInputMethodIDByKeyboardLayout(
          kXkbPrefix);
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
