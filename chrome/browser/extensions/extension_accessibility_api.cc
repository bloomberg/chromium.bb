// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tabs_module.h"

#include "base/json/json_writer.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_accessibility_api.h"
#include "chrome/browser/extensions/extension_accessibility_api_constants.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_service.h"

namespace keys = extension_accessibility_api_constants;

// Returns the AccessibilityControlInfo serialized into a JSON string,
// consisting of an array of a single object of type AccessibilityObject,
// as defined in the accessibility extension api's json schema.
std::string ControlInfoToJsonString(const AccessibilityEventInfo* info) {
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  info->SerializeToDict(dict);
  args.Append(dict);
  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  return json_args;
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
  registrar_.Add(this,
                 chrome::NOTIFICATION_ACCESSIBILITY_VOLUME_CHANGED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_ACCESSIBILITY_SCREEN_UNLOCKED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_ACCESSIBILITY_WOKE_UP,
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
    case chrome::NOTIFICATION_ACCESSIBILITY_VOLUME_CHANGED:
      OnVolumeChanged(
          content::Details<const AccessibilityVolumeInfo>(details).ptr());
      break;
    case chrome::NOTIFICATION_ACCESSIBILITY_SCREEN_UNLOCKED:
      OnScreenUnlocked(
          content::Details<const ScreenUnlockedEventInfo>(details).ptr());
      break;
    case chrome::NOTIFICATION_ACCESSIBILITY_WOKE_UP:
      OnWokeUp(
          content::Details<const WokeUpEventInfo>(details).ptr());
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
  std::string json_args = ControlInfoToJsonString(info);
  DispatchEvent(info->profile(), keys::kOnWindowOpened, json_args);
}

void ExtensionAccessibilityEventRouter::OnWindowClosed(
    const AccessibilityWindowInfo* info) {
  std::string json_args = ControlInfoToJsonString(info);
  DispatchEvent(info->profile(), keys::kOnWindowClosed, json_args);
}

void ExtensionAccessibilityEventRouter::OnControlFocused(
    const AccessibilityControlInfo* info) {
  last_focused_control_dict_.Clear();
  info->SerializeToDict(&last_focused_control_dict_);
  std::string json_args = ControlInfoToJsonString(info);
  DispatchEvent(info->profile(), keys::kOnControlFocused, json_args);
}

void ExtensionAccessibilityEventRouter::OnControlAction(
    const AccessibilityControlInfo* info) {
  std::string json_args = ControlInfoToJsonString(info);
  DispatchEvent(info->profile(), keys::kOnControlAction, json_args);
}

void ExtensionAccessibilityEventRouter::OnTextChanged(
    const AccessibilityControlInfo* info) {
  std::string json_args = ControlInfoToJsonString(info);
  DispatchEvent(info->profile(), keys::kOnTextChanged, json_args);
}

void ExtensionAccessibilityEventRouter::OnMenuOpened(
    const AccessibilityMenuInfo* info) {
  std::string json_args = ControlInfoToJsonString(info);
  DispatchEvent(info->profile(), keys::kOnMenuOpened, json_args);
}

void ExtensionAccessibilityEventRouter::OnMenuClosed(
    const AccessibilityMenuInfo* info) {
  std::string json_args = ControlInfoToJsonString(info);
  DispatchEvent(info->profile(), keys::kOnMenuClosed, json_args);
}

void ExtensionAccessibilityEventRouter::OnVolumeChanged(
    const AccessibilityVolumeInfo* info) {
  std::string json_args = ControlInfoToJsonString(info);
  DispatchEvent(info->profile(), keys::kOnVolumeChanged, json_args);
}

void ExtensionAccessibilityEventRouter::OnScreenUnlocked(
    const ScreenUnlockedEventInfo* info) {
  std::string json_args = ControlInfoToJsonString(info);
  DispatchEvent(info->profile(), keys::kOnScreenUnlocked, json_args);
}

void ExtensionAccessibilityEventRouter::OnWokeUp(const WokeUpEventInfo* info) {
  std::string json_args = ControlInfoToJsonString(info);
  DispatchEvent(info->profile(), keys::kOnWokeUp, json_args);
}

void ExtensionAccessibilityEventRouter::DispatchEvent(
    Profile* profile,
    const char* event_name,
    const std::string& json_args) {
  if (enabled_ && profile && profile->GetExtensionEventRouter()) {
    profile->GetExtensionEventRouter()->DispatchEventToRenderers(
        event_name, json_args, NULL, GURL());
  }
}

bool SetAccessibilityEnabledFunction::RunImpl() {
  bool enabled;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(0, &enabled));
  ExtensionAccessibilityEventRouter::GetInstance()
      ->SetAccessibilityEnabled(enabled);
  return true;
}

bool GetFocusedControlFunction::RunImpl() {
  // Get the serialized dict from the last focused control and return it.
  // However, if the dict is empty, that means we haven't seen any focus
  // events yet, so return null instead.
  ExtensionAccessibilityEventRouter *accessibility_event_router =
      ExtensionAccessibilityEventRouter::GetInstance();
  DictionaryValue *last_focused_control_dict =
      accessibility_event_router->last_focused_control_dict();
  if (last_focused_control_dict->size()) {
    result_.reset(last_focused_control_dict->DeepCopyWithoutEmptyChildren());
  } else {
    result_.reset(Value::CreateNullValue());
  }
  return true;
}
