// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/graph_impl.h"

#include "base/memory/ptr_util.h"
#include "base/process/process.h"
#include "base/time/time.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/system_node_impl.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

using GraphImplTest = GraphTestHarness;

TEST_F(GraphImplTest, SafeCasting) {
  const Graph* graph_base = graph();
  EXPECT_EQ(graph(), GraphImpl::FromGraph(graph_base));
}

TEST_F(GraphImplTest, FindOrCreateSystemNode) {
  EXPECT_TRUE(graph()->IsEmpty());
  SystemNodeImpl* system_node = graph()->FindOrCreateSystemNodeImpl();
  EXPECT_FALSE(graph()->IsEmpty());

  // A second request should return the same instance.
  EXPECT_EQ(system_node, graph()->FindOrCreateSystemNodeImpl());
}

TEST_F(GraphImplTest, GetProcessNodeByPid) {
  TestNodeWrapper<ProcessNodeImpl> process =
      TestNodeWrapper<ProcessNodeImpl>::Create(graph());
  EXPECT_EQ(base::kNullProcessId, process->process_id());
  EXPECT_FALSE(process->process().IsValid());

  const base::Process self = base::Process::Current();

  EXPECT_EQ(nullptr, graph()->GetProcessNodeByPid(self.Pid()));
  process->SetProcess(self.Duplicate(), base::Time::Now());
  EXPECT_TRUE(process->process().IsValid());
  EXPECT_EQ(self.Pid(), process->process_id());
  EXPECT_EQ(process.get(), graph()->GetProcessNodeByPid(self.Pid()));

  // Validate that an exited process isn't removed (yet).
  process->SetProcessExitStatus(0xCAFE);
  EXPECT_FALSE(process->process().IsValid());
  EXPECT_EQ(self.Pid(), process->process_id());
  EXPECT_EQ(process.get(), graph()->GetProcessNodeByPid(self.Pid()));

  process.reset();

  EXPECT_EQ(nullptr, graph()->GetProcessNodeByPid(self.Pid()));
}

TEST_F(GraphImplTest, PIDReuse) {
  // This test emulates what happens on Windows under aggressive PID reuse,
  // where a process termination notification can be delayed until after the
  // PID has been reused for a new process.
  static base::Process self = base::Process::Current();

  TestNodeWrapper<ProcessNodeImpl> process1 =
      TestNodeWrapper<ProcessNodeImpl>::Create(graph());
  TestNodeWrapper<ProcessNodeImpl> process2 =
      TestNodeWrapper<ProcessNodeImpl>::Create(graph());

  process1->SetProcess(self.Duplicate(), base::Time::Now());
  EXPECT_EQ(process1.get(), graph()->GetProcessNodeByPid(self.Pid()));

  // First process exits, but hasn't been deleted yet.
  process1->SetProcessExitStatus(0xCAFE);
  EXPECT_EQ(process1.get(), graph()->GetProcessNodeByPid(self.Pid()));

  // The second registration for the same PID should override the first one.
  process2->SetProcess(self.Duplicate(), base::Time::Now());
  EXPECT_EQ(process2.get(), graph()->GetProcessNodeByPid(self.Pid()));

  // The destruction of the first process node shouldn't clear the PID
  // registration.
  process1.reset();
  EXPECT_EQ(process2.get(), graph()->GetProcessNodeByPid(self.Pid()));
}

TEST_F(GraphImplTest, GetAllCUsByType) {
  MockMultiplePagesInSingleProcessGraph mock_graph(graph());

  std::vector<ProcessNodeImpl*> processes = graph()->GetAllProcessNodeImpls();
  ASSERT_EQ(1u, processes.size());
  EXPECT_NE(nullptr, processes[0]);

  std::vector<FrameNodeImpl*> frames = graph()->GetAllFrameNodeImpls();
  ASSERT_EQ(2u, frames.size());
  EXPECT_NE(nullptr, frames[0]);
  EXPECT_NE(nullptr, frames[1]);

  std::vector<PageNodeImpl*> pages = graph()->GetAllPageNodeImpls();
  ASSERT_EQ(2u, pages.size());
  EXPECT_NE(nullptr, pages[0]);
  EXPECT_NE(nullptr, pages[1]);
}

