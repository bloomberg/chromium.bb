// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_dump_manager.h"

#include "base/bind_helpers.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_io_thread.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/process_memory_dump.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::AtMost;
using testing::Between;
using testing::Invoke;
using testing::Return;

namespace base {
namespace trace_event {
namespace {
MemoryDumpArgs g_high_detail_args = {MemoryDumpArgs::LevelOfDetail::HIGH};
MemoryDumpArgs g_low_detail_args = {MemoryDumpArgs::LevelOfDetail::LOW};
}  // namespace

// GTest matchers for MemoryDumpRequestArgs arguments.
MATCHER(IsHighDetail, "") {
  return arg.level_of_detail == MemoryDumpArgs::LevelOfDetail::HIGH;
}

MATCHER(IsLowDetail, "") {
  return arg.level_of_detail == MemoryDumpArgs::LevelOfDetail::LOW;
}

// GTest matchers for MemoryDumpArgs arguments.
MATCHER(IsHighDetailArgs, "") {
  return arg.dump_args.level_of_detail == MemoryDumpArgs::LevelOfDetail::HIGH;
}

MATCHER(IsLowDetailArgs, "") {
  return arg.dump_args.level_of_detail == MemoryDumpArgs::LevelOfDetail::LOW;
}

// Testing MemoryDumpManagerDelegate which, by default, short-circuits dump
// requests locally to the MemoryDumpManager instead of performing IPC dances.
class MemoryDumpManagerDelegateForTesting : public MemoryDumpManagerDelegate {
 public:
  MemoryDumpManagerDelegateForTesting() {
    EXPECT_CALL(*this, IsCoordinatorProcess()).WillRepeatedly(Return(false));
    ON_CALL(*this, RequestGlobalMemoryDump(_, _))
        .WillByDefault(Invoke(
            this, &MemoryDumpManagerDelegateForTesting::CreateProcessDump));
  }

  MOCK_METHOD2(RequestGlobalMemoryDump,
               void(const MemoryDumpRequestArgs& args,
                    const MemoryDumpCallback& callback));

  MOCK_CONST_METHOD0(IsCoordinatorProcess, bool());

  uint64 GetTracingProcessId() const override {
    NOTREACHED();
    return MemoryDumpManager::kInvalidTracingProcessId;
  }
};

class MockMemoryDumpProvider : public MemoryDumpProvider {
 public:
  MOCK_METHOD2(OnMemoryDump,
               bool(const MemoryDumpArgs& args, ProcessMemoryDump* pmd));
};

class MemoryDumpManagerTest : public testing::Test {
 public:
  void SetUp() override {
    last_callback_success_ = false;
    message_loop_.reset(new MessageLoop());
    mdm_.reset(new MemoryDumpManager());
    MemoryDumpManager::SetInstanceForTesting(mdm_.get());
    ASSERT_EQ(mdm_, MemoryDumpManager::GetInstance());
    mdm_->Initialize();
    delegate_.reset(new MemoryDumpManagerDelegateForTesting);
    mdm_->SetDelegate(delegate_.get());
  }

  void TearDown() override {
    MemoryDumpManager::SetInstanceForTesting(nullptr);
    mdm_.reset();
    delegate_.reset();
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
  // This enables tracing using the legacy category filter string.
  void EnableTracingWithLegacyCategories(const char* category) {
    TraceLog::GetInstance()->SetEnabled(TraceConfig(category, ""),
                                        TraceLog::RECORDING_MODE);
  }

  void EnableTracingWithTraceConfig(const char* trace_config) {
    TraceLog::GetInstance()->SetEnabled(TraceConfig(trace_config),
                                        TraceLog::RECORDING_MODE);
  }

  void DisableTracing() { TraceLog::GetInstance()->SetDisabled(); }

  bool IsPeriodicDumpingEnabled() const {
    return mdm_->periodic_dump_timer_.IsRunning();
  }

  scoped_ptr<MemoryDumpManager> mdm_;
  scoped_ptr<MemoryDumpManagerDelegateForTesting> delegate_;
  bool last_callback_success_;

 private:
  scoped_ptr<MessageLoop> message_loop_;

  // We want our singleton torn down after each test.
  ShadowingAtExitManager at_exit_manager_;
};

// Basic sanity checks. Registers a memory dump provider and checks that it is
// called, but only when memory-infra is enabled.
TEST_F(MemoryDumpManagerTest, SingleDumper) {
  MockMemoryDumpProvider mdp;
  mdm_->RegisterDumpProvider(&mdp);

  // Check that the dumper is not called if the memory category is not enabled.
  EnableTracingWithLegacyCategories("foobar-but-not-memory");
  EXPECT_CALL(*delegate_, RequestGlobalMemoryDump(_, _)).Times(0);
  EXPECT_CALL(mdp, OnMemoryDump(_, _)).Times(0);
  mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                          g_high_detail_args);
  DisableTracing();

