// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automation/extension_proxy.h"

#include "base/string_number_conversions.h"
#include "chrome/common/automation_messages.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"

ExtensionProxy::ExtensionProxy(AutomationMessageSender* sender,
                               AutomationHandleTracker* tracker,
                               int handle)
    : AutomationResourceProxy(tracker, sender, handle) {
}

bool ExtensionProxy::Uninstall() {
  if (!is_valid())
    return false;

  bool success = false;
  if (!sender_->Send(new AutomationMsg_UninstallExtension(0, handle_,
                                                          &success)))
    return false;
  return success;
}

bool ExtensionProxy::Enable() {
  if (!is_valid())
    return false;

  bool success = false;
  if (!sender_->Send(new AutomationMsg_EnableExtension(0, handle_,
                                                       &success)))
    return false;
  return success;
}

bool ExtensionProxy::Disable() {
  if (!is_valid())
    return false;

  bool success = false;
  if (!sender_->Send(new AutomationMsg_DisableExtension(0, handle_,
                                                        &success)))
    return false;
  return success;
}

bool ExtensionProxy::ExecuteActionInActiveTabAsync(BrowserProxy* browser) {
  if (!is_valid())
    return false;

  bool success = false;
  if (!sender_->Send(new AutomationMsg_ExecuteExtensionActionInActiveTabAsync(
      0, handle_, browser->handle(), &success)))
    return false;
  return success;
}

bool ExtensionProxy::MoveBrowserAction(int index) {
  if (!is_valid())
    return false;
  bool success = false;
  if (!sender_->Send(
      new AutomationMsg_MoveExtensionBrowserAction(0, handle_, index,
                                                   &success)))
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

void ExtensionProxy::EnsureIdMatches(const std::string& expected_id) {
  std::string id;
  ASSERT_TRUE(GetId(&id));
  ASSERT_EQ(expected_id, id);
}

void ExtensionProxy::EnsureNameMatches(const std::string& expected_name) {
  std::string name;
  ASSERT_TRUE(GetName(&name));
  ASSERT_EQ(expected_name, name);
}

void ExtensionProxy::EnsureVersionMatches(const std::string& expected_version) {
  std::string version;
  ASSERT_TRUE(GetVersion(&version));
  ASSERT_EQ(expected_version, version);
}

void ExtensionProxy::EnsureBrowserActionIndexMatches(int expected_index) {
  int index;
  ASSERT_TRUE(GetBrowserActionIndex(&index));
  ASSERT_EQ(expected_index, index);
}

bool ExtensionProxy::GetProperty(AutomationMsg_ExtensionProperty type,
                                 std::string* value) {
  DCHECK(value);
  if (!is_valid())
    return false;

  bool success = false;
  if (!sender_->Send(new AutomationMsg_GetExtensionProperty(0, handle_, type,
                                                            &success, value)))
    return false;
  return success;
}
