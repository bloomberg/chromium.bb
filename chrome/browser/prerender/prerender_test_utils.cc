// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_test_utils.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/loader/chrome_resource_dispatcher_host_delegate.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/local_database_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/ppapi_test_utils.h"
#include "net/base/load_flags.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/url_request/url_request_filter.h"
#include "ppapi/shared_impl/ppapi_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;
using content::RenderViewHost;

namespace prerender {

namespace test_utils {

namespace {

// Wrapper over URLRequestMockHTTPJob that exposes extra callbacks.
class MockHTTPJob : public net::URLRequestMockHTTPJob {
 public:
  MockHTTPJob(net::URLRequest* request,
              net::NetworkDelegate* delegate,
              const base::FilePath& file)
      : net::URLRequestMockHTTPJob(
            request,
            delegate,
            file,
            BrowserThread::GetBlockingPool()->GetTaskRunnerWithShutdownBehavior(
                base::SequencedWorkerPool::SKIP_ON_SHUTDOWN)) {}

  void set_start_callback(const base::Closure& start_callback) {
    start_callback_ = start_callback;
  }

  void Start() override {
    if (!start_callback_.is_null())
      start_callback_.Run();
    net::URLRequestMockHTTPJob::Start();
  }

 private:
  ~MockHTTPJob() override {}

  base::Closure start_callback_;
};

// URLRequestInterceptor which counts the number of requests that start.
class CountingInterceptor : public net::URLRequestInterceptor {
 public:
  CountingInterceptor(const base::FilePath& file,
                      const base::WeakPtr<RequestCounter>& counter)
      : file_(file), counter_(counter), weak_factory_(this) {}
  ~CountingInterceptor() override {}

  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    MockHTTPJob* job = new MockHTTPJob(request, network_delegate, file_);
    job->set_start_callback(base::Bind(&CountingInterceptor::RequestStarted,
                                       weak_factory_.GetWeakPtr()));
    return job;
  }

  void RequestStarted() {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&RequestCounter::RequestStarted, counter_));
  }

 private:
  base::FilePath file_;
  base::WeakPtr<RequestCounter> counter_;
  mutable base::WeakPtrFactory<CountingInterceptor> weak_factory_;
};

class CountingInterceptorWithCallback : public net::URLRequestInterceptor {
 public:
  // Inserts the interceptor object to intercept requests to |url|.  Can be
  // called on any thread. Assumes that |counter| lives on the UI thread.  The
  // |callback_io| will be called on IO thread with the net::URLrequest
  // provided.
  static void Initialize(const GURL& url,
                         RequestCounter* counter,
                         base::Callback<void(net::URLRequest*)> callback_io) {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&CountingInterceptorWithCallback::CreateAndAddOnIO, url,
                   counter->AsWeakPtr(), callback_io));
  }

  // net::URLRequestInterceptor:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    // Run the callback.
    callback_.Run(request);

    // Ping the request counter.
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&RequestCounter::RequestStarted, counter_));
    return nullptr;
  }

 private:
  CountingInterceptorWithCallback(
      const base::WeakPtr<RequestCounter>& counter,
      base::Callback<void(net::URLRequest*)> callback)
      : callback_(callback), counter_(counter) {}

  static void CreateAndAddOnIO(
      const GURL& url,
      const base::WeakPtr<RequestCounter>& counter,
      base::Callback<void(net::URLRequest*)> callback_io) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    // Create the object with base::WrapUnique to restrict access to the
    // constructor.
    net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
        url, base::WrapUnique(
                 new CountingInterceptorWithCallback(counter, callback_io)));
  }

  base::Callback<void(net::URLRequest*)> callback_;
  base::WeakPtr<RequestCounter> counter_;

  DISALLOW_COPY_AND_ASSIGN(CountingInterceptorWithCallback);
};

// URLRequestJob (and associated handler) which hangs.
class HangingURLRequestJob : public net::URLRequestJob {
 public:
  HangingURLRequestJob(net::URLRequest* request,
                          net::NetworkDelegate* network_delegate)
      : net::URLRequestJob(request, network_delegate) {
  }

  void Start() override {}

 private:
  ~HangingURLRequestJob() override {}
};