  // Now repeat enabling the memory category and check that the dumper is
  // invoked this time.
  EnableTracingWithLegacyCategories(MemoryDumpManager::kTraceCategory);
  EXPECT_CALL(*delegate_, RequestGlobalMemoryDump(_, _)).Times(3);
  EXPECT_CALL(mdp, OnMemoryDump(_, _)).Times(3).WillRepeatedly(Return(true));
  for (int i = 0; i < 3; ++i)
    mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                            g_high_detail_args);
  DisableTracing();

  mdm_->UnregisterDumpProvider(&mdp);

  // Finally check the unregister logic: the delegate will be invoked but not
  // the dump provider, as it has been unregistered.
  EnableTracingWithLegacyCategories(MemoryDumpManager::kTraceCategory);
  EXPECT_CALL(*delegate_, RequestGlobalMemoryDump(_, _)).Times(1);
  EXPECT_CALL(mdp, OnMemoryDump(_, _)).Times(0);
  mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                          g_high_detail_args);
  TraceLog::GetInstance()->SetDisabled();
}

// Checks that requesting dumps with high level of detail actually propagates
// the level of the detail properly to OnMemoryDump() call on dump providers.
TEST_F(MemoryDumpManagerTest, CheckMemoryDumpArgs) {
  MockMemoryDumpProvider mdp;

  mdm_->RegisterDumpProvider(&mdp);
  EnableTracingWithLegacyCategories(MemoryDumpManager::kTraceCategory);
  EXPECT_CALL(*delegate_, RequestGlobalMemoryDump(_, _)).Times(1);
  EXPECT_CALL(mdp, OnMemoryDump(IsHighDetail(), _)).WillOnce(Return(true));
  mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                          g_high_detail_args);
  DisableTracing();
  mdm_->UnregisterDumpProvider(&mdp);

  // Check that requesting dumps with low level of detail actually propagates to
  // OnMemoryDump() call on dump providers.
  mdm_->RegisterDumpProvider(&mdp);
  EnableTracingWithLegacyCategories(MemoryDumpManager::kTraceCategory);
  EXPECT_CALL(*delegate_, RequestGlobalMemoryDump(_, _)).Times(1);
  EXPECT_CALL(mdp, OnMemoryDump(IsLowDetail(), _)).WillOnce(Return(true));
  mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                          g_low_detail_args);
  DisableTracing();
  mdm_->UnregisterDumpProvider(&mdp);
}

