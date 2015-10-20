// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/test/scheduler_test_common.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
namespace {

class FakeSurfaceFactoryClient : public SurfaceFactoryClient {
 public:
  FakeSurfaceFactoryClient() : begin_frame_source_(nullptr) {}

  void ReturnResources(const ReturnedResourceArray& resources) override {}

  void SetBeginFrameSource(SurfaceId surface_id,
                           BeginFrameSource* begin_frame_source) override {
    begin_frame_source_ = begin_frame_source;
  }

  BeginFrameSource* begin_frame_source() { return begin_frame_source_; }

 private:
  BeginFrameSource* begin_frame_source_;
};

TEST(SurfaceTest, SurfaceLifetime) {
  SurfaceManager manager;
  FakeSurfaceFactoryClient surface_factory_client;
  SurfaceFactory factory(&manager, &surface_factory_client);

  SurfaceId surface_id(6);
  {
    factory.Create(surface_id);
    EXPECT_TRUE(manager.GetSurfaceForId(surface_id));
    factory.Destroy(surface_id);
  }

  EXPECT_EQ(NULL, manager.GetSurfaceForId(surface_id));
}

TEST(SurfaceTest, StableBeginFrameSourceIndependentOfOrderAdded) {
  SurfaceManager manager;
  FakeSurfaceFactoryClient surface_factory_client;
  SurfaceFactory factory(&manager, &surface_factory_client);

  SurfaceId surface_id(6);
  factory.Create(surface_id);
  Surface* surface = manager.GetSurfaceForId(surface_id);

  FakeBeginFrameSource bfs1;
  FakeBeginFrameSource bfs2;
  FakeBeginFrameSource bfs3;

  // Order 1.
  surface->AddBeginFrameSource(&bfs1);
  surface->AddBeginFrameSource(&bfs2);
  surface->AddBeginFrameSource(&bfs3);
  BeginFrameSource* bfs_order1 = surface_factory_client.begin_frame_source();
  // Make sure one of the provided sources was chosen.
  EXPECT_TRUE(&bfs1 == bfs_order1 || &bfs2 == bfs_order1 ||
              &bfs3 == bfs_order1);
  surface->RemoveBeginFrameSource(&bfs1);
  surface->RemoveBeginFrameSource(&bfs2);
  surface->RemoveBeginFrameSource(&bfs3);
  EXPECT_EQ(nullptr, surface_factory_client.begin_frame_source());

  // Order 2.
  surface->AddBeginFrameSource(&bfs1);
  surface->AddBeginFrameSource(&bfs3);
  surface->AddBeginFrameSource(&bfs2);
  BeginFrameSource* bfs_order2 = surface_factory_client.begin_frame_source();
  // Verify choice is same as before.
  EXPECT_EQ(bfs_order1, bfs_order2);
  surface->RemoveBeginFrameSource(&bfs1);
  surface->RemoveBeginFrameSource(&bfs2);
  surface->RemoveBeginFrameSource(&bfs3);
  EXPECT_EQ(nullptr, surface_factory_client.begin_frame_source());

  // Order 3.
  surface->AddBeginFrameSource(&bfs2);
  surface->AddBeginFrameSource(&bfs1);
  surface->AddBeginFrameSource(&bfs3);
  BeginFrameSource* bfs_order3 = surface_factory_client.begin_frame_source();
  // Verify choice is same as before.
  EXPECT_EQ(bfs_order2, bfs_order3);
  surface->RemoveBeginFrameSource(&bfs1);
  surface->RemoveBeginFrameSource(&bfs2);
  surface->RemoveBeginFrameSource(&bfs3);
  EXPECT_EQ(nullptr, surface_factory_client.begin_frame_source());

  // Order 4.
  surface->AddBeginFrameSource(&bfs2);
  surface->AddBeginFrameSource(&bfs3);
  surface->AddBeginFrameSource(&bfs1);
  BeginFrameSource* bfs_order4 = surface_factory_client.begin_frame_source();
  // Verify choice is same as before.
  EXPECT_EQ(bfs_order3, bfs_order4);
  surface->RemoveBeginFrameSource(&bfs1);
  surface->RemoveBeginFrameSource(&bfs2);
  surface->RemoveBeginFrameSource(&bfs3);
  EXPECT_EQ(nullptr, surface_factory_client.begin_frame_source());

  // Order 5.
  surface->AddBeginFrameSource(&bfs3);
  surface->AddBeginFrameSource(&bfs1);
  surface->AddBeginFrameSource(&bfs2);
  BeginFrameSource* bfs_order5 = surface_factory_client.begin_frame_source();
  // Verify choice is same as before.
  EXPECT_EQ(bfs_order4, bfs_order5);
  surface->RemoveBeginFrameSource(&bfs1);
  surface->RemoveBeginFrameSource(&bfs2);
  surface->RemoveBeginFrameSource(&bfs3);
  EXPECT_EQ(nullptr, surface_factory_client.begin_frame_source());

  // Order 6.
  surface->AddBeginFrameSource(&bfs3);
  surface->AddBeginFrameSource(&bfs2);
  surface->AddBeginFrameSource(&bfs1);
  BeginFrameSource* bfs_order6 = surface_factory_client.begin_frame_source();
  // Verify choice is same as before.
  EXPECT_EQ(bfs_order5, bfs_order6);
  surface->RemoveBeginFrameSource(&bfs1);
  surface->RemoveBeginFrameSource(&bfs2);
  surface->RemoveBeginFrameSource(&bfs3);
  EXPECT_EQ(nullptr, surface_factory_client.begin_frame_source());
}

TEST(SurfaceTest, BeginFrameSourceRemovedOnSurfaceDestruction) {
  SurfaceManager manager;
  FakeSurfaceFactoryClient surface_factory_client;
  SurfaceFactory factory(&manager, &surface_factory_client);
  FakeBeginFrameSource bfs;

  SurfaceId surface_id(6);
  factory.Create(surface_id);
  Surface* surface = manager.GetSurfaceForId(surface_id);
  surface->AddBeginFrameSource(&bfs);

  BeginFrameSource* bfs_before = surface_factory_client.begin_frame_source();
  factory.Destroy(surface_id);
  BeginFrameSource* bfs_after = surface_factory_client.begin_frame_source();

  EXPECT_EQ(&bfs, bfs_before);
  EXPECT_EQ(nullptr, bfs_after);
}

}  // namespace
}  // namespace cc