class HangingFirstRequestInterceptor : public net::URLRequestInterceptor {
 public:
  HangingFirstRequestInterceptor(const base::FilePath& file,
                                 base::Closure callback)
      : file_(file),
        callback_(callback),
        first_run_(true) {
  }
  ~HangingFirstRequestInterceptor() override {}

  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    if (first_run_) {
      first_run_ = false;
      if (!callback_.is_null()) {
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE, callback_);
      }
      return new HangingURLRequestJob(request, network_delegate);
    }
    return new net::URLRequestMockHTTPJob(
        request,
        network_delegate,
        file_,
        BrowserThread::GetBlockingPool()->GetTaskRunnerWithShutdownBehavior(
            base::SequencedWorkerPool::SKIP_ON_SHUTDOWN));
  }

 private:
  base::FilePath file_;
  base::Closure callback_;
  mutable bool first_run_;
};

// An ExternalProtocolHandler that blocks everything and asserts it never is
// called.
class NeverRunsExternalProtocolHandlerDelegate
    : public ExternalProtocolHandler::Delegate {
 public:
  scoped_refptr<shell_integration::DefaultProtocolClientWorker>
  CreateShellWorker(
      const shell_integration::DefaultWebClientWorkerCallback& callback,
      const std::string& protocol) override {
    NOTREACHED();
    // This will crash, but it shouldn't get this far with BlockState::BLOCK
    // anyway.
    return nullptr;
  }

  ExternalProtocolHandler::BlockState GetBlockState(const std::string& scheme,
                                                    Profile* profile) override {
    // Block everything and fail the test.
    ADD_FAILURE();
    return ExternalProtocolHandler::BLOCK;
  }

  void BlockRequest() override {}

  void RunExternalProtocolDialog(const GURL& url,
                                 int render_process_host_id,
                                 int routing_id,
                                 ui::PageTransition page_transition,
                                 bool has_user_gesture) override {
    NOTREACHED();
  }

  void LaunchUrlWithoutSecurityCheck(
      const GURL& url,
      content::WebContents* web_contents) override {
    NOTREACHED();
  }

  void FinishedProcessingCheck() override { NOTREACHED(); }
};

}  // namespace

RequestCounter::RequestCounter() : count_(0), expected_count_(-1) {}

RequestCounter::~RequestCounter() {}

void RequestCounter::RequestStarted() {
  count_++;
  if (loop_ && count_ == expected_count_)
    loop_->Quit();
}

void RequestCounter::WaitForCount(int expected_count) {
  ASSERT_TRUE(!loop_);
  ASSERT_EQ(-1, expected_count_);
  if (count_ < expected_count) {
    expected_count_ = expected_count;
    loop_.reset(new base::RunLoop);
    loop_->Run();
    expected_count_ = -1;
    loop_.reset();
  }

  EXPECT_EQ(expected_count, count_);
}

FakeSafeBrowsingDatabaseManager::FakeSafeBrowsingDatabaseManager() {}

bool FakeSafeBrowsingDatabaseManager::CheckBrowseUrl(const GURL& gurl,
                                                     Client* client) {
  if (bad_urls_.find(gurl.spec()) == bad_urls_.end() ||
      bad_urls_[gurl.spec()] == safe_browsing::SB_THREAT_TYPE_SAFE) {
    return true;
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&FakeSafeBrowsingDatabaseManager::OnCheckBrowseURLDone, this,
                 gurl, client));
  return false;
}

bool FakeSafeBrowsingDatabaseManager::IsSupported() const {
  return true;
}

bool FakeSafeBrowsingDatabaseManager::ChecksAreAlwaysAsync() const {
  return false;
}

bool FakeSafeBrowsingDatabaseManager::CanCheckResourceType(
    content::ResourceType /* resource_type */) const {
  return true;
}

bool FakeSafeBrowsingDatabaseManager::CheckExtensionIDs(
    const std::set<std::string>& extension_ids,
    Client* client) {
  return true;
}

FakeSafeBrowsingDatabaseManager::~FakeSafeBrowsingDatabaseManager() {}

void FakeSafeBrowsingDatabaseManager::OnCheckBrowseURLDone(const GURL& gurl,
                                                           Client* client) {
  std::vector<safe_browsing::SBThreatType> expected_threats;
  expected_threats.push_back(safe_browsing::SB_THREAT_TYPE_URL_MALWARE);
  expected_threats.push_back(safe_browsing::SB_THREAT_TYPE_URL_PHISHING);
  // TODO(nparker): Replace SafeBrowsingCheck w/ a call to
  // client->OnCheckBrowseUrlResult()
  safe_browsing::LocalSafeBrowsingDatabaseManager::SafeBrowsingCheck sb_check(
      std::vector<GURL>(1, gurl), std::vector<safe_browsing::SBFullHash>(),
      client, safe_browsing::MALWARE, expected_threats);
  sb_check.url_results[0] = bad_urls_[gurl.spec()];
  sb_check.OnSafeBrowsingResult();
}