namespace {

class LenientMockObserver : public GraphObserver {
 public:
  LenientMockObserver() {}
  ~LenientMockObserver() override {}

  MOCK_METHOD1(OnBeforeGraphDestroyed, void(Graph*));
};

using MockObserver = ::testing::StrictMock<LenientMockObserver>;

using testing::_;
using testing::Invoke;

}  // namespace

TEST_F(GraphImplTest, ObserverWorks) {
  std::unique_ptr<GraphImpl> graph = base::WrapUnique(new GraphImpl());
  Graph* raw_graph = graph.get();

  MockObserver obs;
  graph->AddGraphObserver(&obs);
  graph->RemoveGraphObserver(&obs);
  graph->AddGraphObserver(&obs);

  // Expect the graph teardown callback to be invoked. We have to unregister our
  // observer in order to maintain graph invariants.
  EXPECT_CALL(obs, OnBeforeGraphDestroyed(raw_graph))
      .WillOnce(testing::Invoke(
          [&obs](Graph* graph) { graph->RemoveGraphObserver(&obs); }));
  graph->TearDown();
  graph.reset();
}

namespace {

class Foo : public GraphOwned {
 public:
  explicit Foo(int* destructor_count) : destructor_count_(destructor_count) {}

  ~Foo() override { (*destructor_count_)++; }

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override { passed_to_called_ = true; }
  void OnTakenFromGraph(Graph* graph) override { taken_from_called_ = true; }

  bool passed_to_called() const { return passed_to_called_; }
  bool taken_from_called() const { return taken_from_called_; }

 private:
  bool passed_to_called_ = false;
  bool taken_from_called_ = false;
  int* destructor_count_ = nullptr;
};

}  // namespace

TEST_F(GraphImplTest, GraphOwned) {
  int destructor_count = 0;

  std::unique_ptr<Foo> foo1 = base::WrapUnique(new Foo(&destructor_count));
  std::unique_ptr<Foo> foo2 = base::WrapUnique(new Foo(&destructor_count));
  auto* raw1 = foo1.get();
  auto* raw2 = foo2.get();

  // Pass both objects to the graph.
  std::unique_ptr<GraphImpl> graph = base::WrapUnique(new GraphImpl());
  EXPECT_EQ(0u, graph->GraphOwnedCountForTesting());
  EXPECT_FALSE(raw1->passed_to_called());
  graph->PassToGraph(std::move(foo1));
  EXPECT_TRUE(raw1->passed_to_called());
  EXPECT_EQ(1u, graph->GraphOwnedCountForTesting());
  EXPECT_FALSE(raw2->passed_to_called());
  graph->PassToGraph(std::move(foo2));
  EXPECT_TRUE(raw2->passed_to_called());
  EXPECT_EQ(2u, graph->GraphOwnedCountForTesting());

  // Take one back.
  EXPECT_FALSE(raw1->taken_from_called());
  foo1 = graph->TakeFromGraphAs<Foo>(raw1);
  EXPECT_TRUE(raw1->taken_from_called());
  EXPECT_EQ(1u, graph->GraphOwnedCountForTesting());

  // Destroy that object and expect its destructor to have been invoked.
  EXPECT_EQ(0, destructor_count);
  foo1.reset();
  EXPECT_EQ(1, destructor_count);

  // Now destroy the graph and expect the other object to have been torn down
  // too.
  graph->TearDown();
  graph.reset();
  EXPECT_EQ(2, destructor_count);
}

namespace {

class TestNodeDataDescriber : public NodeDataDescriber {
 public:
  explicit TestNodeDataDescriber(base::StringPiece name) : name_(name) {}

  base::Value DescribeFrameNodeData(const FrameNode* node) const override {
    base::Value list(base::Value::Type::LIST);
    list.Append(name_);
    list.Append("FrameNode");
    return list;
  }

