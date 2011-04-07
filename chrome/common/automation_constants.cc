// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/automation_constants.h"

namespace automation {

// JSON value labels for proxy settings that are passed in via
// AutomationMsg_SetProxyConfig.
const char kJSONProxyAutoconfig[] = "proxy.autoconfig";
const char kJSONProxyNoProxy[] = "proxy.no_proxy";
const char kJSONProxyPacUrl[] = "proxy.pac_url";
const char kJSONProxyBypassList[] = "proxy.bypass_list";
const char kJSONProxyServer[] = "proxy.server";

// Named testing interface is used when you want to connect an
// AutomationProxy to an already-running browser instance.
const char kNamedInterfacePrefix[] = "NamedTestingInterface:";

const int kChromeDriverAutomationVersion = 1;

}  // namespace automation
