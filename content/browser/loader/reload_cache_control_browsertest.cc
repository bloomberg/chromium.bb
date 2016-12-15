// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

const char kReloadTestPath[] = "/loader/reload.html";
const char kReloadImagePath[] = "/loader/empty16x16.png";

const char kNoCacheControl[] = "";
const char kMaxAgeCacheControl[] = "max-age=0";
const char kNoCacheCacheControl[] = "no-cache";

struct RequestLog {
  std::string relative_url;
  std::string cache_control;
};

// Tests end to end behaviors between Blink and content around reload variants.
class ReloadCacheControlBrowserTest : public ContentBrowserTest {
 protected:
  ReloadCacheControlBrowserTest() {}
  ~ReloadCacheControlBrowserTest() override = default;

  void SetUpOnMainThread() override {
    SetUpTestServerOnMainThread();
  }

  void SetUpTestServerOnMainThread() {
    // ContentBrowserTest creates embedded_test_server instance with
    // a registered HandleFileRequest for "content/test/data".
    // Because the handler is registered as the first handler, MonitorHandler
    // is needed to capture all requests.
    embedded_test_server()->RegisterRequestMonitor(base::Bind(
        &ReloadCacheControlBrowserTest::MonitorRequestHandler,
        base::Unretained(this)));

    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  std::vector<RequestLog> request_log_;
  base::Lock request_log_lock_;

 private:
  void MonitorRequestHandler(const HttpRequest& request) {
    RequestLog log;
    log.relative_url = request.relative_url;
    auto cache_control = request.headers.find("Cache-Control");
    log.cache_control = cache_control == request.headers.end()
                            ? kNoCacheControl
                            : cache_control->second;
    base::AutoLock lock(request_log_lock_);
    request_log_.push_back(log);
  }

  DISALLOW_COPY_AND_ASSIGN(ReloadCacheControlBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ReloadCacheControlBrowserTest, NormalReload) {
  GURL url(embedded_test_server()->GetURL(kReloadTestPath));

  EXPECT_TRUE(NavigateToURL(shell(), url));
  ReloadBlockUntilNavigationsComplete(shell(), 1);

  {
    base::AutoLock lock(request_log_lock_);
    ASSERT_EQ(4UL, request_log_.size());
    EXPECT_EQ(kReloadTestPath, request_log_[0].relative_url);
    EXPECT_EQ(kNoCacheControl, request_log_[0].cache_control);
    EXPECT_EQ(kReloadImagePath, request_log_[1].relative_url);
    EXPECT_EQ(kNoCacheControl, request_log_[1].cache_control);

    EXPECT_EQ(kReloadTestPath, request_log_[2].relative_url);
    EXPECT_EQ(kMaxAgeCacheControl, request_log_[2].cache_control);
    EXPECT_EQ(kReloadImagePath, request_log_[3].relative_url);
    EXPECT_EQ(kNoCacheControl, request_log_[3].cache_control);
  }

  shell()->ShowDevTools();
  ReloadBlockUntilNavigationsComplete(shell(), 1);

  {
    base::AutoLock lock(request_log_lock_);
    ASSERT_EQ(6UL, request_log_.size());
    EXPECT_EQ(kReloadTestPath, request_log_[4].relative_url);
    EXPECT_EQ(kMaxAgeCacheControl, request_log_[4].cache_control);
    EXPECT_EQ(kReloadImagePath, request_log_[5].relative_url);
    EXPECT_EQ(kNoCacheControl, request_log_[5].cache_control);
  }

  shell()->CloseDevTools();
  ReloadBlockUntilNavigationsComplete(shell(), 1);

  {
    base::AutoLock lock(request_log_lock_);
    ASSERT_EQ(8UL, request_log_.size());
    EXPECT_EQ(kReloadTestPath, request_log_[6].relative_url);
    EXPECT_EQ(kMaxAgeCacheControl, request_log_[6].cache_control);
    EXPECT_EQ(kReloadImagePath, request_log_[7].relative_url);
    EXPECT_EQ(kNoCacheControl, request_log_[7].cache_control);
  }
}

IN_PROC_BROWSER_TEST_F(ReloadCacheControlBrowserTest, BypassingReload) {
  GURL url(embedded_test_server()->GetURL(kReloadTestPath));

  EXPECT_TRUE(NavigateToURL(shell(), url));
  ReloadBypassingCacheBlockUntilNavigationsComplete(shell(), 1);

  {
    base::AutoLock lock(request_log_lock_);
    ASSERT_EQ(4UL, request_log_.size());
    EXPECT_EQ(kReloadTestPath, request_log_[0].relative_url);
    EXPECT_EQ(kNoCacheControl, request_log_[0].cache_control);
    EXPECT_EQ(kReloadImagePath, request_log_[1].relative_url);
    EXPECT_EQ(kNoCacheControl, request_log_[1].cache_control);

    EXPECT_EQ(kReloadTestPath, request_log_[2].relative_url);
    EXPECT_EQ(kNoCacheCacheControl, request_log_[2].cache_control);
    EXPECT_EQ(kReloadImagePath, request_log_[3].relative_url);
    EXPECT_EQ(kNoCacheCacheControl, request_log_[3].cache_control);
  }

  shell()->ShowDevTools();
  ReloadBypassingCacheBlockUntilNavigationsComplete(shell(), 1);

  {
    base::AutoLock lock(request_log_lock_);
    ASSERT_EQ(6UL, request_log_.size());
    EXPECT_EQ(kReloadTestPath, request_log_[4].relative_url);
    EXPECT_EQ(kNoCacheCacheControl, request_log_[4].cache_control);
    EXPECT_EQ(kReloadImagePath, request_log_[5].relative_url);
    EXPECT_EQ(kNoCacheCacheControl, request_log_[5].cache_control);
  }

  shell()->CloseDevTools();
  ReloadBypassingCacheBlockUntilNavigationsComplete(shell(), 1);

  {
    base::AutoLock lock(request_log_lock_);
    ASSERT_EQ(8UL, request_log_.size());
    EXPECT_EQ(kReloadTestPath, request_log_[6].relative_url);
    EXPECT_EQ(kNoCacheCacheControl, request_log_[6].cache_control);
    EXPECT_EQ(kReloadImagePath, request_log_[7].relative_url);
    EXPECT_EQ(kNoCacheCacheControl, request_log_[7].cache_control);
  }
}

IN_PROC_BROWSER_TEST_F(ReloadCacheControlBrowserTest, NavigateToSame) {
  GURL url(embedded_test_server()->GetURL(kReloadTestPath));

  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // The first navigation is just a normal load.
  {
    base::AutoLock lock(request_log_lock_);
    ASSERT_EQ(4UL, request_log_.size());
    EXPECT_EQ(kReloadTestPath, request_log_[0].relative_url);
    EXPECT_EQ(kNoCacheControl, request_log_[0].cache_control);
    EXPECT_EQ(kReloadImagePath, request_log_[1].relative_url);
    EXPECT_EQ(kNoCacheControl, request_log_[1].cache_control);
  }

  // TODO(crbug.com/671545): This test does not work correctly if browser-side
  // navigation is enabled.
  if (IsBrowserSideNavigationEnabled())
    return;

  // The second navigation is the same page navigation. This should be handled
  // as a reload, revalidating the main resource, but following cache protocols
  // for others.
  {
    base::AutoLock lock(request_log_lock_);
    EXPECT_EQ(kReloadTestPath, request_log_[2].relative_url);
    EXPECT_EQ(kMaxAgeCacheControl, request_log_[2].cache_control);
    EXPECT_EQ(kReloadImagePath, request_log_[3].relative_url);
    EXPECT_EQ(kNoCacheControl, request_log_[3].cache_control);
  }

  shell()->ShowDevTools();
  EXPECT_TRUE(NavigateToURL(shell(), url));

  {
    base::AutoLock lock(request_log_lock_);
    ASSERT_EQ(6UL, request_log_.size());
    EXPECT_EQ(kReloadTestPath, request_log_[4].relative_url);
    EXPECT_EQ(kMaxAgeCacheControl, request_log_[4].cache_control);
    EXPECT_EQ(kReloadImagePath, request_log_[5].relative_url);
    EXPECT_EQ(kNoCacheControl, request_log_[5].cache_control);
  }

  shell()->CloseDevTools();
  EXPECT_TRUE(NavigateToURL(shell(), url));

  {
    base::AutoLock lock(request_log_lock_);
    ASSERT_EQ(8UL, request_log_.size());
    EXPECT_EQ(kReloadTestPath, request_log_[6].relative_url);
    EXPECT_EQ(kMaxAgeCacheControl, request_log_[6].cache_control);
    EXPECT_EQ(kReloadImagePath, request_log_[7].relative_url);
    EXPECT_EQ(kNoCacheControl, request_log_[7].cache_control);
  }
}

}  // namespace

}  // namespace content
