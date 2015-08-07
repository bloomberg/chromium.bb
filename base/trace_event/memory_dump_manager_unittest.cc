// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_dump_manager.h"

#include "base/bind_helpers.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/process_memory_dump.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Between;
using testing::Invoke;
using testing::Return;

namespace base {
namespace trace_event {
namespace {
MemoryDumpArgs high_detail_args = {MemoryDumpArgs::LevelOfDetail::HIGH};
MemoryDumpArgs low_detail_args = {MemoryDumpArgs::LevelOfDetail::LOW};
}

// Testing MemoryDumpManagerDelegate which short-circuits dump requests locally
// instead of performing IPC dances.
class MemoryDumpManagerDelegateForTesting : public MemoryDumpManagerDelegate {
 public:
  void RequestGlobalMemoryDump(const MemoryDumpRequestArgs& args,
                               const MemoryDumpCallback& callback) override {
    CreateProcessDump(args, callback);
  }

  bool IsCoordinatorProcess() const override { return false; }
  uint64 GetTracingProcessId() const override {
    return MemoryDumpManager::kInvalidTracingProcessId;
  }
};

class MemoryDumpManagerTest : public testing::Test {
 public:
  void SetUp() override {
    last_callback_success_ = false;
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
    last_callback_success_ = success;
    task_runner->PostTask(FROM_HERE, closure);
  }

 protected:
  const char* kTraceCategory = MemoryDumpManager::kTraceCategoryForTesting;

  void EnableTracing(const char* category) {
    TraceLog::GetInstance()->SetEnabled(
        TraceConfig(category, ""), TraceLog::RECORDING_MODE);
  }

  void DisableTracing() { TraceLog::GetInstance()->SetDisabled(); }

  scoped_ptr<MemoryDumpManager> mdm_;
  bool last_callback_success_;

 private:
  scoped_ptr<MessageLoop> message_loop_;
  MemoryDumpManagerDelegateForTesting delegate_;

  // We want our singleton torn down after each test.
  ShadowingAtExitManager at_exit_manager_;
};

class MockDumpProvider : public MemoryDumpProvider {
 public:
  MockDumpProvider()
      : dump_provider_to_register_or_unregister(nullptr),
        last_session_state_(nullptr),
        level_of_detail_(MemoryDumpArgs::LevelOfDetail::HIGH) {}

  // Ctor used by the RespectTaskRunnerAffinity test.
  explicit MockDumpProvider(
      const scoped_refptr<SingleThreadTaskRunner>& task_runner)
      : last_session_state_(nullptr),
        task_runner_(task_runner),
        level_of_detail_(MemoryDumpArgs::LevelOfDetail::HIGH) {}

  // Ctor used by CheckMemoryDumpArgs test.
  explicit MockDumpProvider(const MemoryDumpArgs::LevelOfDetail level_of_detail)
      : last_session_state_(nullptr), level_of_detail_(level_of_detail) {}

  virtual ~MockDumpProvider() {}

  MOCK_METHOD2(OnMemoryDump,
               bool(const MemoryDumpArgs& args, ProcessMemoryDump* pmd));

  // OnMemoryDump() override for the RespectTaskRunnerAffinity test.
  bool OnMemoryDump_CheckTaskRunner(const MemoryDumpArgs& args,
                                    ProcessMemoryDump* pmd) {
    EXPECT_TRUE(task_runner_->RunsTasksOnCurrentThread());
    return true;
  }

  // OnMemoryDump() override for the SharedSessionState test.
  bool OnMemoryDump_CheckSessionState(const MemoryDumpArgs& args,
                                      ProcessMemoryDump* pmd) {
    MemoryDumpSessionState* cur_session_state = pmd->session_state().get();
    if (last_session_state_)
      EXPECT_EQ(last_session_state_, cur_session_state);
    last_session_state_ = cur_session_state;
    return true;
  }

  // OnMemoryDump() override for the RegisterDumperWhileDumping test.
  bool OnMemoryDump_RegisterExtraDumpProvider(const MemoryDumpArgs& args,
                                              ProcessMemoryDump* pmd) {
    MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        dump_provider_to_register_or_unregister);
    return true;
  }

  // OnMemoryDump() override for the UnegisterDumperWhileDumping test.
  bool OnMemoryDump_UnregisterDumpProvider(const MemoryDumpArgs& args,
                                           ProcessMemoryDump* pmd) {
    MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
        dump_provider_to_register_or_unregister);
    return true;
  }

  // OnMemoryDump() override for the CheckMemoryDumpArgs test.
  bool OnMemoryDump_CheckMemoryDumpArgs(const MemoryDumpArgs& args,
                                        ProcessMemoryDump* pmd) {
    EXPECT_EQ(level_of_detail_, args.level_of_detail);
    return true;
  }

  // Used by OnMemoryDump_(Un)RegisterExtraDumpProvider.
  MemoryDumpProvider* dump_provider_to_register_or_unregister;

 private:
  MemoryDumpSessionState* last_session_state_;
  scoped_refptr<SingleThreadTaskRunner> task_runner_;
  const MemoryDumpArgs::LevelOfDetail level_of_detail_;
};

