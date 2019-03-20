// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_mirror_view.h"

#include "ash/test/ash_test_base.h"
#include "mojo/public/cpp/bindings/map.h"
#include "services/ws/client_root.h"
#include "services/ws/client_root_test_helper.h"
#include "services/ws/proxy_window.h"
#include "services/ws/window_tree_test_helper.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

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

TEST_F(WindowMirrorViewTest, RemoteClientForcedVisible) {
  auto properties = CreatePropertiesForProxyWindow(gfx::Rect(0, 0, 400, 300));
  std::unique_ptr<aura::Window> window(
      GetWindowTreeTestHelper()->NewTopLevelWindow(
          mojo::MapToFlatMap(properties)));

  ws::ClientRootTestHelper client_root_test_helper(
      ws::ProxyWindow::GetMayBeNull(window.get())
          ->owning_window_tree()
          ->GetClientRootForWindow(window.get()));
  EXPECT_FALSE(client_root_test_helper.IsWindowForcedVisible());
  // Assertions only make sense if window occlusion tracker is running, which it
  // should be at this point.
  EXPECT_FALSE(window->env()->GetWindowOcclusionTracker()->IsPaused());
  auto widget = CreateTestWidget();
  widget->Hide();
  auto mirror_view = std::make_unique<WindowMirrorView>(
      window.get(), /*trilinear_filtering_on_init=*/false);
  widget->widget_delegate()->GetContentsView()->AddChildView(mirror_view.get());
  // Even though the widget is hidden, the remote client should think it's
  // visible.
  EXPECT_TRUE(client_root_test_helper.IsWindowForcedVisible());
}

TEST_F(WindowMirrorViewTest, RemoteClientForcedVisibleWhenRunning) {
  auto properties = CreatePropertiesForProxyWindow(gfx::Rect(0, 0, 400, 300));
  std::unique_ptr<aura::Window> window(
      GetWindowTreeTestHelper()->NewTopLevelWindow(
          mojo::MapToFlatMap(properties)));

  ws::ClientRootTestHelper client_root_test_helper(
      ws::ProxyWindow::GetMayBeNull(window.get())
          ->owning_window_tree()
          ->GetClientRootForWindow(window.get()));

  auto pause_occlusion_tracker =
      std::make_unique<aura::WindowOcclusionTracker::ScopedPause>(
          window->env());
  auto widget = CreateTestWidget();
  widget->Hide();
  auto mirror_view = std::make_unique<WindowMirrorView>(
      window.get(), /*trilinear_filtering_on_init=*/false);
  widget->widget_delegate()->GetContentsView()->AddChildView(mirror_view.get());

  // As occlusion tracking is paused, the remote window should remain hidden.
  EXPECT_FALSE(client_root_test_helper.IsWindowForcedVisible());

  // When occlusion tracking is enabled, the remote window shoul be visible.
  pause_occlusion_tracker.reset();
  EXPECT_TRUE(client_root_test_helper.IsWindowForcedVisible());
}

TEST_F(WindowMirrorViewTest, LocalWindowOcclusionMadeVisible) {
  auto widget = CreateTestWidget();
  widget->Hide();
  aura::Window* widget_window = widget->GetNativeWindow();
  widget_window->TrackOcclusionState();
  EXPECT_EQ(aura::Window::OcclusionState::HIDDEN,
            widget_window->occlusion_state());

  auto mirror_widget = CreateTestWidget();
  auto mirror_view = std::make_unique<WindowMirrorView>(
      widget_window, /*trilinear_filtering_on_init=*/false);
  mirror_widget->widget_delegate()->GetContentsView()->AddChildView(
      mirror_view.get());

  // Even though the widget is hidden, the occlusion state is considered
  // visible. This is to ensure renderers still produce content.
  EXPECT_EQ(aura::Window::OcclusionState::VISIBLE,
            widget_window->occlusion_state());
}

}  // namespace
}  // namespace wm
}  // namespace ash
