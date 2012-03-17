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

bool BrowserActionFunction::RunImpl() {
  ExtensionActionFunction::RunImpl();
  extension_action_ = GetExtension()->browser_action();
  if (!extension_action_) {
    error_ = kNoBrowserActionError;
    return false;
  }

  if (!RunExtensionAction())
    return false;

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_BROWSER_ACTION_UPDATED,
      content::Source<ExtensionAction>(extension_action_),
      content::NotificationService::NoDetails());
  return true;
}

bool BrowserActionSetIconFunction::RunExtensionAction() {
  return SetIcon();
}

bool BrowserActionSetTitleFunction::RunExtensionAction() {
  return SetTitle();
}

bool BrowserActionSetPopupFunction::RunExtensionAction() {
  return SetPopup();
}

bool BrowserActionSetBadgeTextFunction::RunExtensionAction() {
  return SetBadgeText();
}

bool BrowserActionSetBadgeBackgroundColorFunction::RunExtensionAction() {
  return SetBadgeBackgroundColor();
}
