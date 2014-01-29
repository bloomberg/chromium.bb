// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/size.h"

namespace cc {
namespace {

TEST(SurfaceTest, SurfaceLifetime) {
  SurfaceManager manager;

  int surface_id = 0;
  {
    Surface surface(&manager, NULL, gfx::Size(5, 5));
    surface_id = surface.surface_id();
    EXPECT_GT(surface_id, 0);
    EXPECT_EQ(&surface, manager.GetSurfaceForID(surface_id));
  }

  EXPECT_EQ(NULL, manager.GetSurfaceForID(surface_id));
}

}  // namespace
}  // namespace cc
