// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_TEST_UTILS_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_TEST_UTILS_H_

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/safe_browsing_db/test_database_manager.h"
#include "content/public/browser/browser_thread.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "net/url_request/url_request_interceptor.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}  // namespace base

namespace net {
class URLRequest;
class NetworkDelegate;
}  // namespace net

namespace prerender {

namespace test_utils {

// Dummy counter class to live on the UI thread for counting requests.
class RequestCounter : public base::SupportsWeakPtr<RequestCounter> {
 public:
  RequestCounter();

  ~RequestCounter();

  int count() const { return count_; }

  void RequestStarted();
  void WaitForCount(int expected_count);

 private:
  int count_;
  int expected_count_;
  std::unique_ptr<base::RunLoop> loop_;
};

// A SafeBrowsingDatabaseManager implementation that returns a fixed result for
// a given URL.
class FakeSafeBrowsingDatabaseManager
    : public safe_browsing::TestSafeBrowsingDatabaseManager {
 public:
  FakeSafeBrowsingDatabaseManager();

  // Called on the IO thread to check if the given url is safe or not.  If we
  // can synchronously determine that the url is safe, CheckUrl returns true.
  // Otherwise it returns false, and "client" is called asynchronously with the
  // result when it is ready.
  // Returns true, indicating a SAFE result, unless the URL is the fixed URL
  // specified by the user, and the user-specified result is not SAFE
  // (in which that result will be communicated back via a call into the
  // client, and false will be returned).
  // Overrides SafeBrowsingDatabaseManager::CheckBrowseUrl.
  bool CheckBrowseUrl(const GURL& gurl, Client* client) override;

  void SetThreatTypeForUrl(const GURL& url,
                           safe_browsing::SBThreatType threat_type) {
    bad_urls_[url.spec()] = threat_type;
  }

  // These are called when checking URLs, so we implement them.
  bool IsSupported() const override;
  bool ChecksAreAlwaysAsync() const override;
  bool CanCheckResourceType(
      content::ResourceType /* resource_type */) const override;

  bool CheckExtensionIDs(const std::set<std::string>& extension_ids,
                         Client* client) override;

 private:
  ~FakeSafeBrowsingDatabaseManager() override;

  void OnCheckBrowseURLDone(const GURL& gurl, Client* client);

  std::unordered_map<std::string, safe_browsing::SBThreatType> bad_urls_;
  DISALLOW_COPY_AND_ASSIGN(FakeSafeBrowsingDatabaseManager);
};

// PrerenderContents that stops the UI message loop on DidStopLoading().
class TestPrerenderContents : public PrerenderContents {
 public:
  TestPrerenderContents(PrerenderManager* prerender_manager,
                        Profile* profile,
                        const GURL& url,
                        const content::Referrer& referrer,
                        Origin origin,
                        FinalStatus expected_final_status);

  ~TestPrerenderContents() override;

  void RenderProcessGone(base::TerminationStatus status) override;
  bool CheckURL(const GURL& url) override;

  // For tests that open the prerender in a new background tab, the RenderView
  // will not have been made visible when the PrerenderContents is destroyed
  // even though it is used.
  void set_should_be_shown(bool value) { should_be_shown_ = value; }

  // For tests which do not know whether the prerender will be used.
  void set_skip_final_checks(bool value) { skip_final_checks_ = value; }

  FinalStatus expected_final_status() const { return expected_final_status_; }

 private:
  void OnRenderViewHostCreated(
      content::RenderViewHost* new_render_view_host) override;
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  FinalStatus expected_final_status_;

  // The RenderViewHost created for the prerender, if any.
  content::RenderViewHost* new_render_view_host_;
  // Set to true when the prerendering RenderWidget is hidden.
  bool was_hidden_;
  // Set to true when the prerendering RenderWidget is shown, after having been
  // hidden.
  bool was_shown_;
  // Expected final value of was_shown_.  Defaults to true for
  // FINAL_STATUS_USED, and false otherwise.
  bool should_be_shown_;
  // If true, |expected_final_status_| and other shutdown checks are skipped.
  bool skip_final_checks_;
};

// A handle to a TestPrerenderContents whose lifetime is under the caller's
// control. A PrerenderContents may be destroyed at any point. This allows
// tracking the final status, etc.
class TestPrerender : public PrerenderContents::Observer,
                      public base::SupportsWeakPtr<TestPrerender> {
 public:
  TestPrerender();
  ~TestPrerender() override;

  TestPrerenderContents* contents() const { return contents_; }
  int number_of_loads() const { return number_of_loads_; }

  void WaitForCreate() { create_loop_.Run(); }
  void WaitForStart() { start_loop_.Run(); }
  void WaitForStop() { stop_loop_.Run(); }

  // Waits for |number_of_loads()| to be at least |expected_number_of_loads| OR
  // for the prerender to stop running (just to avoid a timeout if the prerender
  // dies). Note: this does not assert equality on the number of loads; the
  // caller must do it instead.
  void WaitForLoads(int expected_number_of_loads);

  void OnPrerenderCreated(TestPrerenderContents* contents);

  // PrerenderContents::Observer implementation:
  void OnPrerenderStart(PrerenderContents* contents) override;

  void OnPrerenderStopLoading(PrerenderContents* contents) override;

  void OnPrerenderStop(PrerenderContents* contents) override;

 private:
  TestPrerenderContents* contents_;
  int number_of_loads_;

  int expected_number_of_loads_;
  std::unique_ptr<base::RunLoop> load_waiter_;

  base::RunLoop create_loop_;
  base::RunLoop start_loop_;
  base::RunLoop stop_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestPrerender);
};

// PrerenderManager that uses TestPrerenderContents.
class TestPrerenderContentsFactory : public PrerenderContents::Factory {
 public:
  TestPrerenderContentsFactory();

