// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/sub_surface.h"

#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo {
namespace {

using SubSurfaceTest = test::ExoTestBase;

TEST_F(SubSurfaceTest, SetPosition) {
  auto parent = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(parent.get());
  auto surface = std::make_unique<Surface>();
  auto sub_surface = std::make_unique<SubSurface>(surface.get(), parent.get());

  // Initial position is at the origin.
  EXPECT_EQ(gfx::Point().ToString(),
            surface->window()->bounds().origin().ToString());

  // Set position to 10, 10.
  gfx::Point position(10, 10);
  sub_surface->SetPosition(position);

  // A call to Commit() is required for position to take effect.
  EXPECT_EQ(gfx::Point().ToString(),
            surface->window()->bounds().origin().ToString());

  // Check that position is updated when Commit() is called.
  parent->Commit();
  EXPECT_EQ(position.ToString(),
            surface->window()->bounds().origin().ToString());

  // Create and commit a new sub-surface using the same surface.
  sub_surface.reset();
  sub_surface = std::make_unique<SubSurface>(surface.get(), parent.get());
  parent->Commit();

  // Initial position should be reset to origin.
  EXPECT_EQ(gfx::Point().ToString(),
            surface->window()->bounds().origin().ToString());
}

TEST_F(SubSurfaceTest, PlaceAbove) {
  auto parent = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(parent.get());
  auto surface1 = std::make_unique<Surface>();
  auto surface2 = std::make_unique<Surface>();
  auto non_sibling_surface = std::make_unique<Surface>();
  auto sub_surface1 =
      std::make_unique<SubSurface>(surface1.get(), parent.get());
  auto sub_surface2 =
      std::make_unique<SubSurface>(surface2.get(), parent.get());

  ASSERT_EQ(2u, parent->window()->children().size());
  EXPECT_EQ(surface1->window(), parent->window()->children()[0]);
  EXPECT_EQ(surface2->window(), parent->window()->children()[1]);

  sub_surface2->PlaceAbove(parent.get());
  sub_surface1->PlaceAbove(non_sibling_surface.get());  // Invalid
  sub_surface1->PlaceAbove(surface1.get());             // Invalid
  sub_surface1->PlaceAbove(surface2.get());

  // Nothing should have changed as Commit() is required for new stacking
  // order to take effect.
  EXPECT_EQ(surface1->window(), parent->window()->children()[0]);
  EXPECT_EQ(surface2->window(), parent->window()->children()[1]);

  parent->Commit();

  // surface1 should now be stacked above surface2.
  EXPECT_EQ(surface2->window(), parent->window()->children()[0]);
  EXPECT_EQ(surface1->window(), parent->window()->children()[1]);
}

TEST_F(SubSurfaceTest, PlaceBelow) {
  auto parent = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(parent.get());
  auto surface1 = std::make_unique<Surface>();
  auto surface2 = std::make_unique<Surface>();
  auto non_sibling_surface = std::make_unique<Surface>();
  auto sub_surface1 =
      std::make_unique<SubSurface>(surface1.get(), parent.get());
  auto sub_surface2 =
      std::make_unique<SubSurface>(surface2.get(), parent.get());

  ASSERT_EQ(2u, parent->window()->children().size());
  EXPECT_EQ(surface1->window(), parent->window()->children()[0]);
  EXPECT_EQ(surface2->window(), parent->window()->children()[1]);

  sub_surface2->PlaceBelow(parent.get());               // Invalid
  sub_surface2->PlaceBelow(non_sibling_surface.get());  // Invalid
  sub_surface1->PlaceBelow(surface2.get());
  sub_surface2->PlaceBelow(surface1.get());

  // Nothing should have changed as Commit() is required for new stacking
  // order to take effect.
  EXPECT_EQ(surface1->window(), parent->window()->children()[0]);
  EXPECT_EQ(surface2->window(), parent->window()->children()[1]);

  parent->Commit();

  // surface1 should now be stacked above surface2.
  EXPECT_EQ(surface2->window(), parent->window()->children()[0]);
  EXPECT_EQ(surface1->window(), parent->window()->children()[1]);
}

TEST_F(SubSurfaceTest, SetCommitBehavior) {
  auto parent = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(parent.get());
  auto child = std::make_unique<Surface>();
  auto grandchild = std::make_unique<Surface>();
  auto child_sub_surface =
      std::make_unique<SubSurface>(child.get(), parent.get());
  auto grandchild_sub_surface =
      std::make_unique<SubSurface>(grandchild.get(), child.get());

  // Initial position is at the origin.
  EXPECT_EQ(gfx::Point().ToString(),
            grandchild->window()->bounds().origin().ToString());

  // Set position to 10, 10.
  gfx::Point position1(10, 10);
  grandchild_sub_surface->SetPosition(position1);
  child->Commit();

  // Initial commit behavior is synchronous and the effect of the child
  // Commit() call will not take effect until Commit() is called on the
  // parent.
  EXPECT_EQ(gfx::Point().ToString(),
            grandchild->window()->bounds().origin().ToString());

  parent->Commit();

  // Position should have been updated when Commit() has been called on both
  // child and parent.
  EXPECT_EQ(position1.ToString(),
            grandchild->window()->bounds().origin().ToString());

  bool synchronized = false;
  child_sub_surface->SetCommitBehavior(synchronized);

  // Set position to 20, 20.
  gfx::Point position2(20, 20);
  grandchild_sub_surface->SetPosition(position2);
  child->Commit();

  // A Commit() call on child should be sufficient for the position of
  // grandchild to take effect when synchronous is disabled.
  EXPECT_EQ(position2.ToString(),
            grandchild->window()->bounds().origin().ToString());
}

}  // namespace
}  // namespace exo