TestPrerenderContents::TestPrerenderContents(
    PrerenderManager* prerender_manager,
    Profile* profile,
    const GURL& url,
    const content::Referrer& referrer,
    Origin origin,
    FinalStatus expected_final_status)
    : PrerenderContents(prerender_manager, profile, url, referrer, origin),
      expected_final_status_(expected_final_status),
      new_render_view_host_(nullptr),
      was_hidden_(false),
      was_shown_(false),
      should_be_shown_(expected_final_status == FINAL_STATUS_USED),
      skip_final_checks_(false) {}

TestPrerenderContents::~TestPrerenderContents() {
  if (skip_final_checks_)
    return;

  EXPECT_EQ(expected_final_status_, final_status())
      << " when testing URL " << prerender_url().path()
      << " (Expected: " << NameFromFinalStatus(expected_final_status_)
      << ", Actual: " << NameFromFinalStatus(final_status()) << ")";

  // Prerendering RenderViewHosts should be hidden before the first
  // navigation, so this should be happen for every PrerenderContents for
  // which a RenderViewHost is created, regardless of whether or not it's
  // used.
  if (new_render_view_host_)
    EXPECT_TRUE(was_hidden_);

  // A used PrerenderContents will only be destroyed when we swap out
  // WebContents, at the end of a navigation caused by a call to
  // NavigateToURLImpl().
  if (final_status() == FINAL_STATUS_USED)
    EXPECT_TRUE(new_render_view_host_);

  EXPECT_EQ(should_be_shown_, was_shown_);
}

bool TestPrerenderContents::CheckURL(const GURL& url) {
  // Prevent FINAL_STATUS_UNSUPPORTED_SCHEME when navigating to about:crash in
  // the PrerenderRendererCrash test.
  if (url.spec() != content::kChromeUICrashURL)
    return PrerenderContents::CheckURL(url);
  return true;
}

void TestPrerenderContents::OnRenderViewHostCreated(
    RenderViewHost* new_render_view_host) {
  // Used to make sure the RenderViewHost is hidden and, if used,
  // subsequently shown.
  notification_registrar().Add(
      this, content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
      content::Source<content::RenderWidgetHost>(
          new_render_view_host->GetWidget()));

  new_render_view_host_ = new_render_view_host;

  PrerenderContents::OnRenderViewHostCreated(new_render_view_host);
}

void TestPrerenderContents::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED) {
    EXPECT_EQ(new_render_view_host_->GetWidget(),
              content::Source<content::RenderWidgetHost>(source).ptr());
    bool is_visible = *content::Details<bool>(details).ptr();

    if (!is_visible) {
      was_hidden_ = true;
    } else if (is_visible && was_hidden_) {
      // Once hidden, a prerendered RenderViewHost should only be shown after
      // being removed from the PrerenderContents for display.
      EXPECT_FALSE(GetRenderViewHost());
      was_shown_ = true;
    }
    return;
  }
  PrerenderContents::Observe(type, source, details);
}

DestructionWaiter::DestructionWaiter(TestPrerenderContents* prerender_contents,
                                     FinalStatus expected_final_status)
    : expected_final_status_(expected_final_status),
      saw_correct_status_(false) {
  if (!prerender_contents) {
    // TODO(mattcary): It is not correct to assume the contents were destroyed
    // correctly, but until the prefetch renderer destruction race can be fixed
    // there's no other way to keep tests from flaking.
    saw_correct_status_ = true;
    return;
  }
  if (prerender_contents->final_status() != FINAL_STATUS_MAX) {
    // The contents was already destroyed by the time this was called.
    MarkDestruction(prerender_contents->final_status());
  } else {
    marker_ = base::MakeUnique<DestructionMarker>(this);
    prerender_contents->AddObserver(marker_.get());
  }
}

DestructionWaiter::~DestructionWaiter() {}

bool DestructionWaiter::WaitForDestroy() {
  if (!saw_correct_status_) {
    wait_loop_.Run();
  }
  return saw_correct_status_;
}

void DestructionWaiter::MarkDestruction(FinalStatus reason) {
  saw_correct_status_ = (reason == expected_final_status_);
  wait_loop_.Quit();
}

