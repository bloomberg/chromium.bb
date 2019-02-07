// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/system_node_impl.h"

#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/graph_test_harness.h"
#include "chrome/browser/performance_manager/graph/mock_graphs.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/graph/system_node_impl.h"
#include "chrome/browser/performance_manager/observers/coordination_unit_graph_observer.h"
#include "chrome/browser/performance_manager/resource_coordinator_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

// Observer used to make sure that signals are dispatched correctly.
class SystemAndProcessObserver : public GraphObserver {
 public:
  // GraphObserver implementation:
  bool ShouldObserve(const NodeBase* coordination_unit) override {
    auto cu_type = coordination_unit->id().type;
    return cu_type == resource_coordinator::CoordinationUnitType::kSystem;
  }

  void OnSystemEventReceived(
      const SystemNodeImpl* system_cu,
      const resource_coordinator::mojom::Event event) override {
    EXPECT_EQ(resource_coordinator::mojom::Event::kProcessCPUUsageReady, event);
    ++system_event_seen_count_;
  }

  void OnProcessPropertyChanged(
      const ProcessNodeImpl* process_cu,
      const resource_coordinator::mojom::PropertyType property,
      int64_t value) override {
    ++process_property_change_seen_count_;
  }

  size_t system_event_seen_count() const { return system_event_seen_count_; }
  size_t process_property_change_seen_count() const {
    return process_property_change_seen_count_;
  }

 private:
  size_t system_event_seen_count_ = 0;
  size_t process_property_change_seen_count_ = 0;
};

class SystemNodeImplTest : public GraphTestHarness {
 public:
  void SetUp() override {
    ResourceCoordinatorClock::SetClockForTesting(&clock_);

    // Sets a valid starting time.
    clock_.SetNowTicks(base::TimeTicks::Now());
  }

  void TearDown() override { ResourceCoordinatorClock::ResetClockForTesting(); }

 protected:
  void AdvanceClock(base::TimeDelta delta) { clock_.Advance(delta); }

 private:
  base::SimpleTestTickClock clock_;
};

resource_coordinator::mojom::ProcessResourceMeasurementBatchPtr
CreateMeasurementBatch(base::TimeTicks start_end_time,
                       size_t num_processes,
                       base::TimeDelta additional_cpu_time) {
  resource_coordinator::mojom::ProcessResourceMeasurementBatchPtr batch =
      resource_coordinator::mojom::ProcessResourceMeasurementBatch::New();
  batch->batch_started_time = start_end_time;
  batch->batch_ended_time = start_end_time;

  for (size_t i = 1; i <= num_processes; ++i) {
    resource_coordinator::mojom::ProcessResourceMeasurementPtr measurement =
        resource_coordinator::mojom::ProcessResourceMeasurement::New();
    measurement->pid = i;
    measurement->cpu_usage =
        base::TimeDelta::FromMicroseconds(i * 10) + additional_cpu_time;
    measurement->private_footprint_kb = static_cast<uint32_t>(i * 100);

    batch->measurements.push_back(std::move(measurement));
  }

  return batch;
}

}  // namespace

TEST_F(SystemNodeImplTest, OnProcessCPUUsageReady) {
  SystemAndProcessObserver observer;
  MockMultiplePagesWithMultipleProcessesGraph cu_graph(
      coordination_unit_graph());
  cu_graph.system->AddObserver(&observer);
  EXPECT_EQ(0u, observer.system_event_seen_count());
  cu_graph.system->OnProcessCPUUsageReady();
  EXPECT_EQ(1u, observer.system_event_seen_count());
}