  ~TestPrerenderContentsFactory() override;

  std::unique_ptr<TestPrerender> ExpectPrerenderContents(
      FinalStatus final_status);

  PrerenderContents* CreatePrerenderContents(
      PrerenderManager* prerender_manager,
      Profile* profile,
      const GURL& url,
      const content::Referrer& referrer,
      Origin origin) override;

 private:
  struct ExpectedContents {
    ExpectedContents();
    ExpectedContents(const ExpectedContents& other);
    ExpectedContents(FinalStatus final_status,
                     const base::WeakPtr<TestPrerender>& handle);
    ~ExpectedContents();

    FinalStatus final_status;
    base::WeakPtr<TestPrerender> handle;
  };

  std::deque<ExpectedContents> expected_contents_queue_;
};

class PrerenderInProcessBrowserTest : virtual public InProcessBrowserTest {
 public:
  PrerenderInProcessBrowserTest();

  ~PrerenderInProcessBrowserTest() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpInProcessBrowserTestFixture() override;
  void TearDownInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;
  content::SessionStorageNamespace* GetSessionStorageNamespace() const;

  bool UrlIsInPrerenderManager(const std::string& html_file) const;
  bool UrlIsInPrerenderManager(const GURL& url) const;

  // Convenience function to get the currently active WebContents in
  // current_browser().
  content::WebContents* GetActiveWebContents() const;

  PrerenderManager* GetPrerenderManager() const;

  TestPrerenderContents* GetPrerenderContentsFor(const GURL& url) const;

  std::unique_ptr<TestPrerender> PrerenderTestURL(
      const std::string& html_file,
      FinalStatus expected_final_status,
      int expected_number_of_loads);

  ScopedVector<TestPrerender> PrerenderTestURL(
      const std::string& html_file,
      const std::vector<FinalStatus>& expected_final_status_queue,
      int expected_number_of_loads);

  std::unique_ptr<TestPrerender> PrerenderTestURL(
      const GURL& url,
      FinalStatus expected_final_status,
      int expected_number_of_loads);

  safe_browsing::TestSafeBrowsingServiceFactory* safe_browsing_factory() const {
    return safe_browsing_factory_.get();
  }

  TestPrerenderContentsFactory* prerender_contents_factory() const {
    return prerender_contents_factory_;
  }

  void set_autostart_test_server(bool value) { autostart_test_server_ = value; }

  void set_browser(Browser* browser) { explicitly_set_browser_ = browser; }

  Browser* current_browser() const {
    return explicitly_set_browser_ ? explicitly_set_browser_ : browser();
  }

 private:
  // Implement load of a url for a prerender test. prerender_url should be
  // loaded, and we should expect to see one prerenderer created, and exit, for
  // each entry in expected_final_status_queue, and seeing
  // expected_number_of_loads. Specific tests can provide additional
  // verification. Note this should be called by one of the convenience wrappers
  // defined above.
  virtual ScopedVector<TestPrerender> PrerenderTestURLImpl(
      const GURL& prerender_url,
      const std::vector<FinalStatus>& expected_final_status_queue,
      int expected_number_of_loads) = 0;

  std::unique_ptr<ExternalProtocolHandler::Delegate>
      external_protocol_handler_delegate_;
  std::unique_ptr<safe_browsing::TestSafeBrowsingServiceFactory>
      safe_browsing_factory_;
  TestPrerenderContentsFactory* prerender_contents_factory_;
  Browser* explicitly_set_browser_;
  bool autostart_test_server_;
};

// Makes |url| respond to requests with the contents of |file|, counting the
// number that start in |counter|.
void CreateCountingInterceptorOnIO(
    const GURL& url,
    const base::FilePath& file,
    const base::WeakPtr<RequestCounter>& counter);

// Makes |url| respond to requests with the contents of |file|.
void CreateMockInterceptorOnIO(const GURL& url, const base::FilePath& file);

}  // namespace test_utils

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_TEST_UTILS_H_
