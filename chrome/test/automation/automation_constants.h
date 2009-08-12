// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_AUTOMATION_CONSTANTS_H_
#define CHROME_TEST_AUTOMATION_AUTOMATION_CONSTANTS_H_

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

#if !defined(OS_WIN)

// WebKit defines a larger set of these in
// WebKit/WebCore/platform/KeyboardCodes.h but I don't think we want to include
// that from here, and besides we only care about a subset of those.
const int VK_TAB = 0x09;
const int VK_RETURN = 0x0D;
const int VK_ESCAPE = 0x1B;
const int VK_SPACE = 0x20;
const int VK_PRIOR = 0x21;
const int VK_NEXT = 0x22;
const int VK_UP = 0x26;
const int VK_DOWN = 0x28;

#endif  // !defined(OS_WIN)

#endif  // CHROME_TEST_AUTOMATION_AUTOMATION_CONSTANTS_H_
