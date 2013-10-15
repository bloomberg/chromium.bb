// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_LAUNCHER_ITEM_DELEGATE_MANAGER_TEST_API_H_
#define ASH_TEST_LAUNCHER_ITEM_DELEGATE_MANAGER_TEST_API_H_

#include "base/basictypes.h"

namespace ash {

class LauncherItemDelegateManager;

namespace test {

// Accesses private methods from a LauncherItemDelegateManager for testing.
class LauncherItemDelegateManagerTestAPI {
 public:
  explicit LauncherItemDelegateManagerTestAPI(
      LauncherItemDelegateManager* manager);

  // Clear all exsiting LauncherItemDelegate for test.
  void RemoveAllLauncherItemDelegateForTest();

 private:
  LauncherItemDelegateManager* manager_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(LauncherItemDelegateManagerTestAPI);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_LAUNCHER_ITEM_DELEGATE_MANAGER_TEST_API_H_
