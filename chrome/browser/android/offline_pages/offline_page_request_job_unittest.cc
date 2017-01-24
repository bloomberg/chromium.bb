// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/offline_page_request_job.h"

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/android/offline_pages/offline_page_request_interceptor.h"
#include "chrome/browser/android/offline_pages/offline_page_tab_helper.h"
#include "chrome/browser/android/offline_pages/test_offline_page_model_builder.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_model_impl.h"
#include "components/previews/core/previews_decider.h"
#include "components/previews/core/previews_experiments.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {

const GURL kTestUrl("http://test.org/page1");
const GURL kTestUrl2("http://test.org/page2");
const GURL kTestUrl3("http://test.org/page3");
const GURL kTestUrl3WithFragment("http://test.org/page3#ref1");
const GURL kTestUrl4("http://test.org/page4");
const GURL kTestOriginalUrl("http://test.org/first");
const ClientId kTestClientId = ClientId(kBookmarkNamespace, "1234");
const ClientId kTestClientId2 = ClientId(kDownloadNamespace, "1a2b3c4d");
const ClientId kTestClientId3 = ClientId(kDownloadNamespace, "3456abcd");
const ClientId kTestClientId4 = ClientId(kDownloadNamespace, "5678");
const int kTestFileSize = 444;
const int kTestFileSize2 = 450;
const int kTestFileSize3 = 450;
const int kTestFileSize4 = 111;
const int kTabId = 1;
const int kBufSize = 1024;
const char kAggregatedRequestResultHistogram[] =
    "OfflinePages.AggregatedRequestResult2";
const char kOpenFileErrorCodeHistogram[] =
    "OfflinePages.RequestJob.OpenFileErrorCode";

class OfflinePageRequestJobTestDelegate :
    public OfflinePageRequestJob::Delegate {
 public:
  OfflinePageRequestJobTestDelegate(content::WebContents* web_content,
                                    int tab_id)
      : web_content_(web_content),
        tab_id_(tab_id) {}

  content::ResourceRequestInfo::WebContentsGetter
  GetWebContentsGetter(net::URLRequest* request) const override {
    return base::Bind(&OfflinePageRequestJobTestDelegate::GetWebContents,
                      web_content_);
  }

  OfflinePageRequestJob::Delegate::TabIdGetter GetTabIdGetter() const override {
    return base::Bind(&OfflinePageRequestJobTestDelegate::GetTabId,
                      tab_id_);
  }

 private:
  static content::WebContents* GetWebContents(
      content::WebContents* web_content) {
    return web_content;
  }

  static bool GetTabId(
      int tab_id_value, content::WebContents* web_content, int* tab_id) {
    *tab_id = tab_id_value;
    return true;
  }

  content::WebContents* web_content_;
  int tab_id_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageRequestJobTestDelegate);
};

class TestURLRequestDelegate : public net::URLRequest::Delegate {
 public:
  typedef base::Callback<void(int)> ReadCompletedCallback;

  explicit TestURLRequestDelegate(const ReadCompletedCallback& callback)
      : read_completed_callback_(callback),
        buffer_(new net::IOBuffer(kBufSize)) {}

  void OnResponseStarted(net::URLRequest* request, int net_error) override {
    DCHECK_NE(net::ERR_IO_PENDING, net_error);
    if (net_error != net::OK) {
      read_completed_callback_.Run(0);
      return;
    }
    int bytes_read = 0;
    request->Read(buffer_.get(), kBufSize, &bytes_read);
  }

  void OnReadCompleted(net::URLRequest* request, int bytes_read) override {
    if (!read_completed_callback_.is_null())
      read_completed_callback_.Run(bytes_read);
  }

 private:
  ReadCompletedCallback read_completed_callback_;
  scoped_refptr<net::IOBuffer> buffer_;

  DISALLOW_COPY_AND_ASSIGN(TestURLRequestDelegate);
};

