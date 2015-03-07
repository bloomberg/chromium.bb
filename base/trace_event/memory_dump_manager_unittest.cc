// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_dump_manager.h"

#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/process_memory_dump.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace base {
namespace trace_event {

class MemoryDumpManagerTest : public testing::Test {
 public:
  void SetUp() override {
    mdm_.reset(new MemoryDumpManager());
    MemoryDumpManager::SetInstanceForTesting(mdm_.get());
    ASSERT_EQ(mdm_, MemoryDumpManager::GetInstance());
    MemoryDumpManager::GetInstance()->Initialize();
  }

  void TearDown() override {
    MemoryDumpManager::SetInstanceForTesting(nullptr);
    mdm_.reset();
    TraceLog::DeleteForTesting();
  }

 protected:
  const char* const kTraceCategory = MemoryDumpManager::kTraceCategory;

  void EnableTracing(const char* category) {
    TraceLog::GetInstance()->SetEnabled(
        CategoryFilter(category), TraceLog::RECORDING_MODE, TraceOptions());
  }

  void DisableTracing() { TraceLog::GetInstance()->SetDisabled(); }

  scoped_ptr<MemoryDumpManager> mdm_;

 private:
  // We want our singleton torn down after each test.
  ShadowingAtExitManager at_exit_manager_;
};

class MockDumpProvider : public MemoryDumpProvider {
 public:
  MOCK_METHOD1(DumpInto, bool(ProcessMemoryDump* pmd));

  const char* GetFriendlyName() const override { return "MockDumpProvider"; }
};

TEST_F(MemoryDumpManagerTest, SingleDumper) {
  MockDumpProvider mdp;
  mdm_->RegisterDumpProvider(&mdp);

  // Check that the dumper is not called if the memory category is not enabled.
  EnableTracing("foo-and-bar-but-not-memory");
  EXPECT_CALL(mdp, DumpInto(_)).Times(0);
  mdm_->RequestDumpPoint(DumpPointType::EXPLICITLY_TRIGGERED);
  DisableTracing();

  // Now repeat enabling the memory category and check that the dumper is
  // invoked this time.
  EnableTracing(kTraceCategory);
  EXPECT_CALL(mdp, DumpInto(_)).Times(3).WillRepeatedly(Return(true));
  for (int i = 0; i < 3; ++i)
    mdm_->RequestDumpPoint(DumpPointType::EXPLICITLY_TRIGGERED);
  DisableTracing();

  mdm_->UnregisterDumpProvider(&mdp);

  // Finally check the unregister logic (no calls to the mdp after unregister).
  EnableTracing(kTraceCategory);
  EXPECT_CALL(mdp, DumpInto(_)).Times(0);
  mdm_->RequestDumpPoint(DumpPointType::EXPLICITLY_TRIGGERED);
  TraceLog::GetInstance()->SetDisabled();
}

TEST_F(MemoryDumpManagerTest, UnregisterDumperWhileTracing) {
  MockDumpProvider mdp;
  mdm_->RegisterDumpProvider(&mdp);

  EnableTracing(kTraceCategory);
  EXPECT_CALL(mdp, DumpInto(_)).Times(1).WillRepeatedly(Return(true));
  mdm_->RequestDumpPoint(DumpPointType::EXPLICITLY_TRIGGERED);

  mdm_->UnregisterDumpProvider(&mdp);
  EXPECT_CALL(mdp, DumpInto(_)).Times(0);
  mdm_->RequestDumpPoint(DumpPointType::EXPLICITLY_TRIGGERED);

  DisableTracing();
}

TEST_F(MemoryDumpManagerTest, MultipleDumpers) {
  MockDumpProvider mdp1;
  MockDumpProvider mdp2;

  // Enable only mdp1.
  mdm_->RegisterDumpProvider(&mdp1);
  EnableTracing(kTraceCategory);
  EXPECT_CALL(mdp1, DumpInto(_)).Times(1).WillRepeatedly(Return(true));
  EXPECT_CALL(mdp2, DumpInto(_)).Times(0);
  mdm_->RequestDumpPoint(DumpPointType::EXPLICITLY_TRIGGERED);
  DisableTracing();

  // Invert: enable mdp1 and disable mdp2.
  mdm_->UnregisterDumpProvider(&mdp1);
  mdm_->RegisterDumpProvider(&mdp2);
  EnableTracing(kTraceCategory);
  EXPECT_CALL(mdp1, DumpInto(_)).Times(0);
  EXPECT_CALL(mdp2, DumpInto(_)).Times(1).WillRepeatedly(Return(true));
  mdm_->RequestDumpPoint(DumpPointType::EXPLICITLY_TRIGGERED);
  DisableTracing();

  // Enable both mdp1 and mdp2.
  mdm_->RegisterDumpProvider(&mdp1);
  EnableTracing(kTraceCategory);
  EXPECT_CALL(mdp1, DumpInto(_)).Times(1).WillRepeatedly(Return(true));
  EXPECT_CALL(mdp2, DumpInto(_)).Times(1).WillRepeatedly(Return(true));
  mdm_->RequestDumpPoint(DumpPointType::EXPLICITLY_TRIGGERED);
  DisableTracing();
}

// Enable both dump providers, make mdp1 fail and assert that only mdp2 is
// invoked the 2nd time.
// FIXME(primiano): remove once crbug.com/461788 gets fixed.
TEST_F(MemoryDumpManagerTest, DisableFailingDumpers) {
  MockDumpProvider mdp1;
  MockDumpProvider mdp2;

  mdm_->RegisterDumpProvider(&mdp1);
  mdm_->RegisterDumpProvider(&mdp2);
  EnableTracing(kTraceCategory);

  EXPECT_CALL(mdp1, DumpInto(_)).Times(1).WillRepeatedly(Return(false));
  EXPECT_CALL(mdp2, DumpInto(_)).Times(1).WillRepeatedly(Return(true));
  mdm_->RequestDumpPoint(DumpPointType::EXPLICITLY_TRIGGERED);

  EXPECT_CALL(mdp1, DumpInto(_)).Times(0);
  EXPECT_CALL(mdp2, DumpInto(_)).Times(1).WillRepeatedly(Return(false));
  mdm_->RequestDumpPoint(DumpPointType::EXPLICITLY_TRIGGERED);

  DisableTracing();
}

}  // namespace trace_event
}  // namespace base
