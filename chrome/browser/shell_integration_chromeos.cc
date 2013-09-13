// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

// static
ShellIntegration::DefaultWebClientSetPermission
    ShellIntegration::CanSetAsDefaultBrowser() {
  return SET_DEFAULT_NOT_ALLOWED;
}

// static
bool ShellIntegration::SetAsDefaultBrowser() {
  return false;
}

// static
bool ShellIntegration::SetAsDefaultProtocolClient(const std::string& protocol) {
  return false;
}

// static
ShellIntegration::DefaultWebClientState ShellIntegration::GetDefaultBrowser() {
  return UNKNOWN_DEFAULT;
}

// static
ShellIntegration::DefaultWebClientState
ShellIntegration::IsDefaultProtocolClient(const std::string& protocol) {
  return UNKNOWN_DEFAULT;
}

// static
bool ShellIntegration::IsFirefoxDefaultBrowser() {
  return false;
}