class TestURLRequestInterceptingJobFactory
    : public net::URLRequestInterceptingJobFactory {
 public:
  TestURLRequestInterceptingJobFactory(
      std::unique_ptr<net::URLRequestJobFactory> job_factory,
      std::unique_ptr<net::URLRequestInterceptor> interceptor,
      content::WebContents* web_contents)
      : net::URLRequestInterceptingJobFactory(
            std::move(job_factory), std::move(interceptor)),
        web_contents_(web_contents) {}
  ~TestURLRequestInterceptingJobFactory() override {}

  net::URLRequestJob* MaybeCreateJobWithProtocolHandler(
      const std::string& scheme,
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    net::URLRequestJob* job = net::URLRequestInterceptingJobFactory::
        MaybeCreateJobWithProtocolHandler(scheme, request, network_delegate);
    if (job) {
      static_cast<OfflinePageRequestJob*>(job)->SetDelegateForTesting(
          base::MakeUnique<OfflinePageRequestJobTestDelegate>(
              web_contents_, kTabId));
    }
    return job;
  }

 private:
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(TestURLRequestInterceptingJobFactory);
};

class TestNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  TestNetworkChangeNotifier() : online_(true) {}
  ~TestNetworkChangeNotifier() override {}

  net::NetworkChangeNotifier::ConnectionType GetCurrentConnectionType()
      const override {
    return online_ ? net::NetworkChangeNotifier::CONNECTION_UNKNOWN
                   : net::NetworkChangeNotifier::CONNECTION_NONE;
  }

  void set_online(bool online) { online_ = online; }

 private:
  bool online_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkChangeNotifier);
};

class TestPreviewsDecider : public previews::PreviewsDecider {
 public:
  TestPreviewsDecider() : should_allow_preview_(false) {}
  ~TestPreviewsDecider() override {}

  bool ShouldAllowPreview(const net::URLRequest& request,
                          previews::PreviewsType type) const override {
    return should_allow_preview_;
  }

  void set_should_allow_preview(bool should_allow_preview) {
    should_allow_preview_ = should_allow_preview;
  }

 private:
  bool should_allow_preview_;

  DISALLOW_COPY_AND_ASSIGN(TestPreviewsDecider);
};

class TestOfflinePageArchiver : public OfflinePageArchiver {
 public:
  TestOfflinePageArchiver(const GURL& url,
                          const base::FilePath& archive_file_path,
                          int archive_file_size)
      : url_(url),
        archive_file_path_(archive_file_path),
        archive_file_size_(archive_file_size) {}
  ~TestOfflinePageArchiver() override {}

  void CreateArchive(const base::FilePath& archives_dir,
                     const CreateArchiveParams& create_archive_params,
                     const CreateArchiveCallback& callback) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, this, ArchiverResult::SUCCESSFULLY_CREATED,
                   url_, archive_file_path_, base::string16(),
                   archive_file_size_));
  }

 private:
  const GURL url_;
  const base::FilePath archive_file_path_;
  const int archive_file_size_;

  DISALLOW_COPY_AND_ASSIGN(TestOfflinePageArchiver);
};

}  // namespace

class OfflinePageRequestJobTest : public testing::Test {
 public:
  OfflinePageRequestJobTest();
  ~OfflinePageRequestJobTest() override {}

  void SetUp() override;
  void TearDown() override;

  void SimulateHasNetworkConnectivity(bool has_connectivity);
  void RunUntilIdle();

  void SavePage(const GURL& url,
                const ClientId& client_id,
                const GURL& original_url,
                std::unique_ptr<OfflinePageArchiver> archiver);

  void InterceptRequest(const GURL& url,
                        const std::string& method,
                        const std::string& extra_header_name,
                        const std::string& extra_header_value,
                        content::ResourceType resource_type);

