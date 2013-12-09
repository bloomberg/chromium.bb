// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_LAUNCHER_TEST_API_H_
#define ASH_TEST_LAUNCHER_TEST_API_H_

#include "base/basictypes.h"

namespace ash {

class Launcher;
class ShelfDelegate;

namespace internal {
class ShelfView;
}

namespace test {

// Use the api in this class to access private members of Launcher.
class LauncherTestAPI {
 public:
  explicit LauncherTestAPI(Launcher* launcher);

  ~LauncherTestAPI();

  // An accessor for |shelf_view|.
  internal::ShelfView* shelf_view();

  // Set ShelfDelegate.
  void SetShelfDelegate(ShelfDelegate* delegate);

 private:
  Launcher* launcher_;

  DISALLOW_COPY_AND_ASSIGN(LauncherTestAPI);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_LAUNCHER_TEST_API_H_