DestructionWaiter::DestructionMarker::DestructionMarker(
    DestructionWaiter* waiter)
    : waiter_(waiter) {}

DestructionWaiter::DestructionMarker::~DestructionMarker() {}

void DestructionWaiter::DestructionMarker::OnPrerenderStop(
    PrerenderContents* contents) {
  waiter_->MarkDestruction(contents->final_status());
}

TestPrerender::TestPrerender()
    : contents_(nullptr),
      final_status_(FINAL_STATUS_MAX),
      number_of_loads_(0),
      expected_number_of_loads_(0),
      started_(false),
      stopped_(false) {}

TestPrerender::~TestPrerender() {
  if (contents_)
    contents_->RemoveObserver(this);
}

FinalStatus TestPrerender::GetFinalStatus() const {
  if (contents_)
    return contents_->final_status();
  return final_status_;
}

void TestPrerender::WaitForCreate() {
  if (contents_)
    return;
  create_loop_.Run();
}

void TestPrerender::WaitForStart() {
  if (started_)
    return;
  start_loop_.Run();
}

void TestPrerender::WaitForStop() {
  if (stopped_)
    return;
  stop_loop_.Run();
}

void TestPrerender::WaitForLoads(int expected_number_of_loads) {
  DCHECK(!load_waiter_);
  DCHECK(!expected_number_of_loads_);
  if (number_of_loads_ < expected_number_of_loads) {
    load_waiter_.reset(new base::RunLoop);
    expected_number_of_loads_ = expected_number_of_loads;
    load_waiter_->Run();
    load_waiter_.reset();
    expected_number_of_loads_ = 0;
  }
  EXPECT_LE(expected_number_of_loads, number_of_loads_);
}

void TestPrerender::OnPrerenderCreated(TestPrerenderContents* contents) {
  DCHECK(!contents_);
  contents_ = contents;
  contents_->AddObserver(this);
  create_loop_.Quit();
}

void TestPrerender::OnPrerenderStart(PrerenderContents* contents) {
  started_ = true;
  start_loop_.Quit();
}

void TestPrerender::OnPrerenderStopLoading(PrerenderContents* contents) {
  number_of_loads_++;
  if (load_waiter_ && number_of_loads_ >= expected_number_of_loads_)
    load_waiter_->Quit();
}

void TestPrerender::OnPrerenderStop(PrerenderContents* contents) {
  DCHECK(contents_);
  contents_ = nullptr;
  final_status_ = contents->final_status();
  stopped_ = true;
  stop_loop_.Quit();
  // If there is a WaitForLoads call and it has yet to see the expected number
  // of loads, stop the loop so the test fails instead of timing out.
  if (load_waiter_)
    load_waiter_->Quit();
}

// static
FirstContentfulPaintManagerWaiter* FirstContentfulPaintManagerWaiter::Create(
    PrerenderManager* manager) {
  auto fcp_waiter = base::WrapUnique(new FirstContentfulPaintManagerWaiter());
  auto* fcp_waiter_ptr = fcp_waiter.get();
  manager->AddObserver(std::move(fcp_waiter));
  return fcp_waiter_ptr;
}

FirstContentfulPaintManagerWaiter::FirstContentfulPaintManagerWaiter()
    : saw_fcp_(false) {}

FirstContentfulPaintManagerWaiter::~FirstContentfulPaintManagerWaiter() {}

void FirstContentfulPaintManagerWaiter::OnFirstContentfulPaint() {
  saw_fcp_ = true;
  if (waiter_)
    waiter_->Quit();
}

void FirstContentfulPaintManagerWaiter::Wait() {
  if (saw_fcp_)
    return;
  waiter_ = base::MakeUnique<base::RunLoop>();
  waiter_->Run();
  waiter_.reset();
}

TestPrerenderContentsFactory::TestPrerenderContentsFactory() {}

TestPrerenderContentsFactory::~TestPrerenderContentsFactory() {
  EXPECT_TRUE(expected_contents_queue_.empty());
}

std::unique_ptr<TestPrerender>
TestPrerenderContentsFactory::ExpectPrerenderContents(
    FinalStatus final_status) {
  std::unique_ptr<TestPrerender> handle(new TestPrerender());
  expected_contents_queue_.push_back(
      ExpectedContents(final_status, handle->AsWeakPtr()));
  return handle;
}