  // Expect exactly one count of |result| UMA reported. No other bucket should
  // have sample.
  void ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult result);
  // Expect exactly |count| of |result| UMA reported. No other bucket should
  // have sample.
  void ExpectMultiUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult result, int count);
  // Expect one count of |result| UMA reported. Other buckets may have samples
  // as well.
  void ExpectOneNonuniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult result);

  void ExpectOpenFileErrorCode(int result);

  net::TestURLRequestContext* url_request_context() {
    return test_url_request_context_.get();
  }
  Profile* profile() { return profile_; }
  OfflinePageTabHelper* offline_page_tab_helper() const {
    return offline_page_tab_helper_;
  }
  int64_t offline_id() const { return offline_id_; }
  int64_t offline_id2() const { return offline_id2_; }
  int64_t offline_id3() const { return offline_id3_; }
  int64_t offline_id4() const { return offline_id4_; }
  int bytes_read() const { return bytes_read_; }

  TestPreviewsDecider* test_previews_decider() {
    return test_previews_decider_.get();
  }

 private:
  void OnSavePageDone(SavePageResult result, int64_t offline_id);
  std::unique_ptr<net::URLRequest> CreateRequest(
      const GURL& url,
      const std::string& method,
      content::ResourceType resource_type);
  void ReadCompleted(int bytes_read);

  // Runs on IO thread.
  void InterceptRequestOnIO(const GURL& url,
                            const std::string& method,
                            const std::string& extra_header_name,
                            const std::string& extra_header_value,
                            content::ResourceType resource_type);
  void ReadCompletedOnIO(int bytes_read);

  content::TestBrowserThreadBundle thread_bundle_;
  base::SimpleTestClock clock_;
  std::unique_ptr<TestNetworkChangeNotifier> network_change_notifier_;
  std::unique_ptr<net::TestURLRequestContext> test_url_request_context_;
  net::URLRequestJobFactoryImpl url_request_job_factory_;
  std::unique_ptr<net::URLRequestInterceptingJobFactory>
      intercepting_job_factory_;
  std::unique_ptr<TestURLRequestDelegate> url_request_delegate_;
  std::unique_ptr<TestPreviewsDecider> test_previews_decider_;
  net::TestNetworkDelegate network_delegate_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  base::HistogramTester histogram_tester_;
  OfflinePageTabHelper* offline_page_tab_helper_;  // Not owned.
  std::unique_ptr<net::URLRequest> request_;
  int64_t offline_id_;
  int64_t offline_id2_;
  int64_t offline_id3_;
  int64_t offline_id4_;
  int bytes_read_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageRequestJobTest);
};

OfflinePageRequestJobTest::OfflinePageRequestJobTest()
    : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
      network_change_notifier_(new TestNetworkChangeNotifier()),
      profile_manager_(TestingBrowserProcess::GetGlobal()),
      offline_id_(-1),
      offline_id2_(-1),
      offline_id3_(-1),
      offline_id4_(-1),
      bytes_read_(0) {}