// Checks that the SharedSessionState object is acqually shared over time.
TEST_F(MemoryDumpManagerTest, SharedSessionState) {
  MockMemoryDumpProvider mdp1;
  MockMemoryDumpProvider mdp2;
  mdm_->RegisterDumpProvider(&mdp1);
  mdm_->RegisterDumpProvider(&mdp2);

  EnableTracingWithLegacyCategories(MemoryDumpManager::kTraceCategory);
  const MemoryDumpSessionState* session_state = mdm_->session_state().get();
  EXPECT_CALL(*delegate_, RequestGlobalMemoryDump(_, _)).Times(2);
  EXPECT_CALL(mdp1, OnMemoryDump(_, _))
      .Times(2)
      .WillRepeatedly(Invoke([session_state](const MemoryDumpArgs&,
                                             ProcessMemoryDump* pmd) -> bool {
        EXPECT_EQ(session_state, pmd->session_state().get());
        return true;
      }));
  EXPECT_CALL(mdp2, OnMemoryDump(_, _))
      .Times(2)
      .WillRepeatedly(Invoke([session_state](const MemoryDumpArgs&,
                                             ProcessMemoryDump* pmd) -> bool {
        EXPECT_EQ(session_state, pmd->session_state().get());
        return true;
      }));

  for (int i = 0; i < 2; ++i)
    mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                            g_high_detail_args);

  DisableTracing();
}

// Checks that the (Un)RegisterDumpProvider logic behaves sanely.
TEST_F(MemoryDumpManagerTest, MultipleDumpers) {
  MockMemoryDumpProvider mdp1;
  MockMemoryDumpProvider mdp2;

  // Enable only mdp1.
  mdm_->RegisterDumpProvider(&mdp1);
  EnableTracingWithLegacyCategories(MemoryDumpManager::kTraceCategory);
  EXPECT_CALL(*delegate_, RequestGlobalMemoryDump(_, _)).Times(1);
  EXPECT_CALL(mdp1, OnMemoryDump(_, _)).WillOnce(Return(true));
  EXPECT_CALL(mdp2, OnMemoryDump(_, _)).Times(0);
  mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                          g_high_detail_args);
  DisableTracing();

  // Invert: enable mdp1 and disable mdp2.
  mdm_->UnregisterDumpProvider(&mdp1);
  mdm_->RegisterDumpProvider(&mdp2);
  EnableTracingWithLegacyCategories(MemoryDumpManager::kTraceCategory);
  EXPECT_CALL(*delegate_, RequestGlobalMemoryDump(_, _)).Times(1);
  EXPECT_CALL(mdp1, OnMemoryDump(_, _)).Times(0);
  EXPECT_CALL(mdp2, OnMemoryDump(_, _)).WillOnce(Return(true));
  mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                          g_high_detail_args);
  DisableTracing();

  // Enable both mdp1 and mdp2.
  mdm_->RegisterDumpProvider(&mdp1);
  EnableTracingWithLegacyCategories(MemoryDumpManager::kTraceCategory);
  EXPECT_CALL(*delegate_, RequestGlobalMemoryDump(_, _)).Times(1);
  EXPECT_CALL(mdp1, OnMemoryDump(_, _)).WillOnce(Return(true));
  EXPECT_CALL(mdp2, OnMemoryDump(_, _)).WillOnce(Return(true));
  mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                          g_high_detail_args);
  DisableTracing();
}

// Checks that the dump provider invocations depend only on the current
// registration state and not on previous registrations and dumps.
TEST_F(MemoryDumpManagerTest, RegistrationConsistency) {
  MockMemoryDumpProvider mdp;

  mdm_->RegisterDumpProvider(&mdp);

  {
    EXPECT_CALL(*delegate_, RequestGlobalMemoryDump(_, _)).Times(1);
    EXPECT_CALL(mdp, OnMemoryDump(_, _)).WillOnce(Return(true));
    EnableTracingWithLegacyCategories(MemoryDumpManager::kTraceCategory);
    mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                            g_high_detail_args);
    DisableTracing();
  }

  mdm_->UnregisterDumpProvider(&mdp);

  {
    EXPECT_CALL(*delegate_, RequestGlobalMemoryDump(_, _)).Times(1);
    EXPECT_CALL(mdp, OnMemoryDump(_, _)).Times(0);
    EnableTracingWithLegacyCategories(MemoryDumpManager::kTraceCategory);
    mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                            g_high_detail_args);
    DisableTracing();
  }

  mdm_->RegisterDumpProvider(&mdp);
  mdm_->UnregisterDumpProvider(&mdp);

  {
    EXPECT_CALL(*delegate_, RequestGlobalMemoryDump(_, _)).Times(1);
    EXPECT_CALL(mdp, OnMemoryDump(_, _)).Times(0);
    EnableTracingWithLegacyCategories(MemoryDumpManager::kTraceCategory);
    mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                            g_high_detail_args);
    DisableTracing();
  }

  mdm_->RegisterDumpProvider(&mdp);
  mdm_->UnregisterDumpProvider(&mdp);
  mdm_->RegisterDumpProvider(&mdp);

  {
    EXPECT_CALL(*delegate_, RequestGlobalMemoryDump(_, _)).Times(1);
    EXPECT_CALL(mdp, OnMemoryDump(_, _)).WillOnce(Return(true));
    EnableTracingWithLegacyCategories(MemoryDumpManager::kTraceCategory);
    mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                            g_high_detail_args);
    DisableTracing();
  }
}

