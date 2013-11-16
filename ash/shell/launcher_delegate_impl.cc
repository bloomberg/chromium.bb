// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/launcher_delegate_impl.h"

#include "ash/shell.h"
#include "ash/shell/toplevel_window.h"
#include "ash/shell/window_watcher.h"
#include "ash/wm/window_util.h"
#include "base/strings/string_util.h"
#include "grit/ash_resources.h"

namespace ash {
namespace shell {

LauncherDelegateImpl::LauncherDelegateImpl(WindowWatcher* watcher)
    : watcher_(watcher) {
}

LauncherDelegateImpl::~LauncherDelegateImpl() {
}

void LauncherDelegateImpl::OnLauncherCreated(Launcher* launcher) {
}

void LauncherDelegateImpl::OnLauncherDestroyed(Launcher* launcher) {
}

LauncherID LauncherDelegateImpl::GetLauncherIDForAppID(
    const std::string& app_id) {
  return 0;
}

const std::string& LauncherDelegateImpl::GetAppIDForLauncherID(LauncherID id) {
  return EmptyString();
}

void LauncherDelegateImpl::PinAppWithID(const std::string& app_id) {
}

bool LauncherDelegateImpl::IsAppPinned(const std::string& app_id) {
  return false;
}

bool LauncherDelegateImpl::CanPin() const {
  return false;
}

void LauncherDelegateImpl::UnpinAppWithID(const std::string& app_id) {
}

}  // namespace shell
}  // namespace ash