void OfflinePageRequestJobTest::SetUp() {
  // Create a test profile.
  ASSERT_TRUE(profile_manager_.SetUp());
  profile_ = profile_manager_.CreateTestingProfile("Profile 1");

  // Create a test web contents.
  web_contents_.reset(content::WebContents::Create(
      content::WebContents::CreateParams(profile_)));
  OfflinePageTabHelper::CreateForWebContents(web_contents_.get());
  offline_page_tab_helper_ =
      OfflinePageTabHelper::FromWebContents(web_contents_.get());

  // Set up the factory for testing.
  OfflinePageModelFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), BuildTestOfflinePageModel);
  RunUntilIdle();

  OfflinePageModel* model =
      OfflinePageModelFactory::GetForBrowserContext(profile());

  // Hook up a test clock such that we can control the time when the offline
  // page is created.
  clock_.SetNow(base::Time::Now());
  static_cast<OfflinePageModelImpl*>(model)->set_testing_clock(&clock_);

  // All offline pages being created below will point to real archive files
  // residing in test data directory.
  base::FilePath test_data_dir_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_path));

  // Save an offline page.
  base::FilePath archive_file_path =
      test_data_dir_path.AppendASCII("offline_pages").AppendASCII("test.mhtml");
  std::unique_ptr<TestOfflinePageArchiver> archiver(
      new TestOfflinePageArchiver(kTestUrl, archive_file_path, kTestFileSize));

  SavePage(kTestUrl, kTestClientId, GURL(), std::move(archiver));

  // Save another offline page associated with same online URL as above, but
  // pointing to different archive file.
  base::FilePath archive_file_path2 =
      test_data_dir_path.AppendASCII("offline_pages").
          AppendASCII("hello.mhtml");
  std::unique_ptr<TestOfflinePageArchiver> archiver2(
      new TestOfflinePageArchiver(
          kTestUrl, archive_file_path2, kTestFileSize2));

  // Make sure that the creation time of 2nd offline file is later.
  clock_.Advance(base::TimeDelta::FromMinutes(10));

  SavePage(kTestUrl, kTestClientId2, GURL(), std::move(archiver2));

  // Save an offline page associated with online URL that has a fragment
  // identifier.
  base::FilePath archive_file_path3 =
      test_data_dir_path.AppendASCII("offline_pages").
          AppendASCII("hello.mhtml");
  std::unique_ptr<TestOfflinePageArchiver> archiver3(
      new TestOfflinePageArchiver(
          kTestUrl3WithFragment, archive_file_path3, kTestFileSize3));

  SavePage(kTestUrl3WithFragment, kTestClientId3, kTestOriginalUrl,
      std::move(archiver3));

  // Save an offline page pointing to non-existent archive file.
  base::FilePath archive_file_path4 =
      test_data_dir_path.AppendASCII("offline_pages")
          .AppendASCII("nonexistent.mhtml");
  std::unique_ptr<TestOfflinePageArchiver> archiver4(
      new TestOfflinePageArchiver(kTestUrl4, archive_file_path4,
                                  kTestFileSize4));

  SavePage(kTestUrl4, kTestClientId4, GURL(), std::move(archiver4));

  // Create a context with delayed initialization.
  test_url_request_context_.reset(new net::TestURLRequestContext(true));

  test_previews_decider_.reset(new TestPreviewsDecider());

  // Install the interceptor.
  std::unique_ptr<net::URLRequestInterceptor> interceptor(
      new OfflinePageRequestInterceptor(test_previews_decider_.get()));
  std::unique_ptr<net::URLRequestJobFactoryImpl> job_factory_impl(
      new net::URLRequestJobFactoryImpl());
  intercepting_job_factory_.reset(new TestURLRequestInterceptingJobFactory(
      std::move(job_factory_impl),
      std::move(interceptor),
      web_contents_.get()));

  test_url_request_context_->set_job_factory(intercepting_job_factory_.get());
  test_url_request_context_->Init();
}

void OfflinePageRequestJobTest::TearDown() {
  OfflinePageModel* model =
      OfflinePageModelFactory::GetForBrowserContext(profile());
  static_cast<OfflinePageModelImpl*>(model)->set_testing_clock(nullptr);
}

void OfflinePageRequestJobTest::SimulateHasNetworkConnectivity(
    bool online) {
  network_change_notifier_->set_online(online);
}

void OfflinePageRequestJobTest::RunUntilIdle() {
  base::RunLoop().RunUntilIdle();
}

std::unique_ptr<net::URLRequest> OfflinePageRequestJobTest::CreateRequest(
    const GURL& url,
    const std::string& method,
    content::ResourceType resource_type) {
  url_request_delegate_ = base::MakeUnique<TestURLRequestDelegate>(
      base::Bind(&OfflinePageRequestJobTest::ReadCompletedOnIO,
                 base::Unretained(this)));

  std::unique_ptr<net::URLRequest> request =
      url_request_context()->CreateRequest(
          url, net::DEFAULT_PRIORITY, url_request_delegate_.get());
  request->set_method(method);

  content::ResourceRequestInfo::AllocateForTesting(
      request.get(), resource_type, nullptr, /*render_process_id=*/1,
      /*render_view_id=*/-1,
      /*render_frame_id=*/1,
      /*is_main_frame=*/true,
      /*parent_is_main_frame=*/false,
      /*allow_download=*/true,
      /*is_async=*/true, content::PREVIEWS_OFF);

  return request;
}