// Checks that the MemoryDumpManager respects the thread affinity when a
// MemoryDumpProvider specifies a task_runner(). The test starts creating 8
// threads and registering a MemoryDumpProvider on each of them. At each
// iteration, one thread is removed, to check the live unregistration logic.
TEST_F(MemoryDumpManagerTest, RespectTaskRunnerAffinity) {
  const uint32 kNumInitialThreads = 8;

  ScopedVector<Thread> threads;
  ScopedVector<MockMemoryDumpProvider> mdps;

  // Create the threads and setup the expectations. Given that at each iteration
  // we will pop out one thread/MemoryDumpProvider, each MDP is supposed to be
  // invoked a number of times equal to its index.
  for (uint32 i = kNumInitialThreads; i > 0; --i) {
    Thread* thread = new Thread("test thread");
    threads.push_back(thread);
    threads.back()->Start();
    scoped_refptr<SingleThreadTaskRunner> task_runner = thread->task_runner();
    MockMemoryDumpProvider* mdp = new MockMemoryDumpProvider();
    mdps.push_back(mdp);
    mdm_->RegisterDumpProvider(mdp, task_runner);
    EXPECT_CALL(*mdp, OnMemoryDump(_, _))
        .Times(i)
        .WillRepeatedly(Invoke(
            [task_runner](const MemoryDumpArgs&, ProcessMemoryDump*) -> bool {
              EXPECT_TRUE(task_runner->RunsTasksOnCurrentThread());
              return true;
            }));
  }

  EnableTracingWithLegacyCategories(MemoryDumpManager::kTraceCategory);

  while (!threads.empty()) {
    last_callback_success_ = false;
    {
      RunLoop run_loop;
      MemoryDumpCallback callback =
          Bind(&MemoryDumpManagerTest::DumpCallbackAdapter, Unretained(this),
               MessageLoop::current()->task_runner(), run_loop.QuitClosure());
      EXPECT_CALL(*delegate_, RequestGlobalMemoryDump(_, _)).Times(1);
      mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                              g_high_detail_args, callback);
      // This nested message loop (|run_loop|) will quit if and only if the
      // |callback| passed to RequestGlobalDump() is invoked.
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

// Checks that providers get disabled after 3 consecutive failures, but not
// otherwise (e.g., if interleaved).
TEST_F(MemoryDumpManagerTest, DisableFailingDumpers) {
  MockMemoryDumpProvider mdp1;
  MockMemoryDumpProvider mdp2;

  mdm_->RegisterDumpProvider(&mdp1);
  mdm_->RegisterDumpProvider(&mdp2);
  EnableTracingWithLegacyCategories(MemoryDumpManager::kTraceCategory);

  const int kNumDumps = 2 * MemoryDumpManager::kMaxConsecutiveFailuresCount;
  EXPECT_CALL(*delegate_, RequestGlobalMemoryDump(_, _)).Times(kNumDumps);

  EXPECT_CALL(mdp1, OnMemoryDump(_, _))
      .Times(MemoryDumpManager::kMaxConsecutiveFailuresCount)
      .WillRepeatedly(Return(false));

  EXPECT_CALL(mdp2, OnMemoryDump(_, _))
      .WillOnce(Return(false))
      .WillOnce(Return(true))
      .WillOnce(Return(false))
      .WillOnce(Return(false))
      .WillOnce(Return(true))
      .WillOnce(Return(false));

  for (int i = 0; i < kNumDumps; i++) {
    mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                            g_high_detail_args);
  }

  DisableTracing();
}

// Sneakily registers an extra memory dump provider while an existing one is
// dumping and expect it to take part in the already active tracing session.
TEST_F(MemoryDumpManagerTest, RegisterDumperWhileDumping) {
  MockMemoryDumpProvider mdp1;
  MockMemoryDumpProvider mdp2;

  mdm_->RegisterDumpProvider(&mdp1);
  EnableTracingWithLegacyCategories(MemoryDumpManager::kTraceCategory);

  EXPECT_CALL(*delegate_, RequestGlobalMemoryDump(_, _)).Times(4);

  EXPECT_CALL(mdp1, OnMemoryDump(_, _))
      .Times(4)
      .WillOnce(Return(true))
      .WillOnce(
          Invoke([&mdp2](const MemoryDumpArgs&, ProcessMemoryDump*) -> bool {
            MemoryDumpManager::GetInstance()->RegisterDumpProvider(&mdp2);
            return true;
          }))
      .WillRepeatedly(Return(true));

  // Depending on the insertion order (before or after mdp1), mdp2 might be
  // called also immediately after it gets registered.
  EXPECT_CALL(mdp2, OnMemoryDump(_, _))
      .Times(Between(2, 3))
      .WillRepeatedly(Return(true));

  for (int i = 0; i < 4; i++) {
    mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                            g_high_detail_args);
  }

  DisableTracing();
}

