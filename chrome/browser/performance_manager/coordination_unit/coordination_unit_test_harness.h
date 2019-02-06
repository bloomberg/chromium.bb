// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_COORDINATION_UNIT_COORDINATION_UNIT_TEST_HARNESS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_COORDINATION_UNIT_COORDINATION_UNIT_TEST_HARNESS_H_

#include <stdint.h>
#include <string>

#include "base/test/scoped_task_environment.h"
#include "chrome/browser/performance_manager/coordination_unit/coordination_unit_base.h"
#include "chrome/browser/performance_manager/coordination_unit/coordination_unit_graph.h"
#include "chrome/browser/performance_manager/coordination_unit/coordination_unit_provider_impl.h"
#include "chrome/browser/performance_manager/coordination_unit/system_coordination_unit_impl.h"
#include "services/service_manager/public/cpp/service_keepalive.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {
struct CoordinationUnitID;
}  // namespace resource_coordinator

namespace performance_manager {

class SystemCoordinationUnitImpl;

template <class CoordinationUnitClass>
class TestCoordinationUnitWrapper {
 public:
  static TestCoordinationUnitWrapper<CoordinationUnitClass> Create(
      CoordinationUnitGraph* graph) {
    resource_coordinator::CoordinationUnitID cu_id(
        CoordinationUnitClass::Type(),
        resource_coordinator::CoordinationUnitID::RANDOM_ID);
    return TestCoordinationUnitWrapper<CoordinationUnitClass>(
        CoordinationUnitClass::Create(cu_id, graph, nullptr));
  }

  explicit TestCoordinationUnitWrapper(CoordinationUnitClass* impl)
      : impl_(impl) {
    DCHECK(impl);
  }
  ~TestCoordinationUnitWrapper() { reset(); }

  CoordinationUnitClass* operator->() const { return impl_; }

  TestCoordinationUnitWrapper(TestCoordinationUnitWrapper&& other)
      : impl_(other.impl_) {}

  CoordinationUnitClass* get() const { return impl_; }

  void reset() {
    if (impl_) {
      impl_->Destruct();
      impl_ = nullptr;
    }
  }

 private:
  CoordinationUnitClass* impl_;

  DISALLOW_COPY_AND_ASSIGN(TestCoordinationUnitWrapper);
};

class CoordinationUnitTestHarness : public testing::Test {
 public:
  CoordinationUnitTestHarness();
  ~CoordinationUnitTestHarness() override;

  template <class CoordinationUnitClass>
  TestCoordinationUnitWrapper<CoordinationUnitClass> CreateCoordinationUnit(
      resource_coordinator::CoordinationUnitID cu_id) {
    return TestCoordinationUnitWrapper<CoordinationUnitClass>(
        CoordinationUnitClass::Create(cu_id, coordination_unit_graph(),
                                      service_keepalive_.CreateRef()));
  }

  template <class CoordinationUnitClass>
  TestCoordinationUnitWrapper<CoordinationUnitClass> CreateCoordinationUnit() {
    resource_coordinator::CoordinationUnitID cu_id(
        CoordinationUnitClass::Type(),
        resource_coordinator::CoordinationUnitID::RANDOM_ID);
    return CreateCoordinationUnit<CoordinationUnitClass>(cu_id);
  }

  TestCoordinationUnitWrapper<SystemCoordinationUnitImpl>
  GetSystemCoordinationUnit() {
    return TestCoordinationUnitWrapper<SystemCoordinationUnitImpl>(
        coordination_unit_graph()->FindOrCreateSystemCoordinationUnit(
            service_keepalive_.CreateRef()));
  }

  // testing::Test:
  void TearDown() override;

 protected:
  base::test::ScopedTaskEnvironment& task_env() { return task_env_; }
  CoordinationUnitGraph* coordination_unit_graph() {
    return &coordination_unit_graph_;
  }
  CoordinationUnitProviderImpl* provider() { return &provider_; }

 private:
  base::test::ScopedTaskEnvironment task_env_;
  service_manager::ServiceKeepalive service_keepalive_;
  CoordinationUnitGraph coordination_unit_graph_;
  CoordinationUnitProviderImpl provider_;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_COORDINATION_UNIT_COORDINATION_UNIT_TEST_HARNESS_H_
