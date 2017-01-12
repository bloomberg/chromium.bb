// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/memory/ptr_util.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

class AnonymousService : public ArcService {
 public:
  AnonymousService(ArcBridgeService* arc_bridge_service, bool* alive)
      : ArcService(arc_bridge_service), alive_(alive, true) {}
  ~AnonymousService() override = default;

 private:
  base::AutoReset<bool> alive_;
};

class NamedService : public ArcService {
 public:
  static const char kArcServiceName[];
  NamedService(ArcBridgeService* arc_bridge_service, bool* alive)
      : ArcService(arc_bridge_service), alive_(alive, true) {}
  ~NamedService() override = default;

 private:
  base::AutoReset<bool> alive_;
};

class DifferentNamedService : public ArcService {
 public:
  static const char kArcServiceName[];
  DifferentNamedService(ArcBridgeService* arc_bridge_service, bool* alive)
      : ArcService(arc_bridge_service), alive_(alive, true) {}
  ~DifferentNamedService() override = default;

 private:
  base::AutoReset<bool> alive_;
};

class EmptyNamedService : public ArcService {
 public:
  static const char kArcServiceName[];
  EmptyNamedService(ArcBridgeService* arc_bridge_service, bool* alive)
      : ArcService(arc_bridge_service), alive_(alive, true) {}
  ~EmptyNamedService() override = default;

 private:
  base::AutoReset<bool> alive_;
};

const char NamedService::kArcServiceName[] =
    "arc::(anonymous namespace)::NamedService";

const char DifferentNamedService::kArcServiceName[] =
    "arc::(anonymous namespace)::DifferentNamedService";

const char EmptyNamedService::kArcServiceName[] = "";

}  // namespace

class ArcServiceManagerTest : public testing::Test {
 public:
  ArcServiceManagerTest() = default;

  void SetUp() override {
    arc_bridge_service_ = base::MakeUnique<ArcBridgeService>();
  }

  void TearDown() override { arc_bridge_service_.reset(); }

  ArcBridgeService* arc_bridge_service() { return arc_bridge_service_.get(); }

 private:
  std::unique_ptr<ArcBridgeService> arc_bridge_service_;

  DISALLOW_COPY_AND_ASSIGN(ArcServiceManagerTest);
};

// Exercises the basic getter functionality of ArcServiceManager.
TEST_F(ArcServiceManagerTest, BasicGetter) {
  bool named_service_alive = false;

  // ArcServiceManager is empty, GetService() should return nullptr.
  auto manager = base::MakeUnique<ArcServiceManager>(nullptr);
  EXPECT_EQ(nullptr, manager->GetService<NamedService>());

  EXPECT_TRUE(manager->AddService(base::MakeUnique<NamedService>(
      arc_bridge_service(), &named_service_alive)));
  EXPECT_TRUE(named_service_alive);
  EXPECT_NE(nullptr, manager->GetService<NamedService>());

  // Finally, the service should not be alive anymore.
  manager.reset();
  EXPECT_FALSE(named_service_alive);
}

// There is no way to distinguish between anonymous services, so it should be
// possible to add them twice (not that it's recommended).
TEST_F(ArcServiceManagerTest, MultipleAnonymousServices) {
  bool anonymous_service_alive = false;
  bool second_anonymous_service_alive = false;

  auto manager = base::MakeUnique<ArcServiceManager>(nullptr);

  EXPECT_TRUE(manager->AddService(base::MakeUnique<AnonymousService>(
      arc_bridge_service(), &anonymous_service_alive)));
  EXPECT_TRUE(anonymous_service_alive);
  EXPECT_TRUE(manager->AddService(base::MakeUnique<AnonymousService>(
      arc_bridge_service(), &second_anonymous_service_alive)));
  EXPECT_TRUE(second_anonymous_service_alive);

  // Finally, the individual services should not be alive anymore.
  manager.reset();
  EXPECT_FALSE(anonymous_service_alive);
  EXPECT_FALSE(second_anonymous_service_alive);
}

// Named services can only be added once, but can still be retrieved.
TEST_F(ArcServiceManagerTest, MultipleNamedServices) {
  bool named_service_alive = false;
  bool second_named_service_alive = false;
  bool different_named_service_alive = false;

  auto manager = base::MakeUnique<ArcServiceManager>(nullptr);

  auto named_service = base::MakeUnique<NamedService>(arc_bridge_service(),
                                                      &named_service_alive);
  NamedService* raw_named_service = named_service.get();
  EXPECT_TRUE(named_service_alive);
  EXPECT_TRUE(manager->AddService(std::move(named_service)));
  auto second_named_service = base::MakeUnique<NamedService>(
      arc_bridge_service(), &second_named_service_alive);
  EXPECT_TRUE(second_named_service_alive);
  // This will fail and immediately destroy the service.
  EXPECT_FALSE(manager->AddService(std::move(second_named_service)));
  EXPECT_FALSE(second_named_service_alive);

  // We should still be able to add a different-named service.
  auto different_named_service = base::MakeUnique<DifferentNamedService>(
      arc_bridge_service(), &different_named_service_alive);
  DifferentNamedService* raw_different_named_service =
      different_named_service.get();
  EXPECT_TRUE(different_named_service_alive);
  EXPECT_TRUE(manager->AddService(std::move(different_named_service)));

  // And find both.
  EXPECT_EQ(raw_named_service, manager->GetService<NamedService>());
  EXPECT_EQ(raw_different_named_service,
            manager->GetService<DifferentNamedService>());

  manager.reset();
  EXPECT_FALSE(named_service_alive);
  EXPECT_FALSE(different_named_service_alive);
}

// Named services with an empty name are treated as anonymous services.
// Developers shouldn't do that, though, and will trigger an error log.
TEST_F(ArcServiceManagerTest, EmptyNamedServices) {
  bool empty_named_service_alive = false;

  auto manager = base::MakeUnique<ArcServiceManager>(nullptr);

  EXPECT_TRUE(manager->AddService(base::MakeUnique<EmptyNamedService>(
      arc_bridge_service(), &empty_named_service_alive)));
  EXPECT_TRUE(empty_named_service_alive);
  EXPECT_EQ(nullptr, manager->GetService<EmptyNamedService>());

  manager.reset();
  EXPECT_FALSE(empty_named_service_alive);
}

// Tests if GetGlobalService works as expected regardless of whether the
// global pointer is null.
TEST_F(ArcServiceManagerTest, TestGetGlobalService) {
  // The getter should return nullptr when no global instance is available.
  EXPECT_EQ(nullptr, ArcServiceManager::GetGlobalService<NamedService>());

  // Create a manager. This will automatically be registered as a global
  // instance.
  auto manager = base::MakeUnique<ArcServiceManager>(nullptr);

  // The getter should return nullptr when the manager doesn't know about the
  // NamedService instance.
  EXPECT_EQ(nullptr, ArcServiceManager::GetGlobalService<NamedService>());

  // Register the instance and retry. The getter should return non-null this
  // time.
  bool unused;
  EXPECT_TRUE(manager->AddService(base::MakeUnique<NamedService>(
      arc_bridge_service(), &unused)));
  EXPECT_NE(nullptr, ArcServiceManager::GetGlobalService<NamedService>());

  // Remove the global instance. The same GetGlobalService() call should return
  // nullptr now.
  manager.reset();
  EXPECT_EQ(nullptr, ArcServiceManager::GetGlobalService<NamedService>());
}

}  // namespace arc