// Like RegisterDumperWhileDumping, but unregister the dump provider instead.
TEST_F(MemoryDumpManagerTest, UnregisterDumperWhileDumping) {
  MockMemoryDumpProvider mdp1;
  MockMemoryDumpProvider mdp2;

  mdm_->RegisterDumpProvider(&mdp1, ThreadTaskRunnerHandle::Get());
  mdm_->RegisterDumpProvider(&mdp2, ThreadTaskRunnerHandle::Get());
  EnableTracingWithLegacyCategories(MemoryDumpManager::kTraceCategory);

  EXPECT_CALL(*delegate_, RequestGlobalMemoryDump(_, _)).Times(4);

  EXPECT_CALL(mdp1, OnMemoryDump(_, _))
      .Times(4)
      .WillOnce(Return(true))
      .WillOnce(
          Invoke([&mdp2](const MemoryDumpArgs&, ProcessMemoryDump*) -> bool {
            MemoryDumpManager::GetInstance()->UnregisterDumpProvider(&mdp2);
            return true;
          }))
      .WillRepeatedly(Return(true));

  // Depending on the insertion order (before or after mdp1), mdp2 might have
  // been already called when UnregisterDumpProvider happens.
  EXPECT_CALL(mdp2, OnMemoryDump(_, _))
      .Times(Between(1, 2))
      .WillRepeatedly(Return(true));

  for (int i = 0; i < 4; i++) {
    mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                            g_high_detail_args);
  }

  DisableTracing();
}

