// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/memory_pressure_monitor_mac.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class TestMemoryPressureMonitorMac : public MemoryPressureMonitorMac {
 public:
  using MemoryPressureMonitorMac::MemoryPressureLevelForMacMemoryPressure;

  TestMemoryPressureMonitorMac() { }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestMemoryPressureMonitorMac);
};

TEST(TestMemoryPressureMonitorMac, MemoryPressureFromMacMemoryPressure) {
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            TestMemoryPressureMonitorMac::
                MemoryPressureLevelForMacMemoryPressure(
                    DISPATCH_MEMORYPRESSURE_NORMAL));
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            TestMemoryPressureMonitorMac::
                MemoryPressureLevelForMacMemoryPressure(
                    DISPATCH_MEMORYPRESSURE_WARN));
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            TestMemoryPressureMonitorMac::
                MemoryPressureLevelForMacMemoryPressure(
                    DISPATCH_MEMORYPRESSURE_CRITICAL));
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            TestMemoryPressureMonitorMac::
                MemoryPressureLevelForMacMemoryPressure(0));
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            TestMemoryPressureMonitorMac::
                MemoryPressureLevelForMacMemoryPressure(3));
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            TestMemoryPressureMonitorMac::
                MemoryPressureLevelForMacMemoryPressure(5));
  EXPECT_EQ(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            TestMemoryPressureMonitorMac::
                MemoryPressureLevelForMacMemoryPressure(-1));
}

TEST(TestMemoryPressureMonitorMac, CurrentMemoryPressure) {
  TestMemoryPressureMonitorMac monitor;
  MemoryPressureListener::MemoryPressureLevel memory_pressure =
      monitor.GetCurrentPressureLevel();
  EXPECT_TRUE(memory_pressure ==
                  MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE ||
              memory_pressure ==
                  MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE ||
              memory_pressure ==
                  MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
}

}  // namespace base
