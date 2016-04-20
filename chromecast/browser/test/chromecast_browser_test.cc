// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/test/chromecast_browser_test.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/browser/cast_browser_context.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/test/chromecast_browser_test_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"

namespace chromecast {
namespace shell {

ChromecastBrowserTest::ChromecastBrowserTest()
    : setup_called_(false) {
}

ChromecastBrowserTest::~ChromecastBrowserTest() {
  CHECK(setup_called_) << "Overridden SetUp() did not call parent "
                       << "implementation, so test not run.";
}

void ChromecastBrowserTest::SetUp() {
  SetUpCommandLine(base::CommandLine::ForCurrentProcess());
  setup_called_ = true;
  BrowserTestBase::SetUp();
}

void ChromecastBrowserTest::RunTestOnMainThreadLoop() {
  // Pump startup related events.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::RunLoop().RunUntilIdle();

  helper_ = ChromecastBrowserTestHelper::Create();
  metrics::CastMetricsHelper::GetInstance()->SetDummySessionIdForTesting();
  SetUpOnMainThread();

  RunTestOnMainThread();

  TearDownOnMainThread();

  helper_.reset();
}

}  // namespace shell
}  // namespace chromecast
