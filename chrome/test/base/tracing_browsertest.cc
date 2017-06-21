// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/tracing.h"

#include "base/command_line.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_config_memory_test_util.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using base::trace_event::MemoryDumpManager;
using base::trace_event::MemoryDumpType;
using tracing::BeginTracingWithTraceConfig;
using tracing::EndTracing;

void RequestGlobalDumpCallback(base::Closure quit_closure,
                               bool success,
                               uint64_t) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, quit_closure);
  ASSERT_TRUE(success);
}

void OnStartTracingDoneCallback(
    base::trace_event::MemoryDumpLevelOfDetail explicit_dump_type,
    base::Closure quit_closure) {
  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestGlobalDumpAndAppendToTrace(
          MemoryDumpType::EXPLICITLY_TRIGGERED, explicit_dump_type,
          Bind(&RequestGlobalDumpCallback, quit_closure));
}

class TracingBrowserTest : public InProcessBrowserTest {
 protected:
  // Execute some no-op javascript on the current tab - this triggers a trace
  // event in RenderFrameImpl::OnJavaScriptExecuteRequestForTests (from the
  // renderer process).
  void ExecuteJavascriptOnCurrentTab() {
    content::RenderViewHost* rvh = browser()->tab_strip_model()->
        GetActiveWebContents()->GetRenderViewHost();
    ASSERT_TRUE(rvh);
    ASSERT_TRUE(content::ExecuteScript(rvh, ";"));
  }

  void PerformDumpMemoryTestActions(
      const base::trace_event::TraceConfig& trace_config,
      base::trace_event::MemoryDumpLevelOfDetail explicit_dump_type) {
    GURL url1("about:blank");
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
    ASSERT_NO_FATAL_FAILURE(ExecuteJavascriptOnCurrentTab());

    // Begin tracing and trigger dump once start is broadcasted to all
    // processes.
    base::RunLoop run_loop;
    ASSERT_TRUE(BeginTracingWithTraceConfig(
        trace_config, Bind(&OnStartTracingDoneCallback, explicit_dump_type,
                           run_loop.QuitClosure())));

    // Create and destroy renderers while tracing is enabled.
    GURL url2("chrome://credits");
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
    ASSERT_NO_FATAL_FAILURE(ExecuteJavascriptOnCurrentTab());

    // Close the current tab.
    browser()->tab_strip_model()->CloseSelectedTabs();

    GURL url3("chrome://chrome-urls");
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url3, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
    ASSERT_NO_FATAL_FAILURE(ExecuteJavascriptOnCurrentTab());

    run_loop.Run();
    std::string json_events;
    ASSERT_TRUE(EndTracing(&json_events));

    // Expect the basic memory dumps to be present in the trace.
    EXPECT_NE(std::string::npos, json_events.find("process_totals"));
    EXPECT_NE(std::string::npos, json_events.find("v8"));
    EXPECT_NE(std::string::npos, json_events.find("blink_gc"));
  }
};

IN_PROC_BROWSER_TEST_F(TracingBrowserTest, TestMemoryInfra) {
  PerformDumpMemoryTestActions(
      base::trace_event::TraceConfig(
          base::trace_event::TraceConfigMemoryTestUtil::
              GetTraceConfig_EmptyTriggers()),
      base::trace_event::MemoryDumpLevelOfDetail::DETAILED);
}

// crbug.com/708487.
#if defined(OS_MACOSX) && defined(ADDRESS_SANITIZER)
#define MAYBE_TestBackgroundMemoryInfra DISABLED_TestBackgroundMemoryInfra
#else
#define MAYBE_TestBackgroundMemoryInfra TestBackgroundMemoryInfra
#endif  // defined(OS_MACOSX) && defined(ADDRESS_SANITIZER)
IN_PROC_BROWSER_TEST_F(TracingBrowserTest, MAYBE_TestBackgroundMemoryInfra) {
  PerformDumpMemoryTestActions(
      base::trace_event::TraceConfig(
          base::trace_event::TraceConfigMemoryTestUtil::
              GetTraceConfig_BackgroundTrigger(200)),
      base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND);
}

}  // namespace
