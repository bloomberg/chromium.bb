// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/shelf_delegate_impl.h"

#include "ash/shell.h"
#include "ash/shell/toplevel_window.h"
#include "ash/shell/window_watcher.h"
#include "ash/wm/window_util.h"
#include "base/strings/string_util.h"
#include "grit/ash_resources.h"

namespace ash {
namespace shell {

ShelfDelegateImpl::ShelfDelegateImpl(WindowWatcher* watcher)
    : watcher_(watcher) {
}

ShelfDelegateImpl::~ShelfDelegateImpl() {
}

void ShelfDelegateImpl::OnShelfCreated(Shelf* shelf) {
}

void ShelfDelegateImpl::OnShelfDestroyed(Shelf* shelf) {
}

ShelfID ShelfDelegateImpl::GetShelfIDForAppID(const std::string& app_id) {
  return 0;
}

const std::string& ShelfDelegateImpl::GetAppIDForShelfID(ShelfID id) {
  return base::EmptyString();
}

void ShelfDelegateImpl::PinAppWithID(const std::string& app_id) {
}

bool ShelfDelegateImpl::IsAppPinned(const std::string& app_id) {
  return false;
}

bool ShelfDelegateImpl::CanPin() const {
  return false;
}

void ShelfDelegateImpl::UnpinAppWithID(const std::string& app_id) {
}

}  // namespace shell
}  // namespace ash
