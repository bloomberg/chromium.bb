// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/tracing.h"

#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using tracing::BeginTracingWithWatch;
using tracing::WaitForWatchEvent;
using tracing::EndTracing;

const char* g_category = "test_tracing";
const char* g_event = "TheEvent";

class TracingBrowserTest : public InProcessBrowserTest {
};

void AddEvents(int num) {
  for (int i = 0; i < num; ++i)
    TRACE_EVENT_INSTANT0(g_category, g_event);
}

// This test times out on various builders. See http://crbug.com/146255 .
IN_PROC_BROWSER_TEST_F(TracingBrowserTest, DISABLED_BeginTracingWithWatch) {
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
  GURL url1("chrome://tracing");
  ASSERT_TRUE(BeginTracingWithWatch(g_category, g_category,
                                    "RenderViewImpl::OnNavigate", 2));
  ui_test_utils::NavigateToURL(browser(), url1);
  ui_test_utils::NavigateToURL(browser(), url1);
  EXPECT_TRUE(WaitForWatchEvent(no_timeout));
  ASSERT_TRUE(EndTracing(&json_events));

  // Child process events from different processes.
  GURL url2("about:blank");
  ASSERT_TRUE(BeginTracingWithWatch(g_category, g_category,
                                    "RenderViewImpl::OnNavigate", 2));
  // Open two tabs to different URLs to encourage two separate renderer
  // processes. Each will fire an event that will be counted towards the total.
  ui_test_utils::NavigateToURL(browser(), url1);
  ui_test_utils::NavigateToURLWithDisposition(browser(), url2,
      NEW_FOREGROUND_TAB, ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_TRUE(WaitForWatchEvent(no_timeout));
  ASSERT_TRUE(EndTracing(&json_events));
}

}  // namespace
