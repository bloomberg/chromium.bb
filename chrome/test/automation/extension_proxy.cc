// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automation/extension_proxy.h"

#include "base/string_number_conversions.h"
#include "chrome/common/automation_messages.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"

ExtensionProxy::ExtensionProxy(AutomationMessageSender* sender,
                               AutomationHandleTracker* tracker,
                               int handle)
    : AutomationResourceProxy(tracker, sender, handle) {
}

bool ExtensionProxy::Uninstall() {
  if (!is_valid())
    return false;

  bool success = false;
  if (!sender_->Send(new AutomationMsg_UninstallExtension(handle_, &success)))
    return false;
  return success;
}

bool ExtensionProxy::Enable() {
  if (!is_valid())
    return false;

  bool success = false;
  if (!sender_->Send(new AutomationMsg_EnableExtension(handle_, &success)))
    return false;
  return success;
}

bool ExtensionProxy::Disable() {
  if (!is_valid())
    return false;

  bool success = false;
  if (!sender_->Send(new AutomationMsg_DisableExtension(handle_, &success)))
    return false;
  return success;
}

bool ExtensionProxy::ExecuteActionInActiveTabAsync(BrowserProxy* browser) {
  if (!is_valid())
    return false;

  bool success = false;
  if (!sender_->Send(new AutomationMsg_ExecuteExtensionActionInActiveTabAsync(
      handle_, browser->handle(), &success)))
    return false;
  return success;
}

bool ExtensionProxy::MoveBrowserAction(int index) {
  if (!is_valid())
    return false;
  bool success = false;
  if (!sender_->Send(
      new AutomationMsg_MoveExtensionBrowserAction(handle_, index, &success)))
    return false;
  return success;
}

bool ExtensionProxy::GetId(std::string* id) {
  DCHECK(id);
  return GetProperty(AUTOMATION_MSG_EXTENSION_ID, id);
}

bool ExtensionProxy::GetName(std::string* name) {
  DCHECK(name);
  return GetProperty(AUTOMATION_MSG_EXTENSION_NAME, name);
}

bool ExtensionProxy::GetVersion(std::string* version) {
  DCHECK(version);
  return GetProperty(AUTOMATION_MSG_EXTENSION_VERSION, version);
}

bool ExtensionProxy::GetBrowserActionIndex(int* index) {
  DCHECK(index);
  std::string index_string;
  if (!GetProperty(AUTOMATION_MSG_EXTENSION_BROWSER_ACTION_INDEX,
                   &index_string))
    return false;
  // Do not modify |index| until we are sure we can get the value, just to be
  // nice to the caller.
  int converted_index;
  if (!base::StringToInt(index_string, &converted_index)) {
    LOG(ERROR) << "Received index string could not be converted to int: "
               << index_string;
    return false;
  }
  *index = converted_index;
  return true;
}

bool ExtensionProxy::GetProperty(AutomationMsg_ExtensionProperty type,
                                 std::string* value) {
  DCHECK(value);
  if (!is_valid())
    return false;

  bool success = false;
  if (!sender_->Send(new AutomationMsg_GetExtensionProperty(handle_, type,
                                                            &success, value)))
    return false;
  return success;
}
