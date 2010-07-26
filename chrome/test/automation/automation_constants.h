// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_AUTOMATION_CONSTANTS_H_
#define CHROME_TEST_AUTOMATION_AUTOMATION_CONSTANTS_H_
#pragma once

namespace automation {
// Amount of time to wait before querying the browser.
static const int kSleepTime = 250;
}

// Used by AutomationProxy, declared here so that other headers don't need
// to include automation_proxy.h.
enum AutomationLaunchResult {
  AUTOMATION_SUCCESS,
  AUTOMATION_TIMEOUT,
  AUTOMATION_VERSION_MISMATCH,
  AUTOMATION_CREATE_TAB_FAILED
};

enum AutomationMsg_NavigationResponseValues {
  AUTOMATION_MSG_NAVIGATION_ERROR = 0,
  AUTOMATION_MSG_NAVIGATION_SUCCESS,
  AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
};

enum AutomationMsg_ExtensionResponseValues {
  AUTOMATION_MSG_EXTENSION_INSTALL_SUCCEEDED = 0,
  AUTOMATION_MSG_EXTENSION_INSTALL_FAILED
};

// Used in the AutomationMsg_GetExtensionProperty to identify which extension
// property should be retrieved, instead of having separate messages for each
// property.
enum AutomationMsg_ExtensionProperty {
  AUTOMATION_MSG_EXTENSION_ID = 0,
  AUTOMATION_MSG_EXTENSION_NAME,
  AUTOMATION_MSG_EXTENSION_VERSION,
  AUTOMATION_MSG_EXTENSION_BROWSER_ACTION_INDEX,
};

#endif  // CHROME_TEST_AUTOMATION_AUTOMATION_CONSTANTS_H_
