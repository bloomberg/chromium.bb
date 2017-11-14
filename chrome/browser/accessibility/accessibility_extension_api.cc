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
#include "extensions/common/image_util.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "ui/events/keycodes/keyboard_codes.h"

#if defined(OS_CHROMEOS)
#include "ash/accessibility/accessibility_focus_ring_controller.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/arc/accessibility/arc_accessibility_helper_bridge.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"

using ash::AccessibilityFocusRingController;
#endif

namespace accessibility_private = extensions::api::accessibility_private;

namespace {

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

  std::unique_ptr<extensions::api::accessibility_private::SetFocusRing::Params>
      params(
          extensions::api::accessibility_private::SetFocusRing::Params::Create(
              *args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<gfx::Rect> rects;
  for (const extensions::api::accessibility_private::ScreenRect& rect :
       params->rects) {
    rects.push_back(gfx::Rect(rect.left, rect.top, rect.width, rect.height));
  }

  if (params->color) {
    SkColor color;
    if (!extensions::image_util::ParseHexColorString(*(params->color), &color))
      return RespondNow(Error("Could not parse hex color"));
    AccessibilityFocusRingController::GetInstance()->SetFocusRingColor(color);
  } else {
    AccessibilityFocusRingController::GetInstance()->ResetFocusRingColor();
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
AccessibilityPrivateSetHighlightsFunction::Run() {
#if defined(OS_CHROMEOS)
  std::unique_ptr<extensions::api::accessibility_private::SetHighlights::Params>
      params(
          extensions::api::accessibility_private::SetHighlights::Params::Create(
              *args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<gfx::Rect> rects;
  for (const extensions::api::accessibility_private::ScreenRect& rect :
       params->rects) {
    rects.push_back(gfx::Rect(rect.left, rect.top, rect.width, rect.height));
  }

  SkColor color;
  if (!extensions::image_util::ParseHexColorString(params->color, &color))
    return RespondNow(Error("Could not parse hex color"));

  // Set the highlights to cover all of these rects.
  AccessibilityFocusRingController::GetInstance()->SetHighlights(rects, color);

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

ExtensionFunction::ResponseAction
AccessibilityPrivateDarkenScreenFunction::Run() {
  ChromeExtensionFunctionDetails details(this);
  CHECK(extension());

#if defined(OS_CHROMEOS)
  bool darken;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(0, &darken));
  chromeos::PowerManagerClient* client =
      chromeos::DBusThreadManager::Get()->GetPowerManagerClient();

  // Called twice to ensure the cros end of the dbus message is in a good
  // state.
  client->SetBacklightsForcedOff(!darken);
  client->SetBacklightsForcedOff(darken);
  return RespondNow(NoArguments());
#endif  // defined OS_CHROMEOS

  return RespondNow(Error(kErrorNotSupported));
}

#if defined(OS_CHROMEOS)
ExtensionFunction::ResponseAction
AccessibilityPrivateSetSwitchAccessKeysFunction::Run() {
  std::unique_ptr<accessibility_private::SetSwitchAccessKeys::Params> params =
      accessibility_private::SetSwitchAccessKeys::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  // For now, only accept key code if it represents an alphanumeric character.
  std::set<int> key_codes;
  for (auto key_code : params->key_codes) {
    EXTENSION_FUNCTION_VALIDATE(key_code >= ui::VKEY_0 &&
                                key_code <= ui::VKEY_Z);
    key_codes.insert(key_code);
  }

  chromeos::AccessibilityManager* manager =
      chromeos::AccessibilityManager::Get();

  // AccessibilityManager can be null during system shut down, but no need to
  // return error in this case, so just check that manager is not null.
  if (manager)
    manager->SetSwitchAccessKeys(key_codes);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSetNativeChromeVoxArcSupportForCurrentAppFunction::Run() {
  std::unique_ptr<
      accessibility_private::SetNativeChromeVoxArcSupportForCurrentApp::Params>
      params = accessibility_private::
          SetNativeChromeVoxArcSupportForCurrentApp::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  ChromeExtensionFunctionDetails details(this);
  arc::ArcAccessibilityHelperBridge* bridge =
      arc::ArcAccessibilityHelperBridge::GetForBrowserContext(
          details.GetProfile());
  if (bridge) {
    bool enabled;
    EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(0, &enabled));
    bridge->SetNativeChromeVoxArcSupport(enabled);
  }
  return RespondNow(NoArguments());
}

#endif  // defined (OS_CHROMEOS)