TEST_F(MemoryDumpManagerTest, SingleDumper) {
  MockDumpProvider mdp;
  mdm_->RegisterDumpProvider(&mdp);

  // Check that the dumper is not called if the memory category is not enabled.
  EnableTracing("foo-and-bar-but-not-memory");
  EXPECT_CALL(mdp, OnMemoryDump(_, _)).Times(0);
  mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                          high_detail_args);
  DisableTracing();

  // Now repeat enabling the memory category and check that the dumper is
  // invoked this time.
  EnableTracing(kTraceCategory);
  EXPECT_CALL(mdp, OnMemoryDump(_, _)).Times(3).WillRepeatedly(Return(true));
  for (int i = 0; i < 3; ++i)
    mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                            high_detail_args);
  DisableTracing();

  mdm_->UnregisterDumpProvider(&mdp);

  // Finally check the unregister logic (no calls to the mdp after unregister).
  EnableTracing(kTraceCategory);
  EXPECT_CALL(mdp, OnMemoryDump(_, _)).Times(0);
  mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                          high_detail_args);
  TraceLog::GetInstance()->SetDisabled();
}

TEST_F(MemoryDumpManagerTest, CheckMemoryDumpArgs) {
  // Check that requesting dumps with high level of detail actually propagates
  // to OnMemoryDump() call on dump providers.
  MockDumpProvider mdp_high_detail(MemoryDumpArgs::LevelOfDetail::HIGH);
  mdm_->RegisterDumpProvider(&mdp_high_detail);

  EnableTracing(kTraceCategory);
  EXPECT_CALL(mdp_high_detail, OnMemoryDump(_, _))
      .Times(1)
      .WillRepeatedly(
          Invoke(&mdp_high_detail,
                 &MockDumpProvider::OnMemoryDump_CheckMemoryDumpArgs));
  mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                          high_detail_args);
  DisableTracing();
  mdm_->UnregisterDumpProvider(&mdp_high_detail);

  // Check that requesting dumps with low level of detail actually propagates to
  // OnMemoryDump() call on dump providers.
  MockDumpProvider mdp_low_detail(MemoryDumpArgs::LevelOfDetail::LOW);
  mdm_->RegisterDumpProvider(&mdp_low_detail);

  EnableTracing(kTraceCategory);
  EXPECT_CALL(mdp_low_detail, OnMemoryDump(_, _))
      .Times(1)
      .WillRepeatedly(
          Invoke(&mdp_low_detail,
                 &MockDumpProvider::OnMemoryDump_CheckMemoryDumpArgs));
  mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                          low_detail_args);
  DisableTracing();
  mdm_->UnregisterDumpProvider(&mdp_low_detail);
}

TEST_F(MemoryDumpManagerTest, SharedSessionState) {
  MockDumpProvider mdp1;
  MockDumpProvider mdp2;
  mdm_->RegisterDumpProvider(&mdp1);
  mdm_->RegisterDumpProvider(&mdp2);

  EnableTracing(kTraceCategory);
  EXPECT_CALL(mdp1, OnMemoryDump(_, _))
      .Times(2)
      .WillRepeatedly(
          Invoke(&mdp1, &MockDumpProvider::OnMemoryDump_CheckSessionState));
  EXPECT_CALL(mdp2, OnMemoryDump(_, _))
      .Times(2)
      .WillRepeatedly(
          Invoke(&mdp2, &MockDumpProvider::OnMemoryDump_CheckSessionState));

  for (int i = 0; i < 2; ++i)
    mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                            high_detail_args);

  DisableTracing();
}

TEST_F(MemoryDumpManagerTest, MultipleDumpers) {
  MockDumpProvider mdp1;
  MockDumpProvider mdp2;

  // Enable only mdp1.
  mdm_->RegisterDumpProvider(&mdp1);
  EnableTracing(kTraceCategory);
  EXPECT_CALL(mdp1, OnMemoryDump(_, _)).Times(1).WillRepeatedly(Return(true));
  EXPECT_CALL(mdp2, OnMemoryDump(_, _)).Times(0);
  mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                          high_detail_args);
  DisableTracing();

  // Invert: enable mdp1 and disable mdp2.
  mdm_->UnregisterDumpProvider(&mdp1);
  mdm_->RegisterDumpProvider(&mdp2);
  EnableTracing(kTraceCategory);
  EXPECT_CALL(mdp1, OnMemoryDump(_, _)).Times(0);
  EXPECT_CALL(mdp2, OnMemoryDump(_, _)).Times(1).WillRepeatedly(Return(true));
  mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                          high_detail_args);
  DisableTracing();

  // Enable both mdp1 and mdp2.
  mdm_->RegisterDumpProvider(&mdp1);
  EnableTracing(kTraceCategory);
  EXPECT_CALL(mdp1, OnMemoryDump(_, _)).Times(1).WillRepeatedly(Return(true));
  EXPECT_CALL(mdp2, OnMemoryDump(_, _)).Times(1).WillRepeatedly(Return(true));
  mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                          high_detail_args);
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
    mdm_->RegisterDumpProvider(mdp, threads.back()->task_runner());
    EXPECT_CALL(*mdp, OnMemoryDump(_, _))
        .Times(i)
        .WillRepeatedly(
            Invoke(mdp, &MockDumpProvider::OnMemoryDump_CheckTaskRunner));
  }

  EnableTracing(kTraceCategory);

  while (!threads.empty()) {
    last_callback_success_ = false;
    {
      RunLoop run_loop;
      MemoryDumpCallback callback =
          Bind(&MemoryDumpManagerTest::DumpCallbackAdapter, Unretained(this),
               MessageLoop::current()->task_runner(), run_loop.QuitClosure());
      mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                              high_detail_args, callback);
      // This nested message loop (|run_loop|) will be quit if and only if
      // the RequestGlobalDump callback is invoked.
      run_loop.Run();
    }
    EXPECT_TRUE(last_callback_success_);

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

