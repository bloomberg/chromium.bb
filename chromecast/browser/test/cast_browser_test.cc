// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/test/cast_browser_test.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/browser/cast_browser_context.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_content_window.h"
#include "chromecast/browser/cast_web_contents_manager.h"
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

void CastBrowserTest::SetUpCommandLine(base::CommandLine* command_line) {
  command_line->AppendSwitch(switches::kNoWifi);
  command_line->AppendSwitchASCII(switches::kTestType, "browser");
}

void CastBrowserTest::PreRunTestOnMainThread() {
  // Pump startup related events.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::RunLoop().RunUntilIdle();

  metrics::CastMetricsHelper::GetInstance()->SetDummySessionIdForTesting();
  web_contents_manager_ = base::MakeUnique<CastWebContentsManager>(
      CastBrowserProcess::GetInstance()->browser_context());
}

void CastBrowserTest::PostRunTestOnMainThread() {
  cast_web_view_.reset();
}

content::WebContents* CastBrowserTest::NavigateToURL(const GURL& url) {
  cast_web_view_ = web_contents_manager_->CreateWebView(
      this, nullptr /*site_instance*/, false /*transparent*/,
      false /*allow_media_access*/);

  content::WebContents* web_contents = cast_web_view_->web_contents();
  content::WaitForLoadStop(web_contents);
  content::TestNavigationObserver same_tab_observer(web_contents, 1);

  cast_web_view_->LoadUrl(url);

  same_tab_observer.Wait();

  return web_contents;
}

void CastBrowserTest::OnPageStopped(int reason) {}

void CastBrowserTest::OnLoadingStateChanged(bool loading) {}

void CastBrowserTest::OnWindowDestroyed() {}

void CastBrowserTest::OnKeyEvent(const ui::KeyEvent& key_event) {}

}  // namespace shell
}  // namespace chromecast
