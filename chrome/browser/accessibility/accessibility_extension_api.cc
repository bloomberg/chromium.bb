// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/accessibility_extension_api.h"

#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/accessibility/accessibility_extension_api_constants.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
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
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/lazy_background_task_queue.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_handlers/background_info.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/ui/accessibility_focus_ring_controller.h"
#endif

namespace keys = extension_accessibility_api_constants;
namespace accessibility_private = extensions::api::accessibility_private;

// Returns the AccessibilityControlInfo serialized into a JSON string,
// consisting of an array of a single object of type AccessibilityObject,
// as defined in the accessibility extension api's json schema.
scoped_ptr<base::ListValue> ControlInfoToEventArguments(
    const AccessibilityEventInfo* info) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  info->SerializeToDict(dict);

  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(dict);
  return args.Pass();
}

ExtensionAccessibilityEventRouter*
    ExtensionAccessibilityEventRouter::GetInstance() {
  return Singleton<ExtensionAccessibilityEventRouter>::get();
}

ExtensionAccessibilityEventRouter::ExtensionAccessibilityEventRouter()
    : enabled_(false) {
}

ExtensionAccessibilityEventRouter::~ExtensionAccessibilityEventRouter() {
  control_event_callback_.Reset();
}

void ExtensionAccessibilityEventRouter::SetAccessibilityEnabled(bool enabled) {
  enabled_ = enabled;
}

bool ExtensionAccessibilityEventRouter::IsAccessibilityEnabled() const {
  return enabled_;
}

void ExtensionAccessibilityEventRouter::SetControlEventCallbackForTesting(
    ControlEventCallback control_event_callback) {
  DCHECK(control_event_callback_.is_null());
  control_event_callback_ = control_event_callback;
}

void ExtensionAccessibilityEventRouter::ClearControlEventCallback() {
  control_event_callback_.Reset();
}

void ExtensionAccessibilityEventRouter::HandleWindowEvent(
    ui::AXEvent event,
    const AccessibilityWindowInfo* info) {
  if (!control_event_callback_.is_null())
    control_event_callback_.Run(event, info);

  if (event == ui::AX_EVENT_ALERT)
    OnWindowOpened(info);
}

void ExtensionAccessibilityEventRouter::HandleMenuEvent(
    ui::AXEvent event,
    const AccessibilityMenuInfo* info) {
  switch (event) {
    case ui::AX_EVENT_MENU_START:
    case ui::AX_EVENT_MENU_POPUP_START:
      OnMenuOpened(info);
      break;
    case ui::AX_EVENT_MENU_END:
    case ui::AX_EVENT_MENU_POPUP_END:
      OnMenuClosed(info);
      break;
    case ui::AX_EVENT_FOCUS:
      OnControlFocused(info);
      break;
    case ui::AX_EVENT_HOVER:
      OnControlHover(info);
      break;
    default:
      NOTREACHED();
  }
}

void ExtensionAccessibilityEventRouter::HandleControlEvent(
    ui::AXEvent event,
    const AccessibilityControlInfo* info) {
  if (!control_event_callback_.is_null())
    control_event_callback_.Run(event, info);

  switch (event) {
    case ui::AX_EVENT_TEXT_CHANGED:
    case ui::AX_EVENT_TEXT_SELECTION_CHANGED:
      OnTextChanged(info);
      break;
    case ui::AX_EVENT_VALUE_CHANGED:
    case ui::AX_EVENT_ALERT:
      OnControlAction(info);
      break;
    case ui::AX_EVENT_FOCUS:
      OnControlFocused(info);
      break;
    case ui::AX_EVENT_HOVER:
      OnControlHover(info);
      break;
    default:
      NOTREACHED();
  }
}

void ExtensionAccessibilityEventRouter::OnWindowOpened(
    const AccessibilityWindowInfo* info) {
  scoped_ptr<base::ListValue> args(ControlInfoToEventArguments(info));
  DispatchEvent(info->profile(),
                accessibility_private::OnWindowOpened::kEventName,
                args.Pass());
}

