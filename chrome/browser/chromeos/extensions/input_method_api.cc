// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/input_method_api.h"

#include "base/values.h"
#include "chrome/browser/chromeos/extensions/input_method_event_router.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"

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
  chromeos::ExtensionInputMethodEventRouter* router =
      extensions::ExtensionSystem::Get(profile_)->extension_service()->
          input_method_event_router();
  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::GetInputMethodManager();
  const std::string input_method =
      router->GetInputMethodForXkb(manager->GetCurrentInputMethod().id());
  SetResult(Value::CreateStringValue(input_method));
  return true;
#endif
}

}  // namespace extensions
