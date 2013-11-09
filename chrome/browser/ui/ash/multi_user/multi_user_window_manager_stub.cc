// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_stub.h"

#include "base/logging.h"
#include "base/strings/string_util.h"

namespace chrome {

void MultiUserWindowManagerStub::SetWindowOwner(aura::Window* window,
                                               const std::string& user_id) {
  NOTIMPLEMENTED();
}

const std::string& MultiUserWindowManagerStub::GetWindowOwner(
    aura::Window* window) {
  return EmptyString();
}

void MultiUserWindowManagerStub::ShowWindowForUser(aura::Window* window,
                                                  const std::string& user_id) {
  NOTIMPLEMENTED();
}

bool MultiUserWindowManagerStub::AreWindowsSharedAmongUsers() {
  return false;
}

bool MultiUserWindowManagerStub::IsWindowOnDesktopOfUser(
    aura::Window* window,
    const std::string& user_id) {
  return true;
}

const std::string& MultiUserWindowManagerStub::GetUserPresentingWindow(
    aura::Window* window) {
  return EmptyString();
}

void MultiUserWindowManagerStub::AddUser(Profile* profile) {
  NOTIMPLEMENTED();
}

}  // namespace chrome
