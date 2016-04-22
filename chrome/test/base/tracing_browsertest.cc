// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/tracing.h"

#include "base/command_line.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using base::trace_event::MemoryDumpManager;
using base::trace_event::MemoryDumpType;
using tracing::BeginTracingWithWatch;
using tracing::WaitForWatchEvent;
using tracing::EndTracing;

const char g_category[] = "test_tracing";
const char g_event[] = "TheEvent";

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

  void PerformDumpMemoryTestActions() {
    std::string json_events;
    base::TimeDelta no_timeout;

    GURL url1("about:blank");
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url1, NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
    ASSERT_NO_FATAL_FAILURE(ExecuteJavascriptOnCurrentTab());

    // Begin tracing and watch for multiple periodic dump trace events.
    std::string event_name = base::trace_event::MemoryDumpTypeToString(
        MemoryDumpType::PERIODIC_INTERVAL);
    ASSERT_TRUE(BeginTracingWithWatch(MemoryDumpManager::kTraceCategory,
                                      MemoryDumpManager::kTraceCategory,
                                      event_name, 10));

    // Create and destroy renderers while tracing is enabled.
    GURL url2("chrome://credits");
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url2, NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
    ASSERT_NO_FATAL_FAILURE(ExecuteJavascriptOnCurrentTab());

    // Close the current tab.
    browser()->tab_strip_model()->CloseSelectedTabs();

    GURL url3("chrome://settings");
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url3, CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
    ASSERT_NO_FATAL_FAILURE(ExecuteJavascriptOnCurrentTab());

    EXPECT_TRUE(WaitForWatchEvent(no_timeout));
    ASSERT_TRUE(EndTracing(&json_events));

    // Expect the basic memory dumps to be present in the trace.
    EXPECT_NE(std::string::npos, json_events.find("process_totals"));

    EXPECT_NE(std::string::npos, json_events.find("v8"));
    EXPECT_NE(std::string::npos, json_events.find("blink_gc"));
  }
};

class SingleProcessTracingBrowserTest : public TracingBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kSingleProcess);
  }
};

void AddEvents(int num) {
  for (int i = 0; i < num; ++i)
    TRACE_EVENT_INSTANT0(g_category, g_event, TRACE_EVENT_SCOPE_THREAD);
}

IN_PROC_BROWSER_TEST_F(TracingBrowserTest, BeginTracingWithWatch) {
  base::TimeDelta no_timeout;
  base::TimeDelta short_timeout = base::TimeDelta::FromMilliseconds(5);
  std::string json_events;

  // One event before wait.
  ASSERT_TRUE(BeginTracingWithWatch(g_category, g_category, g_event, 1));
  AddEvents(1);
  EXPECT_TRUE(WaitForWatchEvent(no_timeout));
  ASSERT_TRUE(EndTracing(&json_events));

  // One event after wait.
  ASSERT_TRUE(BeginTracingWithWatch(g_category, g_category, g_event, 1));
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(&AddEvents, 1));
  EXPECT_TRUE(WaitForWatchEvent(no_timeout));
  ASSERT_TRUE(EndTracing(&json_events));

  // Not enough events timeout.
  ASSERT_TRUE(BeginTracingWithWatch(g_category, g_category, g_event, 2));
  AddEvents(1);
  EXPECT_FALSE(WaitForWatchEvent(short_timeout));
  ASSERT_TRUE(EndTracing(&json_events));

  // Multi event before wait.
  ASSERT_TRUE(BeginTracingWithWatch(g_category, g_category, g_event, 5));
  AddEvents(5);
  EXPECT_TRUE(WaitForWatchEvent(no_timeout));
  ASSERT_TRUE(EndTracing(&json_events));

  // Multi event after wait.
  ASSERT_TRUE(BeginTracingWithWatch(g_category, g_category, g_event, 5));
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(&AddEvents, 5));
  EXPECT_TRUE(WaitForWatchEvent(no_timeout));
  ASSERT_TRUE(EndTracing(&json_events));

  // Child process events from same process.
  ASSERT_TRUE(BeginTracingWithWatch(g_category, g_category,
                                    "OnJavaScriptExecuteRequestForTests", 2));
  ASSERT_NO_FATAL_FAILURE(ExecuteJavascriptOnCurrentTab());
  ASSERT_NO_FATAL_FAILURE(ExecuteJavascriptOnCurrentTab());
  EXPECT_TRUE(WaitForWatchEvent(no_timeout));
  ASSERT_TRUE(EndTracing(&json_events));

  // Child process events from different processes.
  GURL url1("chrome://tracing/");
  GURL url2("chrome://credits/");
  ASSERT_TRUE(BeginTracingWithWatch(g_category, g_category,
                                    "OnJavaScriptExecuteRequestForTests", 2));
  // Open two tabs to different URLs to encourage two separate renderer
  // processes. Each will fire an event that will be counted towards the total.
  ui_test_utils::NavigateToURLWithDisposition(browser(), url1,
      NEW_FOREGROUND_TAB, ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ASSERT_NO_FATAL_FAILURE(ExecuteJavascriptOnCurrentTab());
  ui_test_utils::NavigateToURLWithDisposition(browser(), url2,
      NEW_FOREGROUND_TAB, ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ASSERT_NO_FATAL_FAILURE(ExecuteJavascriptOnCurrentTab());
  EXPECT_TRUE(WaitForWatchEvent(no_timeout));
  ASSERT_TRUE(EndTracing(&json_events));
}

// Multi-process mode.
// Flaky on win and Linux: https://crbug.com/594884.
IN_PROC_BROWSER_TEST_F(TracingBrowserTest, DISABLED_TestMemoryInfra) {
  PerformDumpMemoryTestActions();
}

// Single-process mode.
// Flaky on Windows and Linux: https://crbug.com/594884
// Linux ASan: https://crbug.com/585026
IN_PROC_BROWSER_TEST_F(SingleProcessTracingBrowserTest,
    DISABLED_TestMemoryInfra) {
  PerformDumpMemoryTestActions();
}

}  // namespace
