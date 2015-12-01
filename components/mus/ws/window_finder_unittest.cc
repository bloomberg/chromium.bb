// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_finder.h"

#include "components/mus/ws/server_window.h"
#include "components/mus/ws/server_window_surface_manager.h"
#include "components/mus/ws/server_window_surface_manager_test_api.h"
#include "components/mus/ws/test_server_window_delegate.h"
#include "components/mus/ws/window_finder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mus {
namespace ws {

TEST(WindowFinderTest, FindDeepestVisibleWindow) {
  TestServerWindowDelegate window_delegate;
  ServerWindow root(&window_delegate, WindowId(1, 2));
  window_delegate.set_root_window(&root);
  root.SetVisible(true);
  root.SetBounds(gfx::Rect(0, 0, 100, 100));

  ServerWindow child1(&window_delegate, WindowId(1, 3));
  root.Add(&child1);
  EnableHitTest(&child1);
  child1.SetVisible(true);
  child1.SetBounds(gfx::Rect(10, 10, 20, 20));

  ServerWindow child2(&window_delegate, WindowId(1, 3));
  root.Add(&child2);
  EnableHitTest(&child2);
  child2.SetVisible(true);
  child2.SetBounds(gfx::Rect(15, 15, 20, 20));

  gfx::Point local_point(16, 16);
  EXPECT_EQ(&child2, FindDeepestVisibleWindowForEvents(&root, cc::SurfaceId(),
                                                       &local_point));
  EXPECT_EQ(gfx::Point(1, 1), local_point);

  local_point.SetPoint(13, 14);
  EXPECT_EQ(&child1, FindDeepestVisibleWindowForEvents(&root, cc::SurfaceId(),
                                                       &local_point));
  EXPECT_EQ(gfx::Point(3, 4), local_point);

  child2.set_extended_hit_test_region(gfx::Insets(10, 10, 10, 10));
  local_point.SetPoint(13, 14);
  EXPECT_EQ(&child2, FindDeepestVisibleWindowForEvents(&root, cc::SurfaceId(),
                                                       &local_point));
  EXPECT_EQ(gfx::Point(-2, -1), local_point);
}

}  // namespace ws
}  // namespace mus
