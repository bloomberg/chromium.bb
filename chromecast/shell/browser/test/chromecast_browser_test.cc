// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/shell/browser/test/chromecast_browser_test.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "chromecast/shell/browser/cast_browser_context.h"
#include "chromecast/shell/browser/cast_browser_process.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"

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
  SetUpCommandLine(CommandLine::ForCurrentProcess());
  setup_called_ = true;
  BrowserTestBase::SetUp();
}

void ChromecastBrowserTest::RunTestOnMainThreadLoop() {
  // Pump startup related events.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::RunLoop().RunUntilIdle();

  SetUpOnMainThread();

  RunTestOnMainThread();

  TearDownOnMainThread();

  for (content::RenderProcessHost::iterator i(
           content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->FastShutdownIfPossible();
  }

  web_contents_.reset();
}

void ChromecastBrowserTest::NavigateToURL(content::WebContents* window,
                                          const GURL& url) {
  content::WaitForLoadStop(window);
  content::TestNavigationObserver same_tab_observer(window, 1);
  content::NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED |
      ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  window->GetController().LoadURLWithParams(params);
  same_tab_observer.Wait();
}

content::WebContents* ChromecastBrowserTest::CreateBrowser() {
  content::WebContents::CreateParams create_params(
      CastBrowserProcess::GetInstance()->browser_context(),
      NULL);
  create_params.routing_id = MSG_ROUTING_NONE;
  create_params.initial_size = gfx::Size(1280, 720);
  web_contents_.reset(content::WebContents::Create(create_params));
  return web_contents_.get();
}

}  // namespace shell
}  // namespace chromecast
