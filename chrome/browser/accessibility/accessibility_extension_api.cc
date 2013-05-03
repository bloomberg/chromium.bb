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
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/infobars/infobar_delegate.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/notification_service.h"
#include "extensions/common/error_utils.h"

namespace keys = extension_accessibility_api_constants;

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
  registrar_.Add(this,
                 chrome::NOTIFICATION_ACCESSIBILITY_WINDOW_OPENED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_ACCESSIBILITY_WINDOW_CLOSED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_ACCESSIBILITY_CONTROL_FOCUSED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_ACCESSIBILITY_CONTROL_ACTION,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_ACCESSIBILITY_TEXT_CHANGED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_ACCESSIBILITY_MENU_OPENED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_ACCESSIBILITY_MENU_CLOSED,
                 content::NotificationService::AllSources());
}

ExtensionAccessibilityEventRouter::~ExtensionAccessibilityEventRouter() {
}

void ExtensionAccessibilityEventRouter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_ACCESSIBILITY_WINDOW_OPENED:
      OnWindowOpened(
          content::Details<const AccessibilityWindowInfo>(details).ptr());
      break;
    case chrome::NOTIFICATION_ACCESSIBILITY_WINDOW_CLOSED:
      OnWindowClosed(
          content::Details<const AccessibilityWindowInfo>(details).ptr());
      break;
    case chrome::NOTIFICATION_ACCESSIBILITY_CONTROL_FOCUSED:
      OnControlFocused(
          content::Details<const AccessibilityControlInfo>(details).ptr());
      break;
    case chrome::NOTIFICATION_ACCESSIBILITY_CONTROL_ACTION:
      OnControlAction(
          content::Details<const AccessibilityControlInfo>(details).ptr());
      break;
    case chrome::NOTIFICATION_ACCESSIBILITY_TEXT_CHANGED:
      OnTextChanged(
          content::Details<const AccessibilityControlInfo>(details).ptr());
      break;
    case chrome::NOTIFICATION_ACCESSIBILITY_MENU_OPENED:
      OnMenuOpened(
          content::Details<const AccessibilityMenuInfo>(details).ptr());
      break;
    case chrome::NOTIFICATION_ACCESSIBILITY_MENU_CLOSED:
      OnMenuClosed(
          content::Details<const AccessibilityMenuInfo>(details).ptr());
      break;
    default:
      NOTREACHED();
  }
}

void ExtensionAccessibilityEventRouter::SetAccessibilityEnabled(bool enabled) {
  enabled_ = enabled;
}

bool ExtensionAccessibilityEventRouter::IsAccessibilityEnabled() const {
  return enabled_;
}

void ExtensionAccessibilityEventRouter::OnWindowOpened(
    const AccessibilityWindowInfo* info) {
  scoped_ptr<ListValue> args(ControlInfoToEventArguments(info));
  DispatchEvent(info->profile(), keys::kOnWindowOpened, args.Pass());
}

void ExtensionAccessibilityEventRouter::OnWindowClosed(
    const AccessibilityWindowInfo* info) {
  scoped_ptr<ListValue> args(ControlInfoToEventArguments(info));
  DispatchEvent(info->profile(), keys::kOnWindowClosed, args.Pass());
}

void ExtensionAccessibilityEventRouter::OnControlFocused(
    const AccessibilityControlInfo* info) {
  last_focused_control_dict_.Clear();
  info->SerializeToDict(&last_focused_control_dict_);
  scoped_ptr<ListValue> args(ControlInfoToEventArguments(info));
  DispatchEvent(info->profile(), keys::kOnControlFocused, args.Pass());
}

void ExtensionAccessibilityEventRouter::OnControlAction(
    const AccessibilityControlInfo* info) {
  scoped_ptr<ListValue> args(ControlInfoToEventArguments(info));
  DispatchEvent(info->profile(), keys::kOnControlAction, args.Pass());
}

void ExtensionAccessibilityEventRouter::OnTextChanged(
    const AccessibilityControlInfo* info) {
  scoped_ptr<ListValue> args(ControlInfoToEventArguments(info));
  DispatchEvent(info->profile(), keys::kOnTextChanged, args.Pass());
}

void ExtensionAccessibilityEventRouter::OnMenuOpened(
    const AccessibilityMenuInfo* info) {
  scoped_ptr<ListValue> args(ControlInfoToEventArguments(info));
  DispatchEvent(info->profile(), keys::kOnMenuOpened, args.Pass());
}

void ExtensionAccessibilityEventRouter::OnMenuClosed(
    const AccessibilityMenuInfo* info) {
  scoped_ptr<ListValue> args(ControlInfoToEventArguments(info));
  DispatchEvent(info->profile(), keys::kOnMenuClosed, args.Pass());
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
  if (!ExtensionTabUtil::GetTabById(tab_id, profile(), include_incognito(),
                                    NULL, &tab_strip, &contents, &tab_index)) {
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
