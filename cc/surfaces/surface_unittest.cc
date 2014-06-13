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

  SurfaceId surface_id;
  {
    Surface surface(&manager, NULL, gfx::Size(5, 5));
    surface_id = surface.surface_id();
    EXPECT_TRUE(!surface_id.is_null());
    EXPECT_EQ(&surface, manager.GetSurfaceForId(surface_id));
  }

  EXPECT_EQ(NULL, manager.GetSurfaceForId(surface_id));
}

}  // namespace
}  // namespace cc
