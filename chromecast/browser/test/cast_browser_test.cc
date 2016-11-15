// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/test/cast_browser_test.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/browser/cast_browser_context.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_content_window.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"

namespace chromecast {
namespace shell {

CastBrowserTest::CastBrowserTest() {}

CastBrowserTest::~CastBrowserTest() {}

void CastBrowserTest::SetUp() {
  SetUpCommandLine(base::CommandLine::ForCurrentProcess());

  BrowserTestBase::SetUp();
}

void CastBrowserTest::TearDownOnMainThread() {
  web_contents_.reset();
  window_.reset();

  BrowserTestBase::TearDownOnMainThread();
}

void CastBrowserTest::SetUpCommandLine(base::CommandLine* command_line) {
  BrowserTestBase::SetUpCommandLine(command_line);

  command_line->AppendSwitch(switches::kNoWifi);
  command_line->AppendSwitchASCII(switches::kTestType, "browser");
}

void CastBrowserTest::RunTestOnMainThreadLoop() {
  // Pump startup related events.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::RunLoop().RunUntilIdle();

  metrics::CastMetricsHelper::GetInstance()->SetDummySessionIdForTesting();

  SetUpOnMainThread();
  RunTestOnMainThread();
  TearDownOnMainThread();
}

content::WebContents* CastBrowserTest::NavigateToURL(const GURL& url) {
  window_ = base::MakeUnique<CastContentWindow>();

  web_contents_ = window_->CreateWebContents(
      CastBrowserProcess::GetInstance()->browser_context());
  window_->CreateWindowTree(web_contents_.get());
  content::WaitForLoadStop(web_contents_.get());

  content::TestNavigationObserver same_tab_observer(web_contents_.get(), 1);
  content::NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  web_contents_->GetController().LoadURLWithParams(params);
  same_tab_observer.Wait();

  return web_contents_.get();
}

}  // namespace shell
}  // namespace chromecast
