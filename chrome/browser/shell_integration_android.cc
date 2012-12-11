// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/shell_integration.h"

// TODO: crbug/115375 to track implementation for following methods.
// static
ShellIntegration::DefaultWebClientSetPermission
    ShellIntegration::CanSetAsDefaultBrowser() {
  NOTIMPLEMENTED();
  return SET_DEFAULT_NOT_ALLOWED;
}

// static
bool ShellIntegration::SetAsDefaultBrowser() {
  NOTIMPLEMENTED();
  return false;
}

// static
bool ShellIntegration::SetAsDefaultProtocolClient(const std::string& protocol) {
  NOTIMPLEMENTED();
  return false;
}

// static
ShellIntegration::DefaultWebClientState ShellIntegration::GetDefaultBrowser() {
  NOTIMPLEMENTED();
  return UNKNOWN_DEFAULT;
}

// static
ShellIntegration::DefaultWebClientState
ShellIntegration::IsDefaultProtocolClient(const std::string& protocol) {
  NOTIMPLEMENTED();
  return UNKNOWN_DEFAULT;
}

// static
bool ShellIntegration::IsFirefoxDefaultBrowser() {
  return false;
}

