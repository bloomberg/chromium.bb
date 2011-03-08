// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tabs_module.h"

#include "base/json/json_writer.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/extension_accessibility_api.h"
#include "chrome/browser/extensions/extension_accessibility_api_constants.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "content/common/notification_service.h"

namespace keys = extension_accessibility_api_constants;

// Returns the AccessibilityControlInfo serialized into a JSON string,
// consisting of an array of a single object of type AccessibilityObject,
// as defined in the accessibility extension api's json schema.
std::string ControlInfoToJsonString(const AccessibilityControlInfo* info) {
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
    : enabled_(false) {}

ExtensionAccessibilityEventRouter::~ExtensionAccessibilityEventRouter() {
  STLDeleteElements(&on_enabled_listeners_);
  STLDeleteElements(&on_disabled_listeners_);
}

void ExtensionAccessibilityEventRouter::ObserveProfile(Profile* profile) {
  last_focused_control_dict_.Clear();

  if (registrar_.IsEmpty()) {
    registrar_.Add(this,
                   NotificationType::ACCESSIBILITY_WINDOW_OPENED,
                   NotificationService::AllSources());
    registrar_.Add(this,
                   NotificationType::ACCESSIBILITY_WINDOW_CLOSED,
                   NotificationService::AllSources());
    registrar_.Add(this,
                   NotificationType::ACCESSIBILITY_CONTROL_FOCUSED,
                   NotificationService::AllSources());
    registrar_.Add(this,
                   NotificationType::ACCESSIBILITY_CONTROL_ACTION,
                   NotificationService::AllSources());
    registrar_.Add(this,
                   NotificationType::ACCESSIBILITY_TEXT_CHANGED,
                   NotificationService::AllSources());
    registrar_.Add(this,
                   NotificationType::ACCESSIBILITY_MENU_OPENED,
                   NotificationService::AllSources());
    registrar_.Add(this,
                   NotificationType::ACCESSIBILITY_MENU_CLOSED,
                   NotificationService::AllSources());
  }
}

void ExtensionAccessibilityEventRouter::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::ACCESSIBILITY_WINDOW_OPENED:
      OnWindowOpened(Details<const AccessibilityWindowInfo>(details).ptr());
      break;
    case NotificationType::ACCESSIBILITY_WINDOW_CLOSED:
      OnWindowClosed(Details<const AccessibilityWindowInfo>(details).ptr());
      break;
    case NotificationType::ACCESSIBILITY_CONTROL_FOCUSED:
      OnControlFocused(Details<const AccessibilityControlInfo>(details).ptr());
      break;
    case NotificationType::ACCESSIBILITY_CONTROL_ACTION:
      OnControlAction(Details<const AccessibilityControlInfo>(details).ptr());
      break;
    case NotificationType::ACCESSIBILITY_TEXT_CHANGED:
      OnTextChanged(Details<const AccessibilityControlInfo>(details).ptr());
      break;
    case NotificationType::ACCESSIBILITY_MENU_OPENED:
      OnMenuOpened(Details<const AccessibilityMenuInfo>(details).ptr());
      break;
    case NotificationType::ACCESSIBILITY_MENU_CLOSED:
      OnMenuClosed(Details<const AccessibilityMenuInfo>(details).ptr());
      break;
    default:
      NOTREACHED();
  }
}

void ExtensionAccessibilityEventRouter::SetAccessibilityEnabled(bool enabled) {
  if (enabled_ != enabled) {
    enabled_ = enabled;
    if (enabled_) {
      for (unsigned int i = 0; i < on_enabled_listeners_.size(); i++) {
        on_enabled_listeners_[i]->Run();
      }
    } else {
      for (unsigned int i = 0; i < on_disabled_listeners_.size(); i++) {
        on_disabled_listeners_[i]->Run();
      }
    }
  }
}

bool ExtensionAccessibilityEventRouter::IsAccessibilityEnabled() const {
  return enabled_;
}

void ExtensionAccessibilityEventRouter::AddOnEnabledListener(
    Callback* callback) {
  on_enabled_listeners_.push_back(callback);
}

void ExtensionAccessibilityEventRouter::AddOnDisabledListener(
    Callback* callback) {
  on_disabled_listeners_.push_back(callback);
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