// Checks that the dump does not abort when unregistering a provider while
// dumping from a different thread than the dumping thread.
TEST_F(MemoryDumpManagerTest, UnregisterDumperFromThreadWhileDumping) {
  ScopedVector<TestIOThread> threads;
  ScopedVector<MockMemoryDumpProvider> mdps;

  for (int i = 0; i < 2; i++) {
    threads.push_back(new TestIOThread(TestIOThread::kAutoStart));
    mdps.push_back(new MockMemoryDumpProvider());
    mdm_->RegisterDumpProvider(mdps.back(), threads.back()->task_runner());
  }

  int on_memory_dump_call_count = 0;
  RunLoop run_loop;

  // When OnMemoryDump is called on either of the dump providers, it will
  // unregister the other one.
  for (MockMemoryDumpProvider* mdp : mdps) {
    int other_idx = (mdps.front() == mdp);
    TestIOThread* other_thread = threads[other_idx];
    MockMemoryDumpProvider* other_mdp = mdps[other_idx];
    auto on_dump = [this, other_thread, other_mdp, &on_memory_dump_call_count](
        const MemoryDumpArgs& args, ProcessMemoryDump* pmd) {
      other_thread->PostTaskAndWait(
          FROM_HERE, base::Bind(&MemoryDumpManager::UnregisterDumpProvider,
                                base::Unretained(&*mdm_), other_mdp));
      on_memory_dump_call_count++;
      return true;
    };

    // OnMemoryDump is called once for the provider that dumps first, and zero
    // times for the other provider.
    EXPECT_CALL(*mdp, OnMemoryDump(_, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(on_dump));
  }

  last_callback_success_ = false;
  MemoryDumpCallback callback =
      Bind(&MemoryDumpManagerTest::DumpCallbackAdapter, Unretained(this),
           MessageLoop::current()->task_runner(), run_loop.QuitClosure());

  EnableTracingWithLegacyCategories(MemoryDumpManager::kTraceCategory);
  MemoryDumpRequestArgs request_args = {0, MemoryDumpType::EXPLICITLY_TRIGGERED,
                                        g_high_detail_args};
  mdm_->CreateProcessDump(request_args, callback);

  run_loop.Run();

  ASSERT_EQ(1, on_memory_dump_call_count);
  ASSERT_EQ(true, last_callback_success_);

  DisableTracing();
}

// Checks that a NACK callback is invoked if RequestGlobalDump() is called when
// tracing is not enabled.
TEST_F(MemoryDumpManagerTest, CallbackCalledOnFailure) {
  MockMemoryDumpProvider mdp1;
  mdm_->RegisterDumpProvider(&mdp1);

  EXPECT_CALL(*delegate_, RequestGlobalMemoryDump(_, _)).Times(0);
  EXPECT_CALL(mdp1, OnMemoryDump(_, _)).Times(0);

  last_callback_success_ = true;
  {
    RunLoop run_loop;
    MemoryDumpCallback callback =
        Bind(&MemoryDumpManagerTest::DumpCallbackAdapter, Unretained(this),
             MessageLoop::current()->task_runner(), run_loop.QuitClosure());
    mdm_->RequestGlobalDump(MemoryDumpType::EXPLICITLY_TRIGGERED,
                            g_high_detail_args, callback);
    run_loop.Run();
  }
  EXPECT_FALSE(last_callback_success_);
}

// This test crystallizes the expectations of the chrome://tracing UI and
// chrome telemetry w.r.t. periodic dumps in memory-infra, handling gracefully
// the transition between the legacy and the new-style (JSON-based) TraceConfig.
TEST_F(MemoryDumpManagerTest, TraceConfigExpectations) {
  MemoryDumpManagerDelegateForTesting& delegate = *delegate_;

  // Don't trigger the default behavior of the mock delegate in this test,
  // which would short-circuit the dump request to the actual
  // CreateProcessDump().
  // We don't want to create any dump in this test, only check whether the dumps
  // are requested or not.
  ON_CALL(delegate, RequestGlobalMemoryDump(_, _)).WillByDefault(Return());

  // Enabling memory-infra in a non-coordinator process should not trigger any
  // periodic dumps.
  EXPECT_CALL(delegate, IsCoordinatorProcess()).WillRepeatedly(Return(false));
  EnableTracingWithLegacyCategories(MemoryDumpManager::kTraceCategory);
  EXPECT_FALSE(IsPeriodicDumpingEnabled());
  DisableTracing();

  // Enabling memory-infra with the legacy TraceConfig (category filter) in
  // a coordinator process should enable periodic dumps.
  EXPECT_CALL(delegate, IsCoordinatorProcess()).WillRepeatedly(Return(true));
  EnableTracingWithLegacyCategories(MemoryDumpManager::kTraceCategory);
  EXPECT_TRUE(IsPeriodicDumpingEnabled());
  DisableTracing();

  // Enabling memory-infra with the new (JSON) TraceConfig in a coordinator
  // process without specifying any "memory_dump_config" section should enable
  // periodic dumps. This is to preserve the behavior chrome://tracing UI, that
  // is: ticking memory-infra should dump periodically with the default config.
  EXPECT_CALL(delegate, IsCoordinatorProcess()).WillRepeatedly(Return(true));
  const std::string kTraceConfigWithNoMemorDumpConfig = StringPrintf(
      "{\"included_categories\":[\"%s\"]}", MemoryDumpManager::kTraceCategory);
  EnableTracingWithTraceConfig(kTraceConfigWithNoMemorDumpConfig.c_str());
  EXPECT_TRUE(IsPeriodicDumpingEnabled());
  DisableTracing();

  // Enabling memory-infra with the new (JSON) TraceConfig in a coordinator
  // process with an empty "memory_dump_config" should NOT enable periodic
  // dumps. This is the way telemetry is supposed to use memory-infra with
  // only explicitly triggered dumps.
  EXPECT_CALL(delegate, IsCoordinatorProcess()).WillRepeatedly(Return(true));
  const std::string kTraceConfigWithNoTriggers = StringPrintf(
      "{\"included_categories\":[\"%s\"], \"memory_dump_config\":{}",
      MemoryDumpManager::kTraceCategory);
  EnableTracingWithTraceConfig(kTraceConfigWithNoTriggers.c_str());
  EXPECT_FALSE(IsPeriodicDumpingEnabled());
  DisableTracing();

  // Enabling memory-infra with the new (JSON) TraceConfig in a non-coordinator
  // process with a fully defined trigger config should NOT enable any periodic
  // dumps.
  EXPECT_CALL(delegate, IsCoordinatorProcess()).WillRepeatedly(Return(false));
  const std::string kTraceConfigWithTriggers = StringPrintf(
    "{\"included_categories\":[\"%s\"],"
     "\"memory_dump_config\":{"
        "\"triggers\":["
          "{\"mode\":\"light\", \"periodic_interval_ms\":1},"
          "{\"mode\":\"detailed\", \"periodic_interval_ms\":5}"
        "]"
      "}"
    "}", MemoryDumpManager::kTraceCategory);
  EnableTracingWithTraceConfig(kTraceConfigWithTriggers.c_str());
  EXPECT_FALSE(IsPeriodicDumpingEnabled());
  DisableTracing();

  // Enabling memory-infra with the new (JSON) TraceConfig in a coordinator
  // process with a fully defined trigger config should cause periodic dumps to
  // be performed in the correct order.
  EXPECT_CALL(delegate, IsCoordinatorProcess()).WillRepeatedly(Return(true));

  RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();

  // The expected sequence with config of light=1ms, heavy=5ms is H,L,L,L,H,...
  testing::InSequence sequence;
  EXPECT_CALL(delegate, RequestGlobalMemoryDump(IsHighDetailArgs(), _));
  EXPECT_CALL(delegate, RequestGlobalMemoryDump(IsLowDetailArgs(), _)).Times(3);
  EXPECT_CALL(delegate, RequestGlobalMemoryDump(IsHighDetailArgs(), _));
  EXPECT_CALL(delegate, RequestGlobalMemoryDump(IsLowDetailArgs(), _)).Times(2);
  EXPECT_CALL(delegate, RequestGlobalMemoryDump(IsLowDetailArgs(), _))
      .WillOnce(Invoke([quit_closure](const MemoryDumpRequestArgs& args,
                                      const MemoryDumpCallback& callback) {
        ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, quit_closure);
      }));

  // Swallow all the final spurious calls until tracing gets disabled.
  EXPECT_CALL(delegate, RequestGlobalMemoryDump(_, _)).Times(AnyNumber());

  EnableTracingWithTraceConfig(kTraceConfigWithTriggers.c_str());
  run_loop.Run();
  DisableTracing();
}

}  // namespace trace_event
}  // namespace base
