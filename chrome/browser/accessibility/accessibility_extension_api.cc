// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/accessibility_extension_api.h"

#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/accessibility/accessibility_extension_api_constants.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/infobars/infobar_delegate.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/experimental_accessibility.h"
#include "chrome/common/extensions/background_info.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "extensions/browser/lazy_background_task_queue.h"
#include "extensions/common/error_utils.h"

namespace keys = extension_accessibility_api_constants;
namespace experimental_accessibility =
    extensions::api::experimental_accessibility;

// Returns the AccessibilityControlInfo serialized into a JSON string,
// consisting of an array of a single object of type AccessibilityObject,
// as defined in the accessibility extension api's json schema.
scoped_ptr<ListValue> ControlInfoToEventArguments(
    const AccessibilityEventInfo* info) {
  DictionaryValue* dict = new DictionaryValue();
  info->SerializeToDict(dict);

  scoped_ptr<ListValue> args(new ListValue());
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
    ui::AccessibilityTypes::Event event,
    const AccessibilityWindowInfo* info) {
  if (event == ui::AccessibilityTypes::EVENT_ALERT)
    OnWindowOpened(info);
}

void ExtensionAccessibilityEventRouter::HandleMenuEvent(
    ui::AccessibilityTypes::Event event,
    const AccessibilityMenuInfo* info) {
  switch (event) {
    case ui::AccessibilityTypes::EVENT_MENUSTART:
    case ui::AccessibilityTypes::EVENT_MENUPOPUPSTART:
      OnMenuOpened(info);
      break;
    case ui::AccessibilityTypes::EVENT_MENUEND:
    case ui::AccessibilityTypes::EVENT_MENUPOPUPEND:
      OnMenuClosed(info);
      break;
    default:
      NOTREACHED();
  }
}

void ExtensionAccessibilityEventRouter::HandleControlEvent(
    ui::AccessibilityTypes::Event event,
    const AccessibilityControlInfo* info) {
  if (!control_event_callback_.is_null())
    control_event_callback_.Run(event, info);

  switch (event) {
    case ui::AccessibilityTypes::EVENT_TEXT_CHANGED:
    case ui::AccessibilityTypes::EVENT_SELECTION_CHANGED:
      OnTextChanged(info);
      break;
    case ui::AccessibilityTypes::EVENT_VALUE_CHANGED:
      OnControlAction(info);
      break;
    case ui::AccessibilityTypes::EVENT_FOCUS:
      OnControlFocused(info);
      break;
    default:
      NOTREACHED();
  }
}

void ExtensionAccessibilityEventRouter::OnWindowOpened(
    const AccessibilityWindowInfo* info) {
  scoped_ptr<ListValue> args(ControlInfoToEventArguments(info));
  DispatchEvent(info->profile(),
                experimental_accessibility::OnWindowOpened::kEventName,
                args.Pass());
}

void ExtensionAccessibilityEventRouter::OnControlFocused(
    const AccessibilityControlInfo* info) {
  last_focused_control_dict_.Clear();
  info->SerializeToDict(&last_focused_control_dict_);
  scoped_ptr<ListValue> args(ControlInfoToEventArguments(info));
  DispatchEvent(info->profile(),
                experimental_accessibility::OnControlFocused::kEventName,
                args.Pass());
}

void ExtensionAccessibilityEventRouter::OnControlAction(
    const AccessibilityControlInfo* info) {
  scoped_ptr<ListValue> args(ControlInfoToEventArguments(info));
  DispatchEvent(info->profile(),
                experimental_accessibility::OnControlAction::kEventName,
                args.Pass());
}

void ExtensionAccessibilityEventRouter::OnTextChanged(
    const AccessibilityControlInfo* info) {
  scoped_ptr<ListValue> args(ControlInfoToEventArguments(info));
  DispatchEvent(info->profile(),
                experimental_accessibility::OnTextChanged::kEventName,
                args.Pass());
}

