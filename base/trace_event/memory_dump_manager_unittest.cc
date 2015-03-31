// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_dump_manager.h"

#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/process_memory_dump.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Return;

namespace base {
namespace trace_event {

// Testing MemoryDumpManagerDelegate which short-circuits dump requests locally
// instead of performing IPC dances.
class MemoryDumpManagerDelegateForTesting : public MemoryDumpManagerDelegate {
 public:
  void RequestGlobalMemoryDump(
      const base::trace_event::MemoryDumpRequestArgs& args,
      const MemoryDumpCallback& callback) override {
    MemoryDumpManager::GetInstance()->CreateProcessDump(args);
  }
};

class MemoryDumpManagerTest : public testing::Test {
 public:
  void SetUp() override {
    mdm_.reset(new MemoryDumpManager());
    MemoryDumpManager::SetInstanceForTesting(mdm_.get());
    ASSERT_EQ(mdm_, MemoryDumpManager::GetInstance());
    MemoryDumpManager::GetInstance()->Initialize();
    MemoryDumpManager::GetInstance()->SetDelegate(&delegate_);
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
  MemoryDumpManagerDelegateForTesting delegate_;

  // We want our singleton torn down after each test.
  ShadowingAtExitManager at_exit_manager_;
};

class MockDumpProvider : public MemoryDumpProvider {
 public:
  MOCK_METHOD1(DumpInto, bool(ProcessMemoryDump* pmd));

  // DumpInto() override for the ActiveDumpProviderConsistency test,
  bool DumpIntoAndCheckDumpProviderCurrentlyActive(ProcessMemoryDump* pmd) {
    EXPECT_EQ(
        this,
        MemoryDumpManager::GetInstance()->dump_provider_currently_active());
    return true;
  }

  const char* GetFriendlyName() const override { return "MockDumpProvider"; }
};

TEST_F(MemoryDumpManagerTest, SingleDumper) {
  MockDumpProvider mdp;
  mdm_->RegisterDumpProvider(&mdp);

  // Check that the dumper is not called if the memory category is not enabled.
  EnableTracing("foo-and-bar-but-not-memory");
  EXPECT_CALL(mdp, DumpInto(_)).Times(0);
  mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED);
  DisableTracing();

  // Now repeat enabling the memory category and check that the dumper is
  // invoked this time.
  EnableTracing(kTraceCategory);
  EXPECT_CALL(mdp, DumpInto(_)).Times(3).WillRepeatedly(Return(true));
  for (int i = 0; i < 3; ++i)
    mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED);
  DisableTracing();

  mdm_->UnregisterDumpProvider(&mdp);

  // Finally check the unregister logic (no calls to the mdp after unregister).
  EnableTracing(kTraceCategory);
  EXPECT_CALL(mdp, DumpInto(_)).Times(0);
  mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED);
  TraceLog::GetInstance()->SetDisabled();
}

TEST_F(MemoryDumpManagerTest, UnregisterDumperWhileTracing) {
  MockDumpProvider mdp;
  mdm_->RegisterDumpProvider(&mdp);

  EnableTracing(kTraceCategory);
  EXPECT_CALL(mdp, DumpInto(_)).Times(1).WillRepeatedly(Return(true));
  mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED);

  mdm_->UnregisterDumpProvider(&mdp);
  EXPECT_CALL(mdp, DumpInto(_)).Times(0);
  mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED);

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
  mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED);
  DisableTracing();

  // Invert: enable mdp1 and disable mdp2.
  mdm_->UnregisterDumpProvider(&mdp1);
  mdm_->RegisterDumpProvider(&mdp2);
  EnableTracing(kTraceCategory);
  EXPECT_CALL(mdp1, DumpInto(_)).Times(0);
  EXPECT_CALL(mdp2, DumpInto(_)).Times(1).WillRepeatedly(Return(true));
  mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED);
  DisableTracing();

  // Enable both mdp1 and mdp2.
  mdm_->RegisterDumpProvider(&mdp1);
  EnableTracing(kTraceCategory);
  EXPECT_CALL(mdp1, DumpInto(_)).Times(1).WillRepeatedly(Return(true));
  EXPECT_CALL(mdp2, DumpInto(_)).Times(1).WillRepeatedly(Return(true));
  mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED);
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
  mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED);

  EXPECT_CALL(mdp1, DumpInto(_)).Times(0);
  EXPECT_CALL(mdp2, DumpInto(_)).Times(1).WillRepeatedly(Return(false));
  mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED);

  DisableTracing();
}

// TODO(primiano): remove once crbug.com/466121 gets fixed.
// Ascertains that calls to MDM::dump_provider_currently_active() actually
// returns the MemoryDumpProvider currently active during the DumpInto() call.
TEST_F(MemoryDumpManagerTest, ActiveDumpProviderConsistency) {
  MockDumpProvider mdp1;
  MockDumpProvider mdp2;

  mdm_->RegisterDumpProvider(&mdp1);
  mdm_->RegisterDumpProvider(&mdp2);
  EnableTracing(kTraceCategory);
  EXPECT_CALL(mdp1, DumpInto(_))
      .Times(2)
      .WillRepeatedly(Invoke(
          &mdp1,
          &MockDumpProvider::DumpIntoAndCheckDumpProviderCurrentlyActive));
  EXPECT_CALL(mdp2, DumpInto(_))
      .Times(2)
      .WillRepeatedly(Invoke(
          &mdp2,
          &MockDumpProvider::DumpIntoAndCheckDumpProviderCurrentlyActive));
  mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED);
  mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED);
  DisableTracing();
}

}  // namespace trace_event
}  // namespace base
