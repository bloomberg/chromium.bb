// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/api/shell_gcd/shell_gcd_api.h"

#include <string>

#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/privet_daemon_client.h"
#include "extensions/shell/common/api/shell_gcd.h"

namespace gcd = extensions::shell::api::shell_gcd;

namespace extensions {

ShellGcdPingFunction::ShellGcdPingFunction() {
}

ShellGcdPingFunction::~ShellGcdPingFunction() {
}

ExtensionFunction::ResponseAction ShellGcdPingFunction::Run() {
  // |this| is refcounted so we don't need the usual DBus callback WeakPtr.
  chromeos::DBusThreadManager::Get()->GetPrivetDaemonClient()->Ping(
      base::Bind(&ShellGcdPingFunction::OnPing, this));
  return RespondLater();
}

void ShellGcdPingFunction::OnPing(bool success) {
  Respond(OneArgument(new base::FundamentalValue(success)));
}

///////////////////////////////////////////////////////////////////////////////

ShellGcdGetWiFiBootstrapStateFunction::ShellGcdGetWiFiBootstrapStateFunction() {
}

ShellGcdGetWiFiBootstrapStateFunction::
    ~ShellGcdGetWiFiBootstrapStateFunction() {
}

ExtensionFunction::ResponseAction ShellGcdGetWiFiBootstrapStateFunction::Run() {
  std::string state = chromeos::DBusThreadManager::Get()
                          ->GetPrivetDaemonClient()
                          ->GetWifiBootstrapState();
  return RespondNow(OneArgument(new base::StringValue(state)));
}

}  // namespace extensions
