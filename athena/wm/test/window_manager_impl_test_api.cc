// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/wm/test/window_manager_impl_test_api.h"

#include "athena/wm/window_manager_impl.h"

namespace athena {
namespace test {

WindowManagerImplTestApi::WindowManagerImplTestApi()
    : wm_(static_cast<WindowManagerImpl*>(WindowManager::GetInstance())) {
}

WindowManagerImplTestApi::~WindowManagerImplTestApi() {
}

athena::WindowListProvider* WindowManagerImplTestApi::GetWindowListProvider() {
  return wm_->window_list_provider_.get();
}

athena::SplitViewController*
WindowManagerImplTestApi::GetSplitViewController() {
  return wm_->split_view_controller_.get();
}

}  // namespace test
}  // namespace athena
