// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/api/shell_gcd/shell_gcd_api.h"

#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/privet_daemon_client.h"
#include "extensions/shell/common/api/shell_gcd.h"

namespace gcd = extensions::shell::api::shell_gcd;

namespace extensions {

ShellGcdGetSetupStatusFunction::ShellGcdGetSetupStatusFunction() {
}

ShellGcdGetSetupStatusFunction::~ShellGcdGetSetupStatusFunction() {
}

ExtensionFunction::ResponseAction ShellGcdGetSetupStatusFunction::Run() {
  // |this| is refcounted so we don't need the usual DBus callback WeakPtr.
  chromeos::DBusThreadManager::Get()->GetPrivetDaemonClient()->GetSetupStatus(
      base::Bind(&ShellGcdGetSetupStatusFunction::OnSetupStatus, this));
  return RespondLater();
}

void ShellGcdGetSetupStatusFunction::OnSetupStatus(
    const std::string& status_string) {
  gcd::SetupStatus status = gcd::ParseSetupStatus(status_string);
  Respond(ArgumentList(gcd::GetSetupStatus::Results::Create(status)));
}

}  // namespace extensions
