// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/virtual_keyboard/virtual_keyboard_api.h"

#include <memory>

#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_private_api.h"
#include "extensions/common/api/virtual_keyboard.h"

#if defined(OS_CHROMEOS)
#include "ui/base/ime/chromeos/input_method_manager.h"
using chromeos::input_method::InputMethodManager;
#endif

namespace extensions {

VirtualKeyboardRestrictFeaturesFunction::
    VirtualKeyboardRestrictFeaturesFunction() {}

ExtensionFunction::ResponseAction
VirtualKeyboardRestrictFeaturesFunction::Run() {
  std::unique_ptr<api::virtual_keyboard::RestrictFeatures::Params> params(
      api::virtual_keyboard::RestrictFeatures::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  bool features_enabled = params->restrictions.auto_correct_enabled;
  if (params->restrictions.auto_complete_enabled != features_enabled ||
      params->restrictions.spell_check_enabled != features_enabled ||
      params->restrictions.voice_input_enabled != features_enabled ||
      params->restrictions.handwriting_enabled != features_enabled) {
    return RespondNow(
        Error("All features are expected to have the same enabled state."));
  }
  VirtualKeyboardAPI* api =
      BrowserContextKeyedAPIFactory<VirtualKeyboardAPI>::Get(browser_context());
  api->delegate()->SetKeyboardRestricted(!features_enabled);

#if defined(OS_CHROMEOS)
  InputMethodManager* input_method_manager = InputMethodManager::Get();
  if (input_method_manager) {
    input_method_manager->SetImeMenuFeatureEnabled(
        InputMethodManager::FEATURE_VOICE,
        params->restrictions.voice_input_enabled);
    input_method_manager->SetImeMenuFeatureEnabled(
        InputMethodManager::FEATURE_HANDWRITING,
        params->restrictions.handwriting_enabled);
  }
#endif
  return RespondNow(NoArguments());
}

}  // namespace extensions
