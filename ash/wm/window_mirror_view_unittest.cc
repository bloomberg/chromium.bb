// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_mirror_view.h"

#include "ash/test/ash_test_base.h"
#include "mojo/public/cpp/bindings/map.h"
#include "services/ws/window_tree_test_helper.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace wm {
namespace {

using WindowMirrorViewTest = AshTestBase;

// Regression test for blank Alt-Tab preview windows. https://crbug.com/921224
TEST_F(WindowMirrorViewTest, RemoteClientWindowHasNonEmptyMirror) {
  // Simulate the proxy window created by a remote client using the window
  // service API for a window without a kTopViewInset.
  auto properties = CreatePropertiesForProxyWindow(gfx::Rect(0, 0, 400, 300));
  std::unique_ptr<aura::Window> window(
      GetWindowTreeTestHelper()->NewTopLevelWindow(
          mojo::MapToFlatMap(properties)));
  window->SetProperty(aura::client::kTopViewInset, 0);

  // Verify that the mirror view has non-empty bounds.
  auto mirror_view = std::make_unique<WindowMirrorView>(
      window.get(), /*trilinear_filtering_on_init=*/false);
  EXPECT_FALSE(mirror_view->CalculatePreferredSize().IsEmpty());
}

}  // namespace
}  // namespace wm
}  // namespace ash
