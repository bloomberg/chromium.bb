// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_AUTOMATION_JSON_REQUESTS_H_
#define CHROME_TEST_AUTOMATION_AUTOMATION_JSON_REQUESTS_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "chrome/common/automation_constants.h"

class AutomationMessageSender;
class GURL;

// Sends a JSON request to the chrome automation provider. Returns true
// if the JSON request was successfully sent and the reply was received.
// If true, |success| will be set to whether the JSON request was
// completed successfully by the automation provider.
bool SendAutomationJSONRequest(AutomationMessageSender* sender,
                               const std::string& request,
                               std::string* reply,
                               bool* success) WARN_UNUSED_RESULT;

// Requests the current browser and tab indices for the given |TabProxy|
// handle. Returns true on success.
bool SendGetIndicesFromTabJSONRequest(
    AutomationMessageSender* sender,
    int handle,
    int* browser_index,
    int* tab_index) WARN_UNUSED_RESULT;

// Requests to navigate to the given url and wait for the given number of
// navigations to complete. Returns true on success.
bool SendNavigateToURLJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    const GURL& url,
    int navigation_count,
    AutomationMsg_NavigationResponseValues* nav_response) WARN_UNUSED_RESULT;

#endif  // CHROME_TEST_AUTOMATION_AUTOMATION_JSON_REQUESTS_H_
