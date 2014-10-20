// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/api/shell_identity/shell_identity_api.h"

#include "base/logging.h"

namespace extensions {

ShellIdentityGetAuthTokenFunction::ShellIdentityGetAuthTokenFunction() {
}

ShellIdentityGetAuthTokenFunction::~ShellIdentityGetAuthTokenFunction() {
}

ExtensionFunction::ResponseAction ShellIdentityGetAuthTokenFunction::Run() {
  // TODO(jamescook): Implement this.
  NOTIMPLEMENTED();
  return RespondNow(NoArguments());
}

}  // namespace extensions
