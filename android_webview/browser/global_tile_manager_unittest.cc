// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include "android_webview/browser/global_tile_manager.h"
#include "android_webview/browser/global_tile_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// This should be the same as the one defined global_tile_manager.cc
const size_t kNumTilesLimit = 450;
const size_t kDefaultNumTiles = 150;

}  // namespace

using android_webview::GlobalTileManager;
using android_webview::GlobalTileManagerClient;
using content::SynchronousCompositorMemoryPolicy;
using testing::Test;

class MockGlobalTileManagerClient : public GlobalTileManagerClient {
 public:
  virtual SynchronousCompositorMemoryPolicy GetMemoryPolicy() const OVERRIDE {
    return memory_policy_;
  }

  virtual void SetMemoryPolicy(SynchronousCompositorMemoryPolicy new_policy,
                               bool effective_immediately) OVERRIDE {
    memory_policy_ = new_policy;
  }

  MockGlobalTileManagerClient() {
    tile_request_.num_resources_limit = kDefaultNumTiles;
    key_ = GlobalTileManager::GetInstance()->PushBack(this);
  }

  virtual ~MockGlobalTileManagerClient() {
    GlobalTileManager::GetInstance()->Remove(key_);
  }

  SynchronousCompositorMemoryPolicy GetTileRequest() { return tile_request_; }
  GlobalTileManager::Key GetKey() { return key_; }

 private:
  SynchronousCompositorMemoryPolicy memory_policy_;
  SynchronousCompositorMemoryPolicy tile_request_;
  GlobalTileManager::Key key_;
};

class GlobalTileManagerTest : public Test {
 public:
  virtual void SetUp() {}

  GlobalTileManager* manager() { return GlobalTileManager::GetInstance(); }
};

TEST_F(GlobalTileManagerTest, RequestTilesUnderLimit) {
  MockGlobalTileManagerClient clients[2];

  for (size_t i = 0; i < 2; i++) {
    manager()->RequestTiles(clients[i].GetTileRequest(), clients[i].GetKey());
    manager()->DidUse(clients[i].GetKey());

    // Ensure clients get what they asked for when the manager is under tile
    // limit.
    EXPECT_EQ(clients[i].GetMemoryPolicy().num_resources_limit,
              kDefaultNumTiles);
  }
}

TEST_F(GlobalTileManagerTest, EvictHappensWhenOverLimit) {
  MockGlobalTileManagerClient clients[4];

  for (size_t i = 0; i < 4; i++) {
    manager()->RequestTiles(clients[i].GetTileRequest(), clients[i].GetKey());
    manager()->DidUse(clients[i].GetKey());
  }

  size_t total_tiles = 0;
  for (size_t i = 0; i < 4; i++) {
    total_tiles += clients[i].GetMemoryPolicy().num_resources_limit;
  }

  // Ensure that eviction happened and kept the total number of tiles within
  // kNumTilesLimit.
  EXPECT_LE(total_tiles, kNumTilesLimit);
}

TEST_F(GlobalTileManagerTest, RandomizedStressRequests) {
  MockGlobalTileManagerClient clients[100];
  size_t index[100];
  for (size_t i = 0; i < 100; i++) {
    index[i] = i;
  }

  // Fix the seed so that tests are reproducible.
  std::srand(1);
  // Simulate a random request order of clients.
  std::random_shuffle(&index[0], &index[99]);

  for (size_t i = 0; i < 100; i++) {
    size_t j = index[i];
    manager()->RequestTiles(clients[j].GetTileRequest(), clients[j].GetKey());
    manager()->DidUse(clients[j].GetKey());
  }

  size_t total_tiles = 0;
  for (size_t i = 0; i < 100; i++) {
    total_tiles += clients[i].GetMemoryPolicy().num_resources_limit;
  }

  // Ensure that eviction happened and kept the total number of tiles within
  // kNumTilesLimit.
  EXPECT_LE(total_tiles, kNumTilesLimit);
}

TEST_F(GlobalTileManagerTest, FixedOrderedRequests) {
  MockGlobalTileManagerClient clients[10];

  // 10 clients requesting resources in a fixed order. Do that for 5 rounds.
  for (int round = 0; round < 5; round++) {
    for (size_t i = 0; i < 10; i++) {
      manager()->RequestTiles(clients[i].GetTileRequest(), clients[i].GetKey());
      manager()->DidUse(clients[i].GetKey());
    }
  }

  // Ensure that the total tiles are divided evenly among all clients.
  for (size_t i = 0; i < 10; i++) {
    EXPECT_EQ(clients[i].GetMemoryPolicy().num_resources_limit,
              kNumTilesLimit / 10);
  }
}

TEST_F(GlobalTileManagerTest, FixedOrderedRequestsWithInactiveClients) {
  MockGlobalTileManagerClient clients[20];

  // 20 clients request resources.
  for (size_t i = 0; i < 20; i++) {
    manager()->RequestTiles(clients[i].GetTileRequest(), clients[i].GetKey());
    manager()->DidUse(clients[i].GetKey());
  }

  // Now the last 10 clients become inactive. Only the first 10 clients remain
  // active resource requesters.
  // 10 clients requesting resources in a fixed order. Do that for 5 rounds.
  for (int round = 0; round < 5; round++) {
    for (size_t i = 0; i < 10; i++) {
      manager()->RequestTiles(clients[i].GetTileRequest(), clients[i].GetKey());
      manager()->DidUse(clients[i].GetKey());
    }
  }

  // Ensure that the total tiles are divided evenly among all clients.
  for (size_t i = 0; i < 10; i++) {
    EXPECT_EQ(clients[i].GetMemoryPolicy().num_resources_limit,
              kNumTilesLimit / 10);
  }

  // Ensure that the inactive tiles are evicted.
  for (size_t i = 11; i < 20; i++) {
    EXPECT_EQ(clients[i].GetMemoryPolicy().num_resources_limit, 0u);
  }
}
