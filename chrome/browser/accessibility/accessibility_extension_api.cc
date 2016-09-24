// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/accessibility_extension_api.h"

#include <stddef.h>

#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/accessibility_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/lazy_background_task_queue.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_handlers/background_info.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/ui/accessibility_focus_ring_controller.h"
using chromeos::AccessibilityFocusRingController;
#endif

namespace accessibility_private = extensions::api::accessibility_private;

namespace {
#if defined(OS_CHROMEOS)
// ScreenRect fields.
const char kLeft[] = "left";
const char kTop[] = "top";
const char kWidth[] = "width";
const char kHeight[] = "height";
#endif  // defined(OS_CHROMEOS)

const char kErrorNotSupported[] = "This API is not supported on this platform.";
}  // namespace

ExtensionFunction::ResponseAction
AccessibilityPrivateSetNativeAccessibilityEnabledFunction::Run() {
  bool enabled = false;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(0, &enabled));
  if (enabled) {
    content::BrowserAccessibilityState::GetInstance()->
        EnableAccessibility();
  } else {
    content::BrowserAccessibilityState::GetInstance()->
        DisableAccessibility();
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSetFocusRingFunction::Run() {
#if defined(OS_CHROMEOS)
  base::ListValue* rect_values = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetList(0, &rect_values));

  std::vector<gfx::Rect> rects;
  for (size_t i = 0; i < rect_values->GetSize(); ++i) {
    base::DictionaryValue* rect_value = NULL;
    EXTENSION_FUNCTION_VALIDATE(rect_values->GetDictionary(i, &rect_value));
    int left, top, width, height;
    EXTENSION_FUNCTION_VALIDATE(rect_value->GetInteger(kLeft, &left));
    EXTENSION_FUNCTION_VALIDATE(rect_value->GetInteger(kTop, &top));
    EXTENSION_FUNCTION_VALIDATE(rect_value->GetInteger(kWidth, &width));
    EXTENSION_FUNCTION_VALIDATE(rect_value->GetInteger(kHeight, &height));
    rects.push_back(gfx::Rect(left, top, width, height));
  }

  // Move the visible focus ring to cover all of these rects.
  AccessibilityFocusRingController::GetInstance()->SetFocusRing(
      rects, AccessibilityFocusRingController::PERSIST_FOCUS_RING);

  // Also update the touch exploration controller so that synthesized
  // touch events are anchored within the focused object.
  if (!rects.empty()) {
    chromeos::AccessibilityManager* manager =
        chromeos::AccessibilityManager::Get();
    manager->SetTouchAccessibilityAnchorPoint(rects[0].CenterPoint());
  }

  return RespondNow(NoArguments());
#endif  // defined(OS_CHROMEOS)

  return RespondNow(Error(kErrorNotSupported));
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSetKeyboardListenerFunction::Run() {
  ChromeExtensionFunctionDetails details(this);
  CHECK(extension());

#if defined(OS_CHROMEOS)
  bool enabled;
  bool capture;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(0, &enabled));
  EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(1, &capture));

  chromeos::AccessibilityManager* manager =
      chromeos::AccessibilityManager::Get();

  const std::string current_id = manager->keyboard_listener_extension_id();
  if (!current_id.empty() && extension()->id() != current_id)
    return RespondNow(Error("Existing keyboard listener registered."));

  if (enabled) {
    manager->SetKeyboardListenerExtensionId(extension()->id(),
                                            details.GetProfile());
    manager->set_keyboard_listener_capture(capture);
  } else {
    manager->SetKeyboardListenerExtensionId(std::string(),
                                            details.GetProfile());
    manager->set_keyboard_listener_capture(false);
  }
  return RespondNow(NoArguments());
#endif  // defined OS_CHROMEOS

  return RespondNow(Error(kErrorNotSupported));
}