void ExtensionAccessibilityEventRouter::OnControlFocused(
    const AccessibilityControlInfo* info) {
  last_focused_control_dict_.Clear();
  info->SerializeToDict(&last_focused_control_dict_);
  scoped_ptr<base::ListValue> args(ControlInfoToEventArguments(info));
  DispatchEvent(info->profile(),
                accessibility_private::OnControlFocused::kEventName,
                args.Pass());
}

void ExtensionAccessibilityEventRouter::OnControlAction(
    const AccessibilityControlInfo* info) {
  scoped_ptr<base::ListValue> args(ControlInfoToEventArguments(info));
  DispatchEvent(info->profile(),
                accessibility_private::OnControlAction::kEventName,
                args.Pass());
}

void ExtensionAccessibilityEventRouter::OnControlHover(
    const AccessibilityControlInfo* info) {
  scoped_ptr<base::ListValue> args(ControlInfoToEventArguments(info));
  DispatchEvent(info->profile(),
                accessibility_private::OnControlHover::kEventName,
                args.Pass());
}

void ExtensionAccessibilityEventRouter::OnTextChanged(
    const AccessibilityControlInfo* info) {
  scoped_ptr<base::ListValue> args(ControlInfoToEventArguments(info));
  DispatchEvent(info->profile(),
                accessibility_private::OnTextChanged::kEventName,
                args.Pass());
}

void ExtensionAccessibilityEventRouter::OnMenuOpened(
    const AccessibilityMenuInfo* info) {
  scoped_ptr<base::ListValue> args(ControlInfoToEventArguments(info));
  DispatchEvent(info->profile(),
                accessibility_private::OnMenuOpened::kEventName,
                args.Pass());
}

void ExtensionAccessibilityEventRouter::OnMenuClosed(
    const AccessibilityMenuInfo* info) {
  scoped_ptr<base::ListValue> args(ControlInfoToEventArguments(info));
  DispatchEvent(info->profile(),
                accessibility_private::OnMenuClosed::kEventName,
                args.Pass());
}

void ExtensionAccessibilityEventRouter::OnChromeVoxLoadStateChanged(
    Profile* profile,
    bool loading,
    bool make_announcements) {
  scoped_ptr<base::ListValue> event_args(new base::ListValue());
  event_args->AppendBoolean(loading);
  event_args->AppendBoolean(make_announcements);
  ExtensionAccessibilityEventRouter::DispatchEventToChromeVox(
      profile,
      accessibility_private::OnChromeVoxLoadStateChanged::kEventName,
      event_args.Pass());
}

// Static.
void ExtensionAccessibilityEventRouter::DispatchEventToChromeVox(
    Profile* profile,
    const char* event_name,
    scoped_ptr<base::ListValue> event_args) {
  extensions::ExtensionSystem* system =
      extensions::ExtensionSystem::Get(profile);
  if (!system)
    return;
  scoped_ptr<extensions::Event> event(new extensions::Event(event_name,
                                                            event_args.Pass()));
  system->event_router()->DispatchEventWithLazyListener(
      extension_misc::kChromeVoxExtensionId, event.Pass());
}

void ExtensionAccessibilityEventRouter::DispatchEvent(
    Profile* profile,
    const char* event_name,
    scoped_ptr<base::ListValue> event_args) {
  if (!enabled_ || !profile)
    return;
  extensions::EventRouter* event_router = extensions::EventRouter::Get(profile);
  if (!event_router)
    return;

  scoped_ptr<extensions::Event> event(new extensions::Event(
      event_name, event_args.Pass()));
  event_router->BroadcastEvent(event.Pass());
}

bool AccessibilityPrivateSetAccessibilityEnabledFunction::RunSync() {
  bool enabled;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(0, &enabled));
  ExtensionAccessibilityEventRouter::GetInstance()
      ->SetAccessibilityEnabled(enabled);
  return true;
}

