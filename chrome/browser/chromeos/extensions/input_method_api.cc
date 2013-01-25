// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/input_method_api.h"

#include "base/lazy_instance.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/input_method_event_router.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/extension_function_registry.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"

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
      chromeos::input_method::GetInputMethodManager();
  const std::string input_method = InputMethodAPI::GetInputMethodForXkb(
      manager->GetCurrentInputMethod().id());
  SetResult(Value::CreateStringValue(input_method));
  return true;
#endif
}

InputMethodAPI::InputMethodAPI(Profile* profile)
    : profile_(profile) {
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, event_names::kOnInputMethodChanged);
  ExtensionFunctionRegistry* registry =
      ExtensionFunctionRegistry::GetInstance();
  registry->RegisterFunction<GetInputMethodFunction>();
}

InputMethodAPI::~InputMethodAPI() {
}

// static
std::string InputMethodAPI::GetInputMethodForXkb(const std::string& xkb_id) {
  size_t prefix_length = std::string(kXkbPrefix).length();
  DCHECK(xkb_id.substr(0, prefix_length) == kXkbPrefix);
  return xkb_id.substr(prefix_length);
}

void InputMethodAPI::Shutdown() {
  // UnregisterObserver may have already been called in OnListenerAdded,
  // but it is safe to call it more than once.
  ExtensionSystem::Get(profile_)->event_router()->UnregisterObserver(this);
}

void InputMethodAPI::OnListenerAdded(
    const extensions::EventListenerInfo& details) {
  DCHECK(!input_method_event_router_.get());
  input_method_event_router_.reset(
      new chromeos::ExtensionInputMethodEventRouter());
  ExtensionSystem::Get(profile_)->event_router()->UnregisterObserver(this);
}

static base::LazyInstance<ProfileKeyedAPIFactory<InputMethodAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<InputMethodAPI>* InputMethodAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

}  // namespace extensions