PrerenderContents* TestPrerenderContentsFactory::CreatePrerenderContents(
    PrerenderManager* prerender_manager,
    Profile* profile,
    const GURL& url,
    const content::Referrer& referrer,
    Origin origin) {
  ExpectedContents expected;
  if (!expected_contents_queue_.empty()) {
    expected = expected_contents_queue_.front();
    expected_contents_queue_.pop_front();
  }
  TestPrerenderContents* contents = new TestPrerenderContents(
      prerender_manager, profile, url, referrer, origin, expected.final_status);
  if (expected.handle)
    expected.handle->OnPrerenderCreated(contents);
  return contents;
}

TestPrerenderContentsFactory::ExpectedContents::ExpectedContents()
    : final_status(FINAL_STATUS_MAX) {}

TestPrerenderContentsFactory::ExpectedContents::ExpectedContents(
    const ExpectedContents& other) = default;

TestPrerenderContentsFactory::ExpectedContents::ExpectedContents(
    FinalStatus final_status,
    const base::WeakPtr<TestPrerender>& handle)
    : final_status(final_status), handle(handle) {}

TestPrerenderContentsFactory::ExpectedContents::~ExpectedContents() {}

PrerenderInProcessBrowserTest::PrerenderInProcessBrowserTest()
    : external_protocol_handler_delegate_(
          base::MakeUnique<NeverRunsExternalProtocolHandlerDelegate>()),
      safe_browsing_factory_(
          base::MakeUnique<safe_browsing::TestSafeBrowsingServiceFactory>()),
      prerender_contents_factory_(nullptr),
      explicitly_set_browser_(nullptr),
      autostart_test_server_(true) {}

PrerenderInProcessBrowserTest::~PrerenderInProcessBrowserTest() {}

void PrerenderInProcessBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  ASSERT_TRUE(ppapi::RegisterFlashTestPlugin(command_line));
}

void PrerenderInProcessBrowserTest::TearDownInProcessBrowserTestFixture() {
  safe_browsing::SafeBrowsingService::RegisterFactory(nullptr);
}

content::SessionStorageNamespace*
PrerenderInProcessBrowserTest::GetSessionStorageNamespace() const {
  content::WebContents* web_contents = GetActiveWebContents();
  if (!web_contents)
    return nullptr;
  return web_contents->GetController().GetDefaultSessionStorageNamespace();
}

std::string PrerenderInProcessBrowserTest::MakeAbsolute(
    const std::string& path) {
  CHECK(!path.empty());
  if (path.front() == '/') {
    return path;
  }
  return "/" + path;
}

bool PrerenderInProcessBrowserTest::UrlIsInPrerenderManager(
    const std::string& html_file) const {
  return UrlIsInPrerenderManager(embedded_test_server()->GetURL(html_file));
}

bool PrerenderInProcessBrowserTest::UrlIsInPrerenderManager(
    const GURL& url) const {
  return GetPrerenderManager()->FindPrerenderData(
             url, GetSessionStorageNamespace()) != nullptr;
}

content::WebContents* PrerenderInProcessBrowserTest::GetActiveWebContents()
    const {
  return current_browser()->tab_strip_model()->GetActiveWebContents();
}

PrerenderManager* PrerenderInProcessBrowserTest::GetPrerenderManager() const {
  return PrerenderManagerFactory::GetForBrowserContext(
      current_browser()->profile());
}

TestPrerenderContents* PrerenderInProcessBrowserTest::GetPrerenderContentsFor(
    const GURL& url) const {
  PrerenderManager::PrerenderData* prerender_data =
      GetPrerenderManager()->FindPrerenderData(url, nullptr);
  return static_cast<TestPrerenderContents*>(
      prerender_data ? prerender_data->contents() : nullptr);
}

net::EmbeddedTestServer* PrerenderInProcessBrowserTest::src_server() {
  if (https_src_server_)
    return https_src_server_.get();
  return embedded_test_server();
}

test_utils::FakeSafeBrowsingDatabaseManager*
PrerenderInProcessBrowserTest::GetFakeSafeBrowsingDatabaseManager() {
  return static_cast<test_utils::FakeSafeBrowsingDatabaseManager*>(
      safe_browsing_factory()
          ->test_safe_browsing_service()
          ->database_manager()
          .get());
}

void PrerenderInProcessBrowserTest::SetUpInProcessBrowserTestFixture() {
  safe_browsing_factory_->SetTestDatabaseManager(
      new test_utils::FakeSafeBrowsingDatabaseManager());
  safe_browsing::SafeBrowsingService::RegisterFactory(
      safe_browsing_factory_.get());
}

