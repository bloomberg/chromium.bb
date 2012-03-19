// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/extension_action/extension_browser_actions_api.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_service.h"

namespace {
// Errors.
const char kNoBrowserActionError[] =
    "This extension has no browser action specified.";
}

void BrowserActionFunction::FireUpdateNotification() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_BROWSER_ACTION_UPDATED,
      content::Source<ExtensionAction>(extension_action_),
      content::NotificationService::NoDetails());
}

bool BrowserActionFunction::RunImpl() {
  ExtensionActionFunction::RunImpl();
  extension_action_ = GetExtension()->browser_action();
  if (!extension_action_) {
    error_ = kNoBrowserActionError;
    return false;
  }

  return RunExtensionAction();
}

bool BrowserActionSetIconFunction::RunExtensionAction() {
  if (!SetIcon())
    return false;
  FireUpdateNotification();
  return true;
}

bool BrowserActionSetTitleFunction::RunExtensionAction() {
  if (!SetTitle())
    return false;
  FireUpdateNotification();
  return true;
}

bool BrowserActionSetPopupFunction::RunExtensionAction() {
  if (!SetPopup())
    return false;
  FireUpdateNotification();
  return true;
}

bool BrowserActionSetBadgeTextFunction::RunExtensionAction() {
  if (!SetBadgeText())
    return false;
  FireUpdateNotification();
  return true;
}

bool BrowserActionSetBadgeBackgroundColorFunction::RunExtensionAction() {
  if (!SetBadgeBackgroundColor())
    return false;
  FireUpdateNotification();
  return true;
}

bool BrowserActionGetTitleFunction::RunExtensionAction() {
  return GetTitle();
}

bool BrowserActionGetPopupFunction::RunExtensionAction() {
  return GetPopup();
}

bool BrowserActionGetBadgeTextFunction::RunExtensionAction() {
  return GetBadgeText();
}

bool BrowserActionGetBadgeBackgroundColorFunction::RunExtensionAction() {
  return GetBadgeBackgroundColor();
}
