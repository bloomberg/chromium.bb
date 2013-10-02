// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/launcher_item_delegate_manager_test_api.h"

#include "ash/launcher/launcher_item_delegate.h"
#include "ash/launcher/launcher_item_delegate_manager.h"
#include "base/stl_util.h"

namespace ash {
namespace test {

LauncherItemDelegateManagerTestAPI::LauncherItemDelegateManagerTestAPI(
    LauncherItemDelegateManager* manager)
    : manager_(manager) {
  DCHECK(manager_);
}

void
LauncherItemDelegateManagerTestAPI::RemoveAllLauncherItemDelegateForTest() {
  STLDeleteContainerPairSecondPointers(
      manager_->id_to_item_delegate_map_.begin(),
      manager_->id_to_item_delegate_map_.end());
  manager_->id_to_item_delegate_map_.clear();
}

}  // namespace test
}  // namespace ash