// Enable both dump providers, make sure that mdp gets disabled after 3 failures
// and not disabled after 1.
TEST_F(MemoryDumpManagerTest, DisableFailingDumpers) {
  MockDumpProvider mdp1;
  MockDumpProvider mdp2;

  mdm_->RegisterDumpProvider(&mdp1);
  mdm_->RegisterDumpProvider(&mdp2);
  EnableTracing(kTraceCategory);

  EXPECT_CALL(mdp1, OnMemoryDump(_, _))
      .Times(MemoryDumpManager::kMaxConsecutiveFailuresCount)
      .WillRepeatedly(Return(false));

  EXPECT_CALL(mdp2, OnMemoryDump(_, _))
      .Times(1 + MemoryDumpManager::kMaxConsecutiveFailuresCount)
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));
  for (int i = 0; i < 1 + MemoryDumpManager::kMaxConsecutiveFailuresCount;
       i++) {
    mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                            high_detail_args);
  }

  DisableTracing();
}

// Sneakily register an extra memory dump provider while an existing one is
// dumping and expect it to take part in the already active tracing session.
TEST_F(MemoryDumpManagerTest, RegisterDumperWhileDumping) {
  MockDumpProvider mdp1;
  MockDumpProvider mdp2;

  mdp1.dump_provider_to_register_or_unregister = &mdp2;
  mdm_->RegisterDumpProvider(&mdp1);
  EnableTracing(kTraceCategory);

  EXPECT_CALL(mdp1, OnMemoryDump(_, _))
      .Times(4)
      .WillOnce(Return(true))
      .WillOnce(Invoke(
          &mdp1, &MockDumpProvider::OnMemoryDump_RegisterExtraDumpProvider))
      .WillRepeatedly(Return(true));

  // Depending on the insertion order (before or after mdp1), mdp2 might be
  // called also immediately after it gets registered.
  EXPECT_CALL(mdp2, OnMemoryDump(_, _))
      .Times(Between(2, 3))
      .WillRepeatedly(Return(true));

  for (int i = 0; i < 4; i++) {
    mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                            high_detail_args);
  }

  DisableTracing();
}

// Like the above, but suddenly unregister the dump provider.
TEST_F(MemoryDumpManagerTest, UnregisterDumperWhileDumping) {
  MockDumpProvider mdp1;
  MockDumpProvider mdp2;

  mdm_->RegisterDumpProvider(&mdp1, ThreadTaskRunnerHandle::Get());
  mdm_->RegisterDumpProvider(&mdp2, ThreadTaskRunnerHandle::Get());
  mdp1.dump_provider_to_register_or_unregister = &mdp2;
  EnableTracing(kTraceCategory);

  EXPECT_CALL(mdp1, OnMemoryDump(_, _))
      .Times(4)
      .WillOnce(Return(true))
      .WillOnce(
          Invoke(&mdp1, &MockDumpProvider::OnMemoryDump_UnregisterDumpProvider))
      .WillRepeatedly(Return(true));

  // Depending on the insertion order (before or after mdp1), mdp2 might have
  // been already called when OnMemoryDump_UnregisterDumpProvider happens.
  EXPECT_CALL(mdp2, OnMemoryDump(_, _))
      .Times(Between(1, 2))
      .WillRepeatedly(Return(true));

  for (int i = 0; i < 4; i++) {
    mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                            high_detail_args);
  }

  DisableTracing();
}

// Ensures that a NACK callback is invoked if RequestGlobalDump is called when
// tracing is not enabled.
TEST_F(MemoryDumpManagerTest, CallbackCalledOnFailure) {
  MockDumpProvider mdp1;

  mdm_->RegisterDumpProvider(&mdp1);
  EXPECT_CALL(mdp1, OnMemoryDump(_, _)).Times(0);

  last_callback_success_ = true;
  {
    RunLoop run_loop;
    MemoryDumpCallback callback =
        Bind(&MemoryDumpManagerTest::DumpCallbackAdapter, Unretained(this),
             MessageLoop::current()->task_runner(), run_loop.QuitClosure());
    mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                            high_detail_args, callback);
    run_loop.Run();
  }
  EXPECT_FALSE(last_callback_success_);
}

}  // namespace trace_event
}  // namespace base
