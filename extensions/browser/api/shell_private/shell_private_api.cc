// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/shell_private/shell_private_api.h"

namespace extensions {

ShellPrivatePrintHelloFunction::ShellPrivatePrintHelloFunction() {
}

ShellPrivatePrintHelloFunction::~ShellPrivatePrintHelloFunction() {
}

ExtensionFunction::ResponseAction ShellPrivatePrintHelloFunction::Run() {
  // Warning so it shows in Release and avoids presubmit check for INFO.
  LOG(WARNING) << "hello";
  return RespondNow(NoArguments());
}

}  // namespace extensions
