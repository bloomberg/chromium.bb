// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_dump_manager.h"

#include "base/bind_helpers.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
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
    CreateProcessDump(args, callback);
  }

  bool IsCoordinatorProcess() const override { return false; }
};

class MemoryDumpManagerTest : public testing::Test {
 public:
  void SetUp() override {
    message_loop_.reset(new MessageLoop());
    mdm_.reset(new MemoryDumpManager());
    MemoryDumpManager::SetInstanceForTesting(mdm_.get());
    ASSERT_EQ(mdm_, MemoryDumpManager::GetInstance());
    MemoryDumpManager::GetInstance()->Initialize();
    MemoryDumpManager::GetInstance()->SetDelegate(&delegate_);
  }

  void TearDown() override {
    MemoryDumpManager::SetInstanceForTesting(nullptr);
    mdm_.reset();
    message_loop_.reset();
    TraceLog::DeleteForTesting();
  }

  void DumpCallbackAdapter(scoped_refptr<SingleThreadTaskRunner> task_runner,
                           Closure closure,
                           uint64 dump_guid,
                           bool success) {
    task_runner->PostTask(FROM_HERE, closure);
  }

 protected:
  const char* kTraceCategory = MemoryDumpManager::kTraceCategoryForTesting;

  void EnableTracing(const char* category) {
    TraceLog::GetInstance()->SetEnabled(
        CategoryFilter(category), TraceLog::RECORDING_MODE, TraceOptions());
  }

  void DisableTracing() { TraceLog::GetInstance()->SetDisabled(); }

  scoped_ptr<MemoryDumpManager> mdm_;

 private:
  scoped_ptr<MessageLoop> message_loop_;
  MemoryDumpManagerDelegateForTesting delegate_;

  // We want our singleton torn down after each test.
  ShadowingAtExitManager at_exit_manager_;
};

class MockDumpProvider : public MemoryDumpProvider {
 public:
  MockDumpProvider() {}

  explicit MockDumpProvider(
      const scoped_refptr<SingleThreadTaskRunner>& task_runner)
      : MemoryDumpProvider(task_runner) {}

  // Ctor for the SharedSessionState test.
  explicit MockDumpProvider(const std::string& id) {
    DeclareAllocatorAttribute("allocator" + id, "attr" + id, "type" + id);
  }

  MOCK_METHOD1(DumpInto, bool(ProcessMemoryDump* pmd));

  // DumpInto() override for the ActiveDumpProviderConsistency test.
  bool DumpIntoAndCheckDumpProviderCurrentlyActive(ProcessMemoryDump* pmd) {
    EXPECT_EQ(
        this,
        MemoryDumpManager::GetInstance()->dump_provider_currently_active());
    return true;
  }

  // DumpInto() override for the RespectTaskRunnerAffinity test.
  bool DumpIntoAndCheckTaskRunner(ProcessMemoryDump* pmd) {
    EXPECT_TRUE(task_runner()->RunsTasksOnCurrentThread());
    return true;
  }

  // DumpInto() override for the SharedSessionState test.
  bool DumpIntoAndCheckSessionState(ProcessMemoryDump* pmd) {
    EXPECT_TRUE(pmd->session_state());
    const auto& attrs_type_info =
        pmd->session_state()->allocators_attributes_type_info;
    EXPECT_TRUE(attrs_type_info.Exists("allocator1", "attr1"));
    EXPECT_TRUE(attrs_type_info.Exists("allocator2", "attr2"));
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

TEST_F(MemoryDumpManagerTest, SharedSessionState) {
  MockDumpProvider mdp1("1");  // Will declare an allocator property "attr1".
  MockDumpProvider mdp2("2");  // Will declare an allocator property "attr2".
  mdm_->RegisterDumpProvider(&mdp1);
  mdm_->RegisterDumpProvider(&mdp2);

  EnableTracing(kTraceCategory);
  EXPECT_CALL(mdp1, DumpInto(_)).Times(2).WillRepeatedly(
      Invoke(&mdp1, &MockDumpProvider::DumpIntoAndCheckSessionState));
  EXPECT_CALL(mdp2, DumpInto(_)).Times(2).WillRepeatedly(
      Invoke(&mdp2, &MockDumpProvider::DumpIntoAndCheckSessionState));

  for (int i = 0; i < 2; ++i)
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

// Checks that the MemoryDumpManager respects the thread affinity when a
// MemoryDumpProvider specifies a task_runner(). The test starts creating 8
// threads and registering a MemoryDumpProvider on each of them. At each
// iteration, one thread is removed, to check the live unregistration logic.
TEST_F(MemoryDumpManagerTest, RespectTaskRunnerAffinity) {
  const uint32 kNumInitialThreads = 8;

  ScopedVector<Thread> threads;
  ScopedVector<MockDumpProvider> mdps;

  // Create the threads and setup the expectations. Given that at each iteration
  // we will pop out one thread/MemoryDumpProvider, each MDP is supposed to be
  // invoked a number of times equal to its index.
  for (uint32 i = kNumInitialThreads; i > 0; --i) {
    threads.push_back(new Thread("test thread"));
    threads.back()->Start();
    mdps.push_back(new MockDumpProvider(threads.back()->task_runner()));
    MockDumpProvider* mdp = mdps.back();
    mdm_->RegisterDumpProvider(mdp);
    EXPECT_CALL(*mdp, DumpInto(_))
        .Times(i)
        .WillRepeatedly(
            Invoke(mdp, &MockDumpProvider::DumpIntoAndCheckTaskRunner));
  }

  EnableTracing(kTraceCategory);

  while (!threads.empty()) {
    {
      RunLoop run_loop;
      MemoryDumpCallback callback =
          Bind(&MemoryDumpManagerTest::DumpCallbackAdapter, Unretained(this),
               MessageLoop::current()->task_runner(), run_loop.QuitClosure());
      mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED, callback);
      // This nested message loop (|run_loop|) will be quit if and only if
      // the RequestGlobalDump callback is invoked.
      run_loop.Run();
    }

    // Unregister a MDP and destroy one thread at each iteration to check the
    // live unregistration logic. The unregistration needs to happen on the same
    // thread the MDP belongs to.
    {
      RunLoop run_loop;
      Closure unregistration =
          Bind(&MemoryDumpManager::UnregisterDumpProvider,
               Unretained(mdm_.get()), Unretained(mdps.back()));
      threads.back()->task_runner()->PostTaskAndReply(FROM_HERE, unregistration,
                                                      run_loop.QuitClosure());
      run_loop.Run();
    }
    mdps.pop_back();
    threads.back()->Stop();
    threads.pop_back();
  }

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