void PrerenderInProcessBrowserTest::SetUpOnMainThread() {
  // Increase the memory allowed in a prerendered page above normal settings.
  // Debug build bots occasionally run against the default limit, and tests
  // were failing because the prerender was canceled due to memory exhaustion.
  // http://crbug.com/93076
  GetPrerenderManager()->mutable_config().max_bytes = 2000 * 1024 * 1024;

  current_browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kPromptForDownload, false);
  if (autostart_test_server_)
    ASSERT_TRUE(embedded_test_server()->Start());
  ChromeResourceDispatcherHostDelegate::
      SetExternalProtocolHandlerDelegateForTesting(
          external_protocol_handler_delegate_.get());

  PrerenderManager* prerender_manager = GetPrerenderManager();
  ASSERT_TRUE(prerender_manager);
  prerender_manager->mutable_config().rate_limit_enabled = false;
  ASSERT_FALSE(prerender_contents_factory_);
  prerender_contents_factory_ = new TestPrerenderContentsFactory;
  prerender_manager->SetPrerenderContentsFactoryForTest(
      prerender_contents_factory_);
  ASSERT_TRUE(safe_browsing_factory_->test_safe_browsing_service());
}

void PrerenderInProcessBrowserTest::UseHttpsSrcServer() {
  if (https_src_server_)
    return;
  https_src_server_.reset(
      new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
  https_src_server_->ServeFilesFromSourceDirectory("chrome/test/data");
  CHECK(https_src_server_->Start());
}

base::string16 PrerenderInProcessBrowserTest::MatchTaskManagerTab(
    const char* page_title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_TAB_PREFIX,
                                    base::ASCIIToUTF16(page_title));
}

base::string16 PrerenderInProcessBrowserTest::MatchTaskManagerPrerender(
    const char* page_title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_PRERENDER_PREFIX,
                                    base::ASCIIToUTF16(page_title));
}

std::vector<std::unique_ptr<TestPrerender>>
PrerenderInProcessBrowserTest::NavigateWithPrerenders(
    const GURL& loader_url,
    const std::vector<FinalStatus>& expected_final_status_queue) {
  CHECK(!expected_final_status_queue.empty());
  std::vector<std::unique_ptr<TestPrerender>> prerenders;
  for (size_t i = 0; i < expected_final_status_queue.size(); i++) {
    prerenders.push_back(prerender_contents_factory()
        ->ExpectPrerenderContents(expected_final_status_queue[i]));
  }

  // Navigate to the loader URL and then wait for the first prerender to be
  // created.
  ui_test_utils::NavigateToURL(current_browser(), loader_url);
  prerenders[0]->WaitForCreate();

  return prerenders;
}

GURL PrerenderInProcessBrowserTest::ServeLoaderURL(
    const std::string& loader_path,
    const std::string& replacement_variable,
    const GURL& url_to_prerender,
    const std::string& loader_query) {
  base::StringPairs replacement_text;
  replacement_text.push_back(
      make_pair(replacement_variable, url_to_prerender.spec()));
  std::string replacement_path;
  net::test_server::GetFilePathWithReplacements(loader_path, replacement_text,
                                                &replacement_path);
  return src_server()->GetURL(replacement_path + loader_query);
}

void CreateCountingInterceptorOnIO(
    const GURL& url,
    const base::FilePath& file,
    const base::WeakPtr<RequestCounter>& counter) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
      url, base::MakeUnique<CountingInterceptor>(file, counter));
}

void InterceptRequestAndCount(
    const GURL& url,
    RequestCounter* counter,
    base::Callback<void(net::URLRequest*)> callback_io) {
  CountingInterceptorWithCallback::Initialize(url, counter, callback_io);
}

void CreateMockInterceptorOnIO(const GURL& url, const base::FilePath& file) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
      url, net::URLRequestMockHTTPJob::CreateInterceptorForSingleFile(
               file, content::BrowserThread::GetBlockingPool()));
}

void CreateHangingFirstRequestInterceptorOnIO(
    const GURL& url, const base::FilePath& file, base::Closure callback) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  std::unique_ptr<net::URLRequestInterceptor> interceptor(
      new HangingFirstRequestInterceptor(file, callback));
  net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
      url, std::move(interceptor));
}

}  // namespace test_utils

}  // namespace prerender
