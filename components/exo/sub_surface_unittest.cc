// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/sub_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo {
namespace {

using SubSurfaceTest = test::ExoTestBase;

TEST_F(SubSurfaceTest, SetPosition) {
  scoped_ptr<Surface> parent(new Surface);
  scoped_ptr<Surface> surface(new Surface);
  scoped_ptr<SubSurface> sub_surface(
      new SubSurface(surface.get(), parent.get()));

  // Initial position is at the origin.
  EXPECT_EQ(gfx::Point().ToString(), surface->bounds().origin().ToString());

  // Set position to 10, 10.
  gfx::Point position(10, 10);
  sub_surface->SetPosition(position);

  // A call to Commit() is required for position to take effect.
  EXPECT_EQ(gfx::Point().ToString(), surface->bounds().origin().ToString());

  // Check that position is updated when Commit() is called.
  parent->Commit();
  EXPECT_EQ(position.ToString(), surface->bounds().origin().ToString());

  // Create and commit a new sub-surface using the same surface.
  sub_surface.reset();
  sub_surface = make_scoped_ptr(new SubSurface(surface.get(), parent.get()));
  parent->Commit();

  // Initial position should be reset to origin.
  EXPECT_EQ(gfx::Point().ToString(), surface->bounds().origin().ToString());
}

TEST_F(SubSurfaceTest, PlaceAbove) {
  scoped_ptr<Surface> parent(new Surface);
  scoped_ptr<Surface> surface1(new Surface);
  scoped_ptr<Surface> surface2(new Surface);
  scoped_ptr<Surface> non_sibling_surface(new Surface);
  scoped_ptr<SubSurface> sub_surface1(
      new SubSurface(surface1.get(), parent.get()));
  scoped_ptr<SubSurface> sub_surface2(
      new SubSurface(surface2.get(), parent.get()));

  ASSERT_EQ(2u, parent->children().size());
  EXPECT_EQ(surface1.get(), parent->children()[0]);
  EXPECT_EQ(surface2.get(), parent->children()[1]);

  sub_surface2->PlaceAbove(parent.get());
  sub_surface1->PlaceAbove(non_sibling_surface.get());  // Invalid
  sub_surface1->PlaceAbove(surface1.get());             // Invalid
  sub_surface1->PlaceAbove(surface2.get());

  // Nothing should have changed as Commit() is required for new stacking
  // order to take effect.
  EXPECT_EQ(surface1.get(), parent->children()[0]);
  EXPECT_EQ(surface2.get(), parent->children()[1]);

  parent->Commit();

  // surface1 should now be stacked above surface2.
  EXPECT_EQ(surface2.get(), parent->children()[0]);
  EXPECT_EQ(surface1.get(), parent->children()[1]);
}

TEST_F(SubSurfaceTest, PlaceBelow) {
  scoped_ptr<Surface> parent(new Surface);
  scoped_ptr<Surface> surface1(new Surface);
  scoped_ptr<Surface> surface2(new Surface);
  scoped_ptr<Surface> non_sibling_surface(new Surface);
  scoped_ptr<SubSurface> sub_surface1(
      new SubSurface(surface1.get(), parent.get()));
  scoped_ptr<SubSurface> sub_surface2(
      new SubSurface(surface2.get(), parent.get()));

  ASSERT_EQ(2u, parent->children().size());
  EXPECT_EQ(surface1.get(), parent->children()[0]);
  EXPECT_EQ(surface2.get(), parent->children()[1]);

  sub_surface2->PlaceBelow(parent.get());               // Invalid
  sub_surface2->PlaceBelow(non_sibling_surface.get());  // Invalid
  sub_surface1->PlaceBelow(surface2.get());
  sub_surface2->PlaceBelow(surface1.get());

  // Nothing should have changed as Commit() is required for new stacking
  // order to take effect.
  EXPECT_EQ(surface1.get(), parent->children()[0]);
  EXPECT_EQ(surface2.get(), parent->children()[1]);

  parent->Commit();

  // surface1 should now be stacked above surface2.
  EXPECT_EQ(surface2.get(), parent->children()[0]);
  EXPECT_EQ(surface1.get(), parent->children()[1]);
}

TEST_F(SubSurfaceTest, SetCommitBehavior) {
  scoped_ptr<Surface> parent(new Surface);
  scoped_ptr<Surface> child(new Surface);
  scoped_ptr<Surface> grandchild(new Surface);
  scoped_ptr<SubSurface> child_sub_surface(
      new SubSurface(child.get(), parent.get()));
  scoped_ptr<SubSurface> grandchild_sub_surface(
      new SubSurface(grandchild.get(), child.get()));

  // Initial position is at the origin.
  EXPECT_EQ(gfx::Point().ToString(), grandchild->bounds().origin().ToString());

  // Set position to 10, 10.
  gfx::Point position1(10, 10);
  grandchild_sub_surface->SetPosition(position1);
  child->Commit();

  // Initial commit behavior is synchronous and the effect of the child
  // Commit() call will not take effect until Commit() is called on the
  // parent.
  EXPECT_EQ(gfx::Point().ToString(), grandchild->bounds().origin().ToString());

  parent->Commit();

  // Position should have been updated when Commit() has been called on both
  // child and parent.
  EXPECT_EQ(position1.ToString(), grandchild->bounds().origin().ToString());

  // Disable synchronous commit behavior.
  bool synchronized = false;
  child_sub_surface->SetCommitBehavior(synchronized);

  // Set position to 20, 20.
  gfx::Point position2(20, 20);
  grandchild_sub_surface->SetPosition(position2);
  child->Commit();

  // A Commit() call on child should be sufficient for the position of
  // grandchild to take effect when synchronous is disabled.
  EXPECT_EQ(position2.ToString(), grandchild->bounds().origin().ToString());
}

}  // namespace
}  // namespace exo
