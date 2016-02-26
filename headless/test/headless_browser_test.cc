// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/test/headless_browser_test.h"

#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/headless_content_main_delegate.h"

namespace headless {

HeadlessBrowserTest::HeadlessBrowserTest() {
  base::FilePath headless_test_data(FILE_PATH_LITERAL("headless/test/data"));
  CreateTestServer(headless_test_data);
}

HeadlessBrowserTest::~HeadlessBrowserTest() {}

void HeadlessBrowserTest::SetUpOnMainThread() {}

void HeadlessBrowserTest::TearDownOnMainThread() {
  browser()->Shutdown();
}

void HeadlessBrowserTest::RunTestOnMainThreadLoop() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Pump startup related events.
  base::MessageLoop::current()->RunUntilIdle();

  SetUpOnMainThread();
  RunTestOnMainThread();
  TearDownOnMainThread();

  for (content::RenderProcessHost::iterator i(
           content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->FastShutdownIfPossible();
  }
}

HeadlessBrowser* HeadlessBrowserTest::browser() const {
  return HeadlessContentMainDelegate::GetInstance()->browser();
}

}  // namespace headless
