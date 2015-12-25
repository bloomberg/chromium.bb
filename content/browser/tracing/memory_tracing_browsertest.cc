// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/trace_config_memory_test_util.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::trace_event::MemoryDumpArgs;
using base::trace_event::MemoryDumpManager;
using base::trace_event::MemoryDumpType;
using base::trace_event::ProcessMemoryDump;
using testing::_;
using testing::Return;

namespace content {

// A mock dump provider, used to check that dump requests actually end up
// creating memory dumps.
class MockDumpProvider : public base::trace_event::MemoryDumpProvider {
 public:
  MOCK_METHOD2(OnMemoryDump, bool(const MemoryDumpArgs& args,
                                  ProcessMemoryDump* pmd));
};

class MemoryTracingTest : public ContentBrowserTest {
 public:
  void DoRequestGlobalDump(const base::trace_event::MemoryDumpCallback& cb) {
    MemoryDumpManager::GetInstance()->RequestGlobalDump(
        MemoryDumpType::EXPLICITLY_TRIGGERED,
        base::trace_event::MemoryDumpLevelOfDetail::DETAILED, cb);
  }

  // Used as callback argument for MemoryDumpManager::RequestGlobalDump():
  void OnGlobalMemoryDumpDone(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::Closure closure,
      uint64_t dump_guid,
      bool success) {
    // Make sure we run the RunLoop closure on the same thread that originated
    // the run loop (which is the IN_PROC_BROWSER_TEST_F main thread).
    if (!task_runner->RunsTasksOnCurrentThread()) {
      task_runner->PostTask(
          FROM_HERE, base::Bind(&MemoryTracingTest::OnGlobalMemoryDumpDone,
                                base::Unretained(this), task_runner, closure,
                                dump_guid, success));
      return;
    }
    ++callback_call_count_;
    last_callback_dump_guid_ = dump_guid;
    last_callback_success_ = success;
    closure.Run();
  }

 protected:
  void SetUp() override {
    callback_call_count_ = 0;
    last_callback_dump_guid_ = 0;
    last_callback_success_ = false;

    mock_dump_provider_.reset(new MockDumpProvider());
    MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        mock_dump_provider_.get(), "MockDumpProvider", nullptr);
    MemoryDumpManager::GetInstance()
        ->set_dumper_registrations_ignored_for_testing(false);
    ContentBrowserTest::SetUp();
  }

  void TearDown() override {
    MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
        mock_dump_provider_.get());
    mock_dump_provider_.reset();
    ContentBrowserTest::TearDown();
  }

  void EnableMemoryTracing() {
    // Enable tracing without periodic dumps.
    base::trace_event::TraceConfig trace_config(
        base::trace_event::TraceConfigMemoryTestUtil::
            GetTraceConfig_EmptyTriggers());

    base::RunLoop run_loop;
    bool success = TracingController::GetInstance()->StartTracing(
      trace_config, run_loop.QuitClosure());
    EXPECT_TRUE(success);
    run_loop.Run();
  }

  void DisableTracing() {
    bool success = TracingController::GetInstance()->StopTracing(NULL);
    EXPECT_TRUE(success);
    base::RunLoop().RunUntilIdle();
  }

  void RequestGlobalDumpAndWait(bool from_renderer_thread) {
    base::RunLoop run_loop;
    base::trace_event::MemoryDumpCallback callback = base::Bind(
        &MemoryTracingTest::OnGlobalMemoryDumpDone, base::Unretained(this),
        base::ThreadTaskRunnerHandle::Get(), run_loop.QuitClosure());
    if (from_renderer_thread) {
      PostTaskToInProcessRendererAndWait(
          base::Bind(&MemoryTracingTest::DoRequestGlobalDump,
                     base::Unretained(this), callback));
    } else {
      DoRequestGlobalDump(callback);
    }
    run_loop.Run();
  }

  void Navigate(Shell* shell) {
    NavigateToURL(shell, GetTestUrl("", "title.html"));
  }

  base::Closure on_memory_dump_complete_closure_;
  scoped_ptr<MockDumpProvider> mock_dump_provider_;
  uint32_t callback_call_count_;
  uint64_t last_callback_dump_guid_;
  bool last_callback_success_;
};

