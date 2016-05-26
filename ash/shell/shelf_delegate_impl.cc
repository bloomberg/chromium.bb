// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/shelf_delegate_impl.h"

#include "base/strings/string_util.h"

namespace ash {
namespace shell {

ShelfDelegateImpl::ShelfDelegateImpl() {}

ShelfDelegateImpl::~ShelfDelegateImpl() {}

void ShelfDelegateImpl::OnShelfCreated(Shelf* shelf) {}

void ShelfDelegateImpl::OnShelfDestroyed(Shelf* shelf) {}

void ShelfDelegateImpl::OnShelfAlignmentChanged(Shelf* shelf) {}

void ShelfDelegateImpl::OnShelfAutoHideBehaviorChanged(Shelf* shelf) {}

void ShelfDelegateImpl::OnShelfAutoHideStateChanged(Shelf* shelf) {}

void ShelfDelegateImpl::OnShelfVisibilityStateChanged(Shelf* shelf) {}

ShelfID ShelfDelegateImpl::GetShelfIDForAppID(const std::string& app_id) {
  return 0;
}

bool ShelfDelegateImpl::HasShelfIDToAppIDMapping(ShelfID id) const {
  return false;
}

const std::string& ShelfDelegateImpl::GetAppIDForShelfID(ShelfID id) {
  return base::EmptyString();
}

void ShelfDelegateImpl::PinAppWithID(const std::string& app_id) {}

bool ShelfDelegateImpl::IsAppPinned(const std::string& app_id) {
  return false;
}

void ShelfDelegateImpl::UnpinAppWithID(const std::string& app_id) {}

}  // namespace shell
}  // namespace ash