void
OfflinePageRequestJobTest::ExpectOneUniqueSampleForAggregatedRequestResult(
    OfflinePageRequestJob::AggregatedRequestResult result) {
  histogram_tester_.ExpectUniqueSample(
      kAggregatedRequestResultHistogram, static_cast<int>(result), 1);
}

void
OfflinePageRequestJobTest::ExpectMultiUniqueSampleForAggregatedRequestResult(
    OfflinePageRequestJob::AggregatedRequestResult result, int count) {
  histogram_tester_.ExpectUniqueSample(
      kAggregatedRequestResultHistogram, static_cast<int>(result), count);
}

void
OfflinePageRequestJobTest::ExpectOneNonuniqueSampleForAggregatedRequestResult(
    OfflinePageRequestJob::AggregatedRequestResult result) {
  histogram_tester_.ExpectBucketCount(
      kAggregatedRequestResultHistogram, static_cast<int>(result), 1);
}

void OfflinePageRequestJobTest::ExpectOpenFileErrorCode(int result) {
  histogram_tester_.ExpectUniqueSample(kOpenFileErrorCodeHistogram, -result, 1);
}

void OfflinePageRequestJobTest::SavePage(
    const GURL& url,
    const ClientId& client_id,
    const GURL& original_url,
    std::unique_ptr<OfflinePageArchiver> archiver) {
  OfflinePageModel::SavePageParams save_page_params;
  save_page_params.url = url;
  save_page_params.client_id = client_id;
  save_page_params.original_url = original_url;
  OfflinePageModelFactory::GetForBrowserContext(profile())->SavePage(
      save_page_params,
      std::move(archiver),
      base::Bind(&OfflinePageRequestJobTest::OnSavePageDone,
                 base::Unretained(this)));
  RunUntilIdle();
}

void OfflinePageRequestJobTest::OnSavePageDone(SavePageResult result,
                                               int64_t offline_id) {
  ASSERT_EQ(SavePageResult::SUCCESS, result);
  if (offline_id_ == -1)
    offline_id_ = offline_id;
  else if (offline_id2_ == -1)
    offline_id2_ = offline_id;
  else if (offline_id3_ == -1)
    offline_id3_ = offline_id;
  else if (offline_id4_ == -1)
    offline_id4_ = offline_id;
}

void OfflinePageRequestJobTest::InterceptRequestOnIO(
    const GURL& url,
    const std::string& method,
    const std::string& extra_header_name,
    const std::string& extra_header_value,
    content::ResourceType resource_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  request_ = CreateRequest(url, method, resource_type);
  if (!extra_header_name.empty()) {
    request_->SetExtraRequestHeaderByName(
        extra_header_name, extra_header_value, true);
  }
  request_->Start();
}

void OfflinePageRequestJobTest::InterceptRequest(
    const GURL& url,
    const std::string& method,
    const std::string& extra_header_name,
    const std::string& extra_header_value,
    content::ResourceType resource_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&OfflinePageRequestJobTest::InterceptRequestOnIO,
                 base::Unretained(this), url, method, extra_header_name,
                 extra_header_value, resource_type));
}

void OfflinePageRequestJobTest::ReadCompletedOnIO(int bytes_read) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&OfflinePageRequestJobTest::ReadCompleted,
                 base::Unretained(this), bytes_read));
}

void OfflinePageRequestJobTest::ReadCompleted(int bytes_read) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  bytes_read_ = bytes_read;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
}

TEST_F(OfflinePageRequestJobTest, FailedToCreateRequestJob) {
  SimulateHasNetworkConnectivity(false);

  // Must be http/https URL.
  InterceptRequest(GURL("ftp://host/doc"), "GET", "", "",
      content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();
  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());

  InterceptRequest(GURL("file:///path/doc"), "GET", "", "",
      content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();
  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());

  // Must be GET method.
  InterceptRequest(
      kTestUrl, "POST", "", "", content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();
  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());

  InterceptRequest(
      kTestUrl, "HEAD", "", "", content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();
  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());

  // Must be main resource.
  InterceptRequest(
      kTestUrl, "POST", "", "", content::RESOURCE_TYPE_SUB_FRAME);
  base::RunLoop().Run();
  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());

  InterceptRequest(
      kTestUrl, "POST", "", "", content::RESOURCE_TYPE_IMAGE);
  base::RunLoop().Run();
  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
}

