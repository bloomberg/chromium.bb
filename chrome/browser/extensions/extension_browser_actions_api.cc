// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browser_actions_api.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/common/notification_service.h"

namespace extension_browser_actions_api_constants {

const char kSetNameFunction[] = "browserAction.setName";
const char kSetIconFunction[] = "browserAction.setIcon";

}  // namespace extension_browser_actions_api_constants

namespace {
// Errors.
const char kNoBrowserActionError[] =
    "This extension has no browser action specified.";
}

bool BrowserActionSetNameFunction::RunImpl() {
  std::string name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetAsString(&name));

  Extension* extension = dispatcher()->GetExtension();
  if (!extension->browser_action()) {
    error_ = kNoBrowserActionError;
    return false;
  }

  extension->browser_action_state()->set_title(name);

  NotificationService::current()->Notify(
      NotificationType::EXTENSION_BROWSER_ACTION_UPDATED,
      Source<ExtensionAction>(extension->browser_action()),
      Details<ExtensionActionState>(extension->browser_action_state()));
  return true;
}

bool BrowserActionSetIconFunction::RunImpl() {
  int icon_index = -1;
  EXTENSION_FUNCTION_VALIDATE(args_->GetAsInteger(&icon_index));

  Extension* extension = dispatcher()->GetExtension();
  if (!extension->browser_action()) {
    error_ = kNoBrowserActionError;
    return false;
  }

  extension->browser_action_state()->set_icon_index(icon_index);

  NotificationService::current()->Notify(
      NotificationType::EXTENSION_BROWSER_ACTION_UPDATED,
      Source<ExtensionAction>(extension->browser_action()),
      Details<ExtensionActionState>(extension->browser_action_state()));
  return true;
}