void ExtensionAccessibilityEventRouter::OnMenuOpened(
    const AccessibilityMenuInfo* info) {
  scoped_ptr<ListValue> args(ControlInfoToEventArguments(info));
  DispatchEvent(info->profile(),
                experimental_accessibility::OnMenuOpened::kEventName,
                args.Pass());
}

void ExtensionAccessibilityEventRouter::OnMenuClosed(
    const AccessibilityMenuInfo* info) {
  scoped_ptr<ListValue> args(ControlInfoToEventArguments(info));
  DispatchEvent(info->profile(),
                experimental_accessibility::OnMenuClosed::kEventName,
                args.Pass());
}

void ExtensionAccessibilityEventRouter::OnChromeVoxLoadStateChanged(
    Profile* profile,
    bool loading,
    bool make_announcements) {
  scoped_ptr<base::ListValue> event_args(new base::ListValue());
  event_args->Append(base::Value::CreateBooleanValue(loading));
  event_args->Append(base::Value::CreateBooleanValue(make_announcements));
  ExtensionAccessibilityEventRouter::DispatchEventToChromeVox(profile,
      experimental_accessibility::OnChromeVoxLoadStateChanged::kEventName,
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
  system->event_router()->DispatchEventToExtension(
      extension_misc::kChromeVoxExtensionId, event.Pass());
}

void ExtensionAccessibilityEventRouter::DispatchEvent(
    Profile* profile,
    const char* event_name,
    scoped_ptr<base::ListValue> event_args) {
  if (enabled_ && profile &&
      extensions::ExtensionSystem::Get(profile)->event_router()) {
    scoped_ptr<extensions::Event> event(new extensions::Event(
        event_name, event_args.Pass()));
    extensions::ExtensionSystem::Get(profile)->event_router()->
        BroadcastEvent(event.Pass());
  }
}

bool AccessibilitySetAccessibilityEnabledFunction::RunImpl() {
  bool enabled;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(0, &enabled));
  ExtensionAccessibilityEventRouter::GetInstance()
      ->SetAccessibilityEnabled(enabled);
  return true;
}

bool AccessibilitySetNativeAccessibilityEnabledFunction::RunImpl() {
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

bool AccessibilityGetFocusedControlFunction::RunImpl() {
  // Get the serialized dict from the last focused control and return it.
  // However, if the dict is empty, that means we haven't seen any focus
  // events yet, so return null instead.
  ExtensionAccessibilityEventRouter *accessibility_event_router =
      ExtensionAccessibilityEventRouter::GetInstance();
  DictionaryValue *last_focused_control_dict =
      accessibility_event_router->last_focused_control_dict();
  if (last_focused_control_dict->size()) {
    SetResult(last_focused_control_dict->DeepCopyWithoutEmptyChildren());
  } else {
    SetResult(Value::CreateNullValue());
  }
  return true;
}

bool AccessibilityGetAlertsForTabFunction::RunImpl() {
  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &tab_id));

  TabStripModel* tab_strip = NULL;
  content::WebContents* contents = NULL;
  int tab_index = -1;
  if (!ExtensionTabUtil::GetTabById(tab_id,
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

  ListValue* alerts_value = new ListValue;

  InfoBarService* infobar_service = InfoBarService::FromWebContents(contents);
  for (size_t i = 0; i < infobar_service->infobar_count(); ++i) {
    // TODO(hashimoto): Make other kind of alerts available.  crosbug.com/24281
    ConfirmInfoBarDelegate* confirm_infobar_delegate =
        infobar_service->infobar_at(i)->AsConfirmInfoBarDelegate();
    if (confirm_infobar_delegate) {
      DictionaryValue* alert_value = new DictionaryValue;
      const string16 message_text = confirm_infobar_delegate->GetMessageText();
      alert_value->SetString(keys::kMessageKey, message_text);
      alerts_value->Append(alert_value);
    }
  }

  SetResult(alerts_value);
  return true;
}
