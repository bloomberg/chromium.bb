// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/api/shell_gcd/shell_gcd_api.h"

#include "base/values.h"
#include "extensions/shell/common/api/shell_gcd.h"

namespace shell_gcd = extensions::shell::api::shell_gcd;

namespace extensions {

ShellGcdGetSetupStatusFunction::ShellGcdGetSetupStatusFunction() {
}

ShellGcdGetSetupStatusFunction::~ShellGcdGetSetupStatusFunction() {
}

ExtensionFunction::ResponseAction ShellGcdGetSetupStatusFunction::Run() {
  // TODO(jamescook): Implement this.
  shell_gcd::SetupStatus status = shell_gcd::SETUP_STATUS_COMPLETED;
  return RespondNow(
      ArgumentList(shell_gcd::GetSetupStatus::Results::Create(status)));
}

}  // namespace extensions