  base::Value DescribePageNodeData(const PageNode* node) const override {
    base::Value list(base::Value::Type::LIST);
    list.Append(name_);
    list.Append("PageNode");
    return list;
  }

  base::Value DescribeProcessNodeData(const ProcessNode* node) const override {
    base::Value list(base::Value::Type::LIST);
    list.Append(name_);
    list.Append("ProcessNode");
    return list;
  }

  base::Value DescribeSystemNodeData(const SystemNode* node) const override {
    base::Value list(base::Value::Type::LIST);
    list.Append(name_);
    list.Append("SystemNode");
    return list;
  }

  base::Value DescribeWorkerNodeData(const WorkerNode* node) const override {
    base::Value list(base::Value::Type::LIST);
    list.Append(name_);
    list.Append("WorkerNode");
    return list;
  }

 private:
  const std::string name_;
};

void AssertDictValueContainsListKey(const base::Value& descr,
                                    const char* key,
                                    const char* s1,
                                    const char* s2) {
  ASSERT_TRUE(descr.is_dict());
  const base::Value* v = descr.FindListKey(key);
  ASSERT_NE(nullptr, v);

  const auto list = v->GetList();
  ASSERT_EQ(2u, list.size());
  ASSERT_EQ(list[0], base::Value(s1));
  ASSERT_EQ(list[1], base::Value(s2));
}

}  // namespace

TEST_F(GraphImplTest, NodeDataDescribers) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  NodeDataDescriberRegistry* registry = graph()->GetNodeDataDescriberRegistry();

  // No describers->no description.
  base::Value descr = registry->DescribeNodeData(mock_graph.frame.get());
  EXPECT_EQ(0u, descr.DictSize());

  // Test that the default impl does nothing.
  NodeDataDescriberDefaultImpl default_impl;
  registry->RegisterDescriber(&default_impl, "default_impl");

  // Test a single non-default describer for each node type.
  TestNodeDataDescriber d1("d1");
  registry->RegisterDescriber(&d1, "d1");

  descr = registry->DescribeNodeData(mock_graph.frame.get());
  AssertDictValueContainsListKey(descr, "d1", "d1", "FrameNode");
  EXPECT_EQ(1u, descr.DictSize());

  descr = registry->DescribeNodeData(mock_graph.page.get());
  AssertDictValueContainsListKey(descr, "d1", "d1", "PageNode");
  EXPECT_EQ(1u, descr.DictSize());

  descr = registry->DescribeNodeData(mock_graph.process.get());
  AssertDictValueContainsListKey(descr, "d1", "d1", "ProcessNode");
  EXPECT_EQ(1u, descr.DictSize());

  descr = registry->DescribeNodeData(graph()->FindOrCreateSystemNodeImpl());
  AssertDictValueContainsListKey(descr, "d1", "d1", "SystemNode");
  EXPECT_EQ(1u, descr.DictSize());

  auto worker = CreateNode<WorkerNodeImpl>(WorkerNode::WorkerType::kDedicated,
                                           mock_graph.process.get());
  descr = registry->DescribeNodeData(worker.get());
  AssertDictValueContainsListKey(descr, "d1", "d1", "WorkerNode");
  EXPECT_EQ(1u, descr.DictSize());

  // Unregister the default impl now that it's been verified to say nothing
  // about all node types.
  registry->UnregisterDescriber(&default_impl);

  // Register a second describer and test one node type.
  TestNodeDataDescriber d2("d2");
  registry->RegisterDescriber(&d2, "d2");

  descr = registry->DescribeNodeData(mock_graph.frame.get());
  EXPECT_EQ(2u, descr.DictSize());
  AssertDictValueContainsListKey(descr, "d1", "d1", "FrameNode");
  AssertDictValueContainsListKey(descr, "d2", "d2", "FrameNode");

  registry->UnregisterDescriber(&d2);
  registry->UnregisterDescriber(&d1);

  // No describers after unregistration->no description.
  descr = registry->DescribeNodeData(mock_graph.frame.get());
  EXPECT_EQ(0u, descr.DictSize());
}

}  // namespace performance_manager