bool AccessibilityPrivateSetNativeAccessibilityEnabledFunction::RunSync() {
  bool enabled;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(0, &enabled));
  if (enabled) {
    content::BrowserAccessibilityState::GetInstance()->
        EnableAccessibility();
  } else {
    content::BrowserAccessibilityState::GetInstance()->
        DisableAccessibility();
  }
  return true;
}

bool AccessibilityPrivateGetFocusedControlFunction::RunSync() {
  // Get the serialized dict from the last focused control and return it.
  // However, if the dict is empty, that means we haven't seen any focus
  // events yet, so return null instead.
  ExtensionAccessibilityEventRouter *accessibility_event_router =
      ExtensionAccessibilityEventRouter::GetInstance();
  base::DictionaryValue *last_focused_control_dict =
      accessibility_event_router->last_focused_control_dict();
  if (last_focused_control_dict->size()) {
    SetResult(last_focused_control_dict->DeepCopyWithoutEmptyChildren());
  } else {
    SetResult(base::Value::CreateNullValue());
  }
  return true;
}

bool AccessibilityPrivateGetAlertsForTabFunction::RunSync() {
  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &tab_id));

  TabStripModel* tab_strip = NULL;
  content::WebContents* contents = NULL;
  int tab_index = -1;
  if (!extensions::ExtensionTabUtil::GetTabById(tab_id,
                                                GetProfile(),
                                                include_incognito(),
                                                NULL,
                                                &tab_strip,
                                                &contents,
                                                &tab_index)) {
    error_ = extensions::ErrorUtils::FormatErrorMessage(
        extensions::tabs_constants::kTabNotFoundError,
        base::IntToString(tab_id));
    return false;
  }

  base::ListValue* alerts_value = new base::ListValue;

  InfoBarService* infobar_service = InfoBarService::FromWebContents(contents);
  for (size_t i = 0; i < infobar_service->infobar_count(); ++i) {
    // TODO(hashimoto): Make other kind of alerts available.  crosbug.com/24281
    ConfirmInfoBarDelegate* confirm_infobar_delegate =
        infobar_service->infobar_at(i)->delegate()->AsConfirmInfoBarDelegate();
    if (confirm_infobar_delegate) {
      base::DictionaryValue* alert_value = new base::DictionaryValue;
      const base::string16 message_text =
          confirm_infobar_delegate->GetMessageText();
      alert_value->SetString(keys::kMessageKey, message_text);
      alerts_value->Append(alert_value);
    }
  }

  SetResult(alerts_value);
  return true;
}

bool AccessibilityPrivateSetFocusRingFunction::RunSync() {
#if defined(OS_CHROMEOS)
  base::ListValue* rect_values = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetList(0, &rect_values));

  std::vector<gfx::Rect> rects;
  for (size_t i = 0; i < rect_values->GetSize(); ++i) {
    base::DictionaryValue* rect_value = NULL;
    EXTENSION_FUNCTION_VALIDATE(rect_values->GetDictionary(i, &rect_value));
    int left, top, width, height;
    EXTENSION_FUNCTION_VALIDATE(rect_value->GetInteger(keys::kLeft, &left));
    EXTENSION_FUNCTION_VALIDATE(rect_value->GetInteger(keys::kTop, &top));
    EXTENSION_FUNCTION_VALIDATE(rect_value->GetInteger(keys::kWidth, &width));
    EXTENSION_FUNCTION_VALIDATE(rect_value->GetInteger(keys::kHeight, &height));
    rects.push_back(gfx::Rect(left, top, width, height));
  }

  chromeos::AccessibilityFocusRingController::GetInstance()->SetFocusRing(
      rects);
  return true;
#endif  // defined(OS_CHROMEOS)

  error_ = keys:: kErrorNotSupported;
  return false;
}
