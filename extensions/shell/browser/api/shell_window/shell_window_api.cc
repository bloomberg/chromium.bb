// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/api/shell_window/shell_window_api.h"

#include "base/logging.h"
#include "extensions/shell/common/api/shell_window.h"

namespace shell_window = extensions::shell::api::shell_window;

namespace extensions {

ShellWindowRequestHideFunction::ShellWindowRequestHideFunction() {
}

ShellWindowRequestHideFunction::~ShellWindowRequestHideFunction() {
}

ExtensionFunction::ResponseAction ShellWindowRequestHideFunction::Run() {
  // TODO(zork): Implement this.
  return RespondNow(NoArguments());
}

ShellWindowRequestShowFunction::ShellWindowRequestShowFunction() {
}

ShellWindowRequestShowFunction::~ShellWindowRequestShowFunction() {
}

ExtensionFunction::ResponseAction ShellWindowRequestShowFunction::Run() {
  // TODO(zork): Implement this.
  return RespondNow(NoArguments());
}

ShellWindowShowAppFunction::ShellWindowShowAppFunction() {
}

ShellWindowShowAppFunction::~ShellWindowShowAppFunction() {
}

ExtensionFunction::ResponseAction ShellWindowShowAppFunction::Run() {
  // Get parameters
  scoped_ptr<shell_window::ShowApp::Params> params(
      shell_window::ShowApp::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // TODO(zork): Implement this.
  return RespondNow(NoArguments());
}

}  // namespace extensions
