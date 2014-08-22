// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_WM_TEST_WINDOW_MANAGER_IMPL_TEST_API_H_
#define ATHENA_WM_TEST_WINDOW_MANAGER_IMPL_TEST_API_H_

#include "base/macros.h"

namespace athena {
class SplitViewController;
class WindowListProvider;
class WindowManagerImpl;

namespace test {

class WindowManagerImplTestApi {
 public:
  WindowManagerImplTestApi();
  ~WindowManagerImplTestApi();

  athena::WindowListProvider* GetWindowListProvider();
  athena::SplitViewController* GetSplitViewController();

 private:
  athena::WindowManagerImpl* wm_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerImplTestApi);
};

}  // namespace test
}  // namespace athena

#endif  // ATHENA_WM_TEST_WINDOW_MANAGER_IMPL_TEST_API_H_
