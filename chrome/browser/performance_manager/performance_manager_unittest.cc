// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/performance_manager.h"

#include <utility>

#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/performance_manager/frame_resource_coordinator.h"
#include "chrome/browser/performance_manager/page_resource_coordinator.h"
#include "chrome/browser/performance_manager/process_resource_coordinator.h"
#include "chrome/browser/performance_manager/system_resource_coordinator.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_id.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"
#include "services/resource_coordinator/public/mojom/coordination_unit_provider.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

class PerformanceManagerTest : public testing::Test {
 public:
  PerformanceManagerTest() {}

  ~PerformanceManagerTest() override {}

  void SetUp() override {
    EXPECT_EQ(nullptr, PerformanceManager::GetInstance());
    performance_manager_ = PerformanceManager::Create();
    // Make sure creation registers the created instance.
    EXPECT_EQ(performance_manager_.get(), PerformanceManager::GetInstance());
  }

  void TearDown() override {
    if (performance_manager_) {
      PerformanceManager::Destroy(std::move(performance_manager_));
      // Make sure destruction unregisters the instance.
      EXPECT_EQ(nullptr, PerformanceManager::GetInstance());
    }

    task_environment_.RunUntilIdle();
  }

  // Given a CU, tests that it works by invoking GetID and waiting for the
  // response. This test will hang (and eventually fail) if the response does
  // not come back from the remote endpoint.
  template <typename CoordinationUnitPtrType>
  void TestCUImpl(CoordinationUnitPtrType cu) {
    base::RunLoop loop;
    cu->GetID(base::BindLambdaForTesting(
        [&loop](const resource_coordinator::CoordinationUnitID& cu_id) {
          loop.Quit();
        }));
    loop.Run();
  }

  // Variant that works with mojo interface pointers.
  template <typename CoordinationUnitPtrType>
  void TestCU(CoordinationUnitPtrType& cu) {
    TestCUImpl<CoordinationUnitPtrType&>(cu);
  }

  // Variant that works with pointers to FooResourceCoordinator wrappers.
  template <typename CoordinationUnitPtrType>
  void TestCU(CoordinationUnitPtrType* cu) {
    TestCUImpl<CoordinationUnitPtrType*>(cu);
  }

 protected:
  PerformanceManager* performance_manager() {
    return performance_manager_.get();
  }

 private:
  std::unique_ptr<PerformanceManager> performance_manager_;
  base::test::ScopedTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(PerformanceManagerTest);
};

TEST_F(PerformanceManagerTest, ResourceCoordinatorInstantiate) {
  // Get the CU provider interface.
  resource_coordinator::mojom::CoordinationUnitProviderPtr provider;
  performance_manager()->BindInterface(mojo::MakeRequest(&provider));

  // Create and test a dummy FrameCU.
  resource_coordinator::CoordinationUnitID frame_id(
      resource_coordinator::CoordinationUnitType::kFrame,
      resource_coordinator::CoordinationUnitID::RANDOM_ID);
  resource_coordinator::mojom::FrameCoordinationUnitPtr frame_cu;
  provider->CreateFrameCoordinationUnit(mojo::MakeRequest(&frame_cu), frame_id);
  TestCU(frame_cu);

  // Create and test a dummy PageCU.
  resource_coordinator::CoordinationUnitID page_id(
      resource_coordinator::CoordinationUnitType::kPage,
      resource_coordinator::CoordinationUnitID::RANDOM_ID);
  resource_coordinator::mojom::PageCoordinationUnitPtr page_cu;
  provider->CreatePageCoordinationUnit(mojo::MakeRequest(&page_cu), page_id);
  TestCU(page_cu);

  // Create and test a dummy SystemCU.
  resource_coordinator::mojom::SystemCoordinationUnitPtr system_cu;
  provider->GetSystemCoordinationUnit(mojo::MakeRequest(&system_cu));
  TestCU(system_cu);

  // Create and test a dummy ProcessCU.
  resource_coordinator::CoordinationUnitID process_id(
      resource_coordinator::CoordinationUnitType::kProcess,
      resource_coordinator::CoordinationUnitID::RANDOM_ID);
  resource_coordinator::mojom::ProcessCoordinationUnitPtr process_cu;
  provider->CreateProcessCoordinationUnit(mojo::MakeRequest(&process_cu),
                                          process_id);
  TestCU(process_cu);

  // Also test the convenience headers for creating and communicating with CUs.
  FrameResourceCoordinator frame_rc(performance_manager());
  TestCU(&frame_rc);
  PageResourceCoordinator page_rc(performance_manager());
  TestCU(&page_rc);
  ProcessResourceCoordinator process_rc(performance_manager());
  TestCU(&process_rc);
  SystemResourceCoordinator system_rc(performance_manager());
  TestCU(&system_rc);
}

}  // namespace performance_manager