TEST_F(OfflinePageRequestJobTest, LoadOfflinePageOnDisconnectedNetwork) {
  SimulateHasNetworkConnectivity(false);

  InterceptRequest(kTestUrl, "GET", "", "", content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(kTestFileSize2, bytes_read());
  ASSERT_TRUE(offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(offline_id2(),
            offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK);
}

TEST_F(OfflinePageRequestJobTest, PageNotFoundOnDisconnectedNetwork) {
  SimulateHasNetworkConnectivity(false);

  InterceptRequest(kTestUrl2, "GET", "", "", content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          PAGE_NOT_FOUND_ON_DISCONNECTED_NETWORK);
}

TEST_F(OfflinePageRequestJobTest, LoadOfflinePageOnProhibitivelySlowNetwork) {
  SimulateHasNetworkConnectivity(true);

  test_previews_decider()->set_should_allow_preview(true);

  InterceptRequest(kTestUrl, "GET", "", "", content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(kTestFileSize2, bytes_read());
  ASSERT_TRUE(offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(offline_id2(),
            offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_PROHIBITIVELY_SLOW_NETWORK);
  test_previews_decider()->set_should_allow_preview(false);
}

TEST_F(OfflinePageRequestJobTest, PageNotFoundOnProhibitivelySlowNetwork) {
  SimulateHasNetworkConnectivity(true);

  test_previews_decider()->set_should_allow_preview(true);

  InterceptRequest(kTestUrl2, "GET", "", "", content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          PAGE_NOT_FOUND_ON_PROHIBITIVELY_SLOW_NETWORK);
  test_previews_decider()->set_should_allow_preview(false);
}

TEST_F(OfflinePageRequestJobTest, LoadOfflinePageOnFlakyNetwork) {
  SimulateHasNetworkConnectivity(true);

  // When custom offline header exists and contains "reason=error", it means
  // that net error is hit in last request due to flaky network.
  InterceptRequest(
      kTestUrl,
      "GET",
      kOfflinePageHeader,
      std::string(kOfflinePageHeaderReasonKey) + "=" +
          kOfflinePageHeaderReasonValueDueToNetError,
      content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(kTestFileSize2, bytes_read());
  ASSERT_TRUE(offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(offline_id2(),
            offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_FLAKY_NETWORK);
}

TEST_F(OfflinePageRequestJobTest, PageNotFoundOnFlakyNetwork) {
  SimulateHasNetworkConnectivity(true);

  // When custom offline header exists and contains "reason=error", it means
  // that net error is hit in last request due to flaky network.
  InterceptRequest(
      kTestUrl2,
      "GET",
      kOfflinePageHeader,
      std::string(kOfflinePageHeaderReasonKey) + "=" +
          kOfflinePageHeaderReasonValueDueToNetError,
      content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          PAGE_NOT_FOUND_ON_FLAKY_NETWORK);
}

TEST_F(OfflinePageRequestJobTest, ForceLoadOfflinePageOnConnectedNetwork) {
  SimulateHasNetworkConnectivity(true);

  // When custom offline header exists and contains value other than
  // "reason=error", it means that offline page is forced to load.
  InterceptRequest(
      kTestUrl,
      "GET",
      kOfflinePageHeader,
      std::string(kOfflinePageHeaderReasonKey) + "=download",
      content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(kTestFileSize2, bytes_read());
  ASSERT_TRUE(offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(offline_id2(),
            offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_CONNECTED_NETWORK);
}

TEST_F(OfflinePageRequestJobTest, PageNotFoundOnConnectedNetwork) {
  SimulateHasNetworkConnectivity(true);

  // When custom offline header exists and contains value other than
  // "reason=error", it means that offline page is forced to load.
  InterceptRequest(
      kTestUrl2,
      "GET",
      kOfflinePageHeader,
      std::string(kOfflinePageHeaderReasonKey) + "=download",
      content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          PAGE_NOT_FOUND_ON_CONNECTED_NETWORK);
}

TEST_F(OfflinePageRequestJobTest, DoNotLoadOfflinePageOnConnectedNetwork) {
  SimulateHasNetworkConnectivity(true);

  InterceptRequest(kTestUrl, "GET", "", "", content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
}

TEST_F(OfflinePageRequestJobTest, LoadOfflinePageByOfflineID) {
  SimulateHasNetworkConnectivity(true);

  InterceptRequest(
      kTestUrl,
      "GET",
      kOfflinePageHeader,
      std::string(kOfflinePageHeaderReasonKey) + "=download " +
          kOfflinePageHeaderIDKey + "=" + base::Int64ToString(offline_id()),
      content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(kTestFileSize, bytes_read());
  ASSERT_TRUE(offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(offline_id(),
            offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_CONNECTED_NETWORK);
}

TEST_F(OfflinePageRequestJobTest,
       LoadOfflinePageByOfflineIDAndFallbackToOnlineURL) {
  SimulateHasNetworkConnectivity(true);

  // The offline page found with specific offline ID does not match the passed
  // online URL. Should fall back to find the offline page based on the online
  // URL.
  InterceptRequest(
      kTestUrl2,
      "GET",
      kOfflinePageHeader,
      std::string(kOfflinePageHeaderReasonKey) + "=download " +
          kOfflinePageHeaderIDKey + "=" + base::Int64ToString(offline_id()),
      content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          PAGE_NOT_FOUND_ON_CONNECTED_NETWORK);
}

TEST_F(OfflinePageRequestJobTest, LoadOfflinePageForUrlWithFragment) {
  SimulateHasNetworkConnectivity(false);

  // Loads an url with fragment, that will match the offline URL without the
  // fragment.
  GURL url_with_fragment(kTestUrl.spec() + "#ref");
  InterceptRequest(
      url_with_fragment, "GET", "", "", content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(kTestFileSize2, bytes_read());
  ASSERT_TRUE(offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(offline_id2(),
            offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK);

  // Loads an url without fragment, that will match the offline URL with the
  // fragment.
  InterceptRequest(kTestUrl3, "GET", "", "", content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(kTestFileSize3, bytes_read());
  ASSERT_TRUE(offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(offline_id3(),
            offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);
  ExpectMultiUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK, 2);

  // Loads an url with fragment, that will match the offline URL with different
  // fragment.
  GURL url3_with_different_fragment(kTestUrl3.spec() + "#different_ref");
  InterceptRequest(url3_with_different_fragment, "GET", "", "",
                   content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(kTestFileSize3, bytes_read());
  ASSERT_TRUE(offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(offline_id3(),
            offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);
  ExpectMultiUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK, 3);

}

TEST_F(OfflinePageRequestJobTest, LoadOfflinePageAfterRedirect) {
  SimulateHasNetworkConnectivity(false);

  // This should trigger redirect first.
  InterceptRequest(
      kTestOriginalUrl, "GET", "", "", content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(kTestFileSize3, bytes_read());
  ASSERT_TRUE(offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(offline_id3(),
            offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);
  ExpectOneNonuniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          REDIRECTED_ON_DISCONNECTED_NETWORK);
  ExpectOneNonuniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK);
}

TEST_F(OfflinePageRequestJobTest, LoadOfflinePageFromNonExistentFile) {
  SimulateHasNetworkConnectivity(false);

  InterceptRequest(kTestUrl4, "GET", "", "", content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_read());
  ASSERT_TRUE(offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(offline_id4(),
            offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK);
  ExpectOpenFileErrorCode(net::ERR_FILE_NOT_FOUND);
}

}  // namespace offline_pages
