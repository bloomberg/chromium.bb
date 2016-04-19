// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/content/content_favicon_driver.h"

#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/favicon/core/favicon_handler.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "net/base/load_flags.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/url_request.h"
#include "url/url_constants.h"

namespace {

// Tracks whether the URL passed to the constructor is requested and whether
// the request bypasses the cache.
class TestResourceDispatcherHostDelegate
    : public content::ResourceDispatcherHostDelegate {
 public:
  explicit TestResourceDispatcherHostDelegate(const GURL& url)
      : url_(url), was_requested_(false), bypassed_cache_(false) {}
  ~TestResourceDispatcherHostDelegate() override {}

  void Reset() {
    was_requested_ = false;
    bypassed_cache_ = false;
  }

  // Resturns whether |url_| was requested.
  bool was_requested() const { return was_requested_; }

  // Returns whether any of the requests bypassed the HTTP cache.
  bool bypassed_cache() const { return bypassed_cache_; }

 private:
  // content::ResourceDispatcherHostDelegate:
  bool ShouldBeginRequest(const std::string& method,
                          const GURL& url,
                          content::ResourceType resource_type,
                          content::ResourceContext* resource_context) override {
    return true;
  }

  void RequestBeginning(
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      content::AppCacheService* appcache_service,
      content::ResourceType resource_type,
      ScopedVector<content::ResourceThrottle>* throttles) override {
    if (request->url() == url_) {
      was_requested_ = true;
      if (request->load_flags() & net::LOAD_BYPASS_CACHE)
        bypassed_cache_ = true;
    }
  }

  void DownloadStarting(
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      int child_id,
      int route_id,
      int request_id,
      bool is_content_initiated,
      bool must_download,
      ScopedVector<content::ResourceThrottle>* throttles) override {}

 private:
  GURL url_;
  bool was_requested_;
  bool bypassed_cache_;

  DISALLOW_COPY_AND_ASSIGN(TestResourceDispatcherHostDelegate);
};

// Checks whether the FaviconDriver is waiting for a download to complete or
// for data from the FaviconService.
class FaviconDriverPendingTaskChecker {
 public:
  virtual ~FaviconDriverPendingTaskChecker() {}

  virtual bool HasPendingTasks() = 0;
};

// Waits for the following the finish:
// - The pending navigation.
// - FaviconHandler's pending favicon database requests.
// - FaviconHandler's pending downloads.
class PendingTaskWaiter : public content::NotificationObserver,
                          public favicon::FaviconDriverObserver {
 public:
  PendingTaskWaiter(content::WebContents* web_contents,
                    FaviconDriverPendingTaskChecker* checker)
      : checker_(checker),
        load_stopped_(false),
        scoped_observer_(this),
        weak_factory_(this) {
    registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                   content::Source<content::NavigationController>(
                       &web_contents->GetController()));
    scoped_observer_.Add(
        favicon::ContentFaviconDriver::FromWebContents(web_contents));
  }
  ~PendingTaskWaiter() override {}

  void Wait() {
    if (load_stopped_ && !checker_->HasPendingTasks())
      return;

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    if (type == content::NOTIFICATION_LOAD_STOP)
      load_stopped_ = true;

    OnNotification();
  }

  // favicon::Favicon
  void OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                        NotificationIconType notification_icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override {
    OnNotification();
  }

  void OnNotification() {
    if (!quit_closure_.is_null()) {
      // We stop waiting based on changes in state to FaviconHandler which occur
      // immediately after OnFaviconUpdated() is called. Post a task to check if
      // we can stop waiting.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(&PendingTaskWaiter::EndLoopIfCanStopWaiting,
                                weak_factory_.GetWeakPtr()));
    }
  }

  void EndLoopIfCanStopWaiting() {
    if (!quit_closure_.is_null() &&
        load_stopped_ &&
        !checker_->HasPendingTasks()) {
      quit_closure_.Run();
    }
  }

  FaviconDriverPendingTaskChecker* checker_;  // Not owned.
  bool load_stopped_;
  base::Closure quit_closure_;
  content::NotificationRegistrar registrar_;
  ScopedObserver<favicon::FaviconDriver, PendingTaskWaiter> scoped_observer_;
  base::WeakPtrFactory<PendingTaskWaiter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PendingTaskWaiter);
};

}  // namespace

class ContentFaviconDriverTest : public InProcessBrowserTest,
                                 public FaviconDriverPendingTaskChecker {
 public:
  ContentFaviconDriverTest() {}
  ~ContentFaviconDriverTest() override {}

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // FaviconDriverPendingTaskChecker:
  bool HasPendingTasks() override {
    return favicon::ContentFaviconDriver::FromWebContents(web_contents())
        ->HasPendingTasksForTest();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentFaviconDriverTest);
};

// Test that when a user reloads a page ignoring the cache that the favicon is
// is redownloaded and (not returned from either the favicon cache or the HTTP
// cache).
IN_PROC_BROWSER_TEST_F(ContentFaviconDriverTest, ReloadBypassingCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/favicon/page_with_favicon.html");
  GURL icon_url = embedded_test_server()->GetURL("/favicon/icon.png");

  std::unique_ptr<TestResourceDispatcherHostDelegate> delegate(
      new TestResourceDispatcherHostDelegate(icon_url));
  content::ResourceDispatcherHost::Get()->SetDelegate(delegate.get());

  // Initial visit in order to populate the cache.
  {
    PendingTaskWaiter waiter(web_contents(), this);
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, CURRENT_TAB, ui_test_utils::BROWSER_TEST_NONE);
    waiter.Wait();
  }
  ASSERT_TRUE(delegate->was_requested());
  EXPECT_FALSE(delegate->bypassed_cache());
  delegate->Reset();

  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // A normal visit should fetch the favicon from either the favicon database or
  // the HTTP cache.
  {
    PendingTaskWaiter waiter(web_contents(), this);
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, CURRENT_TAB, ui_test_utils::BROWSER_TEST_NONE);
    waiter.Wait();
  }
  EXPECT_FALSE(delegate->bypassed_cache());
  delegate->Reset();

  // A reload ignoring the cache should refetch the favicon from the website.
  {
    PendingTaskWaiter waiter(web_contents(), this);
    chrome::ExecuteCommand(browser(), IDC_RELOAD_BYPASSING_CACHE);
    waiter.Wait();
  }
  ASSERT_TRUE(delegate->was_requested());
  EXPECT_TRUE(delegate->bypassed_cache());
}
