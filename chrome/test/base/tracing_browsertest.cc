// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/tracing.h"

#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using tracing::BeginTracingWithWatch;
using tracing::WaitForWatchEvent;
using tracing::EndTracing;

const char* g_category = "test_tracing";
const char* g_event = "TheEvent";

class TracingBrowserTest : public InProcessBrowserTest {
 protected:
  // Execute some no-op javascript on the current tab - this triggers a trace
  // event in RenderViewImpl::OnScriptEvalRequest (from the renderer process).
  void ExecuteJavascriptOnCurrentTab() {
    content::RenderViewHost* rvh =
        chrome::GetActiveWebContents(browser())->GetRenderViewHost();
    ASSERT_TRUE(rvh);
    ASSERT_TRUE(content::ExecuteJavaScript(rvh, "", ";"));
  }
};

void AddEvents(int num) {
  for (int i = 0; i < num; ++i)
    TRACE_EVENT_INSTANT0(g_category, g_event);
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
  MessageLoop::current()->PostTask(FROM_HERE, base::Bind(&AddEvents, 1));
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
  MessageLoop::current()->PostTask(FROM_HERE, base::Bind(&AddEvents, 5));
  EXPECT_TRUE(WaitForWatchEvent(no_timeout));
  ASSERT_TRUE(EndTracing(&json_events));

  // Child process events from same process.
  ASSERT_TRUE(BeginTracingWithWatch(g_category, g_category,
                                    "OnScriptEvalRequest", 2));
  ASSERT_NO_FATAL_FAILURE(ExecuteJavascriptOnCurrentTab());
  ASSERT_NO_FATAL_FAILURE(ExecuteJavascriptOnCurrentTab());
  EXPECT_TRUE(WaitForWatchEvent(no_timeout));
  ASSERT_TRUE(EndTracing(&json_events));

  // Child process events from different processes.
  GURL url1("chrome://tracing/");
  GURL url2("chrome://credits/");
  ASSERT_TRUE(BeginTracingWithWatch(g_category, g_category,
                                    "OnScriptEvalRequest", 2));
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

}  // namespace
