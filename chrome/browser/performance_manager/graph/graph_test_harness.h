// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_GRAPH_TEST_HARNESS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_GRAPH_TEST_HARNESS_H_

#include <stdint.h>
#include <string>

#include "base/test/scoped_task_environment.h"
#include "chrome/browser/performance_manager/graph/graph.h"
#include "chrome/browser/performance_manager/graph/graph_node_provider_impl.h"
#include "chrome/browser/performance_manager/graph/node_base.h"
#include "chrome/browser/performance_manager/graph/system_node_impl.h"
#include "services/service_manager/public/cpp/service_keepalive.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {
struct CoordinationUnitID;
}  // namespace resource_coordinator

namespace performance_manager {

class SystemNodeImpl;

template <class NodeClass>
class TestNodeWrapper {
 public:
  static TestNodeWrapper<NodeClass> Create(Graph* graph) {
    resource_coordinator::CoordinationUnitID cu_id(
        NodeClass::Type(), resource_coordinator::CoordinationUnitID::RANDOM_ID);
    return TestNodeWrapper<NodeClass>(NodeClass::Create(cu_id, graph, nullptr));
  }

  explicit TestNodeWrapper(NodeClass* impl) : impl_(impl) { DCHECK(impl); }
  ~TestNodeWrapper() { reset(); }

  NodeClass* operator->() const { return impl_; }

  TestNodeWrapper(TestNodeWrapper&& other) : impl_(other.impl_) {}

  NodeClass* get() const { return impl_; }

  void reset() {
    if (impl_) {
      impl_->Destruct();
      impl_ = nullptr;
    }
  }

 private:
  NodeClass* impl_;

  DISALLOW_COPY_AND_ASSIGN(TestNodeWrapper);
};

class GraphTestHarness : public testing::Test {
 public:
  GraphTestHarness();
  ~GraphTestHarness() override;

  template <class NodeClass>
  TestNodeWrapper<NodeClass> CreateCoordinationUnit(
      resource_coordinator::CoordinationUnitID cu_id) {
    return TestNodeWrapper<NodeClass>(NodeClass::Create(
        cu_id, coordination_unit_graph(), service_keepalive_.CreateRef()));
  }

  template <class NodeClass>
  TestNodeWrapper<NodeClass> CreateCoordinationUnit() {
    resource_coordinator::CoordinationUnitID cu_id(
        NodeClass::Type(), resource_coordinator::CoordinationUnitID::RANDOM_ID);
    return CreateCoordinationUnit<NodeClass>(cu_id);
  }

  TestNodeWrapper<SystemNodeImpl> GetSystemCoordinationUnit() {
    return TestNodeWrapper<SystemNodeImpl>(
        coordination_unit_graph()->FindOrCreateSystemNode(
            service_keepalive_.CreateRef()));
  }

  // testing::Test:
  void TearDown() override;

 protected:
  base::test::ScopedTaskEnvironment& task_env() { return task_env_; }
  Graph* coordination_unit_graph() { return &coordination_unit_graph_; }
  GraphNodeProviderImpl* provider() { return &provider_; }

 private:
  base::test::ScopedTaskEnvironment task_env_;
  service_manager::ServiceKeepalive service_keepalive_;
  Graph coordination_unit_graph_;
  GraphNodeProviderImpl provider_;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_GRAPH_TEST_HARNESS_H_
