// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_TEST_WM_SHELL_TEST_API_H_
#define ASH_COMMON_TEST_WM_SHELL_TEST_API_H_

#include <memory>

#include "base/macros.h"

namespace ash {

class SystemTrayDelegate;

// Test API to access the internal state of the singleton WmShell.
class WmShellTestApi {
 public:
  WmShellTestApi();
  ~WmShellTestApi();

  void SetSystemTrayDelegate(std::unique_ptr<SystemTrayDelegate> delegate);

 private:
  DISALLOW_COPY_AND_ASSIGN(WmShellTestApi);
};

}  // namespace ash

#endif  // ASH_COMMON_TEST_WM_SHELL_TEST_API_H_