TEST_F(SystemNodeImplTest, DistributeMeasurementBatch) {
  SystemAndProcessObserver observer;
  MockMultiplePagesWithMultipleProcessesGraph cu_graph(
      coordination_unit_graph());
  cu_graph.system->AddObserver(&observer);
  cu_graph.process->AddObserver(&observer);
  cu_graph.other_process->AddObserver(&observer);

  EXPECT_EQ(0u, observer.system_event_seen_count());

  // Build and dispatch a measurement batch.
  base::TimeTicks start_time = base::TimeTicks::Now();
  EXPECT_EQ(0U, observer.process_property_change_seen_count());
  cu_graph.system->DistributeMeasurementBatch(
      CreateMeasurementBatch(start_time, 3, base::TimeDelta()));

  EXPECT_EQ(start_time, cu_graph.system->last_measurement_start_time());
  EXPECT_EQ(start_time, cu_graph.system->last_measurement_end_time());

  EXPECT_EQ(2U, observer.process_property_change_seen_count());
  EXPECT_EQ(1u, observer.system_event_seen_count());

  // The first measurement batch results in a zero CPU usage for the processes.
  int64_t cpu_usage;
  EXPECT_TRUE(cu_graph.process->GetProperty(
      resource_coordinator::mojom::PropertyType::kCPUUsage, &cpu_usage));
  EXPECT_EQ(0, cpu_usage);
  EXPECT_EQ(100u, cu_graph.process->private_footprint_kb());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(10u),
            cu_graph.process->cumulative_cpu_usage());

  EXPECT_TRUE(cu_graph.other_process->GetProperty(
      resource_coordinator::mojom::PropertyType::kCPUUsage, &cpu_usage));
  EXPECT_EQ(0, cpu_usage);
  EXPECT_EQ(200u, cu_graph.other_process->private_footprint_kb());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(20u),
            cu_graph.other_process->cumulative_cpu_usage());

  EXPECT_EQ(base::TimeDelta::FromMicroseconds(5),
            cu_graph.page->cumulative_cpu_usage_estimate());
  EXPECT_EQ(50u, cu_graph.page->private_footprint_kb_estimate());

  EXPECT_EQ(base::TimeDelta::FromMicroseconds(25),
            cu_graph.other_page->cumulative_cpu_usage_estimate());
  EXPECT_EQ(250u, cu_graph.other_page->private_footprint_kb_estimate());

  // Dispatch another batch, and verify the CPUUsage is appropriately updated.
  cu_graph.system->DistributeMeasurementBatch(
      CreateMeasurementBatch(start_time + base::TimeDelta::FromMicroseconds(10),
                             3, base::TimeDelta::FromMicroseconds(10)));
  EXPECT_TRUE(cu_graph.process->GetProperty(
      resource_coordinator::mojom::PropertyType::kCPUUsage, &cpu_usage));
  EXPECT_EQ(100000, cpu_usage);
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(20u),
            cu_graph.process->cumulative_cpu_usage());
  EXPECT_TRUE(cu_graph.other_process->GetProperty(
      resource_coordinator::mojom::PropertyType::kCPUUsage, &cpu_usage));
  EXPECT_EQ(100000, cpu_usage);
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(30u),
            cu_graph.other_process->cumulative_cpu_usage());

  EXPECT_EQ(base::TimeDelta::FromMicroseconds(10),
            cu_graph.page->cumulative_cpu_usage_estimate());
  EXPECT_EQ(50u, cu_graph.page->private_footprint_kb_estimate());

  EXPECT_EQ(base::TimeDelta::FromMicroseconds(40),
            cu_graph.other_page->cumulative_cpu_usage_estimate());
  EXPECT_EQ(250u, cu_graph.other_page->private_footprint_kb_estimate());

  // Now test that a measurement batch that leaves out a process clears the
  // properties of that process - except for cumulative CPU, which can only
  // go forwards.
  cu_graph.system->DistributeMeasurementBatch(
      CreateMeasurementBatch(start_time + base::TimeDelta::FromMicroseconds(20),
                             1, base::TimeDelta::FromMicroseconds(310)));

  EXPECT_TRUE(cu_graph.process->GetProperty(
      resource_coordinator::mojom::PropertyType::kCPUUsage, &cpu_usage));
  EXPECT_EQ(3000000, cpu_usage);
  EXPECT_EQ(100u, cu_graph.process->private_footprint_kb());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(320u),
            cu_graph.process->cumulative_cpu_usage());

  EXPECT_TRUE(cu_graph.other_process->GetProperty(
      resource_coordinator::mojom::PropertyType::kCPUUsage, &cpu_usage));
  EXPECT_EQ(0, cpu_usage);
  EXPECT_EQ(0u, cu_graph.other_process->private_footprint_kb());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(30u),
            cu_graph.other_process->cumulative_cpu_usage());

  EXPECT_EQ(base::TimeDelta::FromMicroseconds(160),
            cu_graph.page->cumulative_cpu_usage_estimate());
  EXPECT_EQ(50u, cu_graph.page->private_footprint_kb_estimate());

  EXPECT_EQ(base::TimeDelta::FromMicroseconds(190),
            cu_graph.other_page->cumulative_cpu_usage_estimate());
  EXPECT_EQ(50u, cu_graph.other_page->private_footprint_kb_estimate());
}

}  // namespace performance_manager
