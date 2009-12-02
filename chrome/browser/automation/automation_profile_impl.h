// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_PROFILE_IMPL_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_PROFILE_IMPL_H_

#include "ipc/ipc_message.h"

class Profile;
class ChromeURLRequestContextGetter;
class AutomationResourceMessageFilter;

namespace AutomationRequestContext {

// Returns the URL request context to be used by HTTP requests handled over
// the automation channel.
ChromeURLRequestContextGetter* CreateAutomationURLRequestContextForTab(
    int tab_handle,
    Profile* profile,
    AutomationResourceMessageFilter* automation_client);

}

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_PROFILE_IMPL_H_