// Ignore SingleProcessMemoryTracingTests for Google Chrome builds because
// single-process is not supported on those builds.
#if !defined(GOOGLE_CHROME_BUILD)

class SingleProcessMemoryTracingTest : public MemoryTracingTest {
 public:
  SingleProcessMemoryTracingTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kSingleProcess);
  }
};

// Checks that a memory dump initiated from a the main browser thread ends up in
// a single dump even in single process mode.
IN_PROC_BROWSER_TEST_F(SingleProcessMemoryTracingTest,
                       BrowserInitiatedSingleDump) {
  Navigate(shell());

  EXPECT_CALL(*mock_dump_provider_, OnMemoryDump(_,_)).WillOnce(Return(true));

  EnableMemoryTracing();
  RequestGlobalDumpAndWait(false /* from_renderer_thread */);
  EXPECT_EQ(1u, callback_call_count_);
  EXPECT_NE(0u, last_callback_dump_guid_);
  EXPECT_TRUE(last_callback_success_);
  DisableTracing();
}

// Checks that a memory dump initiated from a renderer thread ends up in a
// single dump even in single process mode.
IN_PROC_BROWSER_TEST_F(SingleProcessMemoryTracingTest,
                       RendererInitiatedSingleDump) {
  Navigate(shell());

  EXPECT_CALL(*mock_dump_provider_, OnMemoryDump(_,_)).WillOnce(Return(true));

  EnableMemoryTracing();
  RequestGlobalDumpAndWait(true /* from_renderer_thread */);
  EXPECT_EQ(1u, callback_call_count_);
  EXPECT_NE(0u, last_callback_dump_guid_);
  EXPECT_TRUE(last_callback_success_);
  DisableTracing();
}

IN_PROC_BROWSER_TEST_F(SingleProcessMemoryTracingTest, ManyInterleavedDumps) {
  Navigate(shell());

  EXPECT_CALL(*mock_dump_provider_, OnMemoryDump(_,_))
      .Times(4)
      .WillRepeatedly(Return(true));

  EnableMemoryTracing();

  RequestGlobalDumpAndWait(true /* from_renderer_thread */);
  EXPECT_NE(0u, last_callback_dump_guid_);
  EXPECT_TRUE(last_callback_success_);

  RequestGlobalDumpAndWait(false /* from_renderer_thread */);
  EXPECT_NE(0u, last_callback_dump_guid_);
  EXPECT_TRUE(last_callback_success_);

  RequestGlobalDumpAndWait(false /* from_renderer_thread */);
  EXPECT_NE(0u, last_callback_dump_guid_);
  EXPECT_TRUE(last_callback_success_);

  RequestGlobalDumpAndWait(true /* from_renderer_thread */);
  EXPECT_EQ(4u, callback_call_count_);
  EXPECT_NE(0u, last_callback_dump_guid_);
  EXPECT_TRUE(last_callback_success_);

  DisableTracing();
}

#endif  // !defined(GOOGLE_CHROME_BUILD)

// Non-deterministic races under TSan. crbug.com/529678
#if defined(THREAD_SANITIZER)
#define MAYBE_BrowserInitiatedDump DISABLED_BrowserInitiatedDump
#else
#define MAYBE_BrowserInitiatedDump BrowserInitiatedDump
#endif
// Checks that a memory dump initiated from a the main browser thread ends up in
// a successful dump.
IN_PROC_BROWSER_TEST_F(MemoryTracingTest, MAYBE_BrowserInitiatedDump) {
  Navigate(shell());

  EXPECT_CALL(*mock_dump_provider_, OnMemoryDump(_,_)).WillOnce(Return(true));

  EnableMemoryTracing();
  RequestGlobalDumpAndWait(false /* from_renderer_thread */);
  EXPECT_EQ(1u, callback_call_count_);
  EXPECT_NE(0u, last_callback_dump_guid_);
  EXPECT_TRUE(last_callback_success_);
  DisableTracing();
}

}  // namespace content
