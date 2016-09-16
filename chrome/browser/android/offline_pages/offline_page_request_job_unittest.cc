// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/offline_page_request_job.h"

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
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
#include "components/offline_pages/client_namespace_constants.h"
#include "components/offline_pages/offline_page_model_impl.h"
#include "components/previews/previews_experiments.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/nqe/network_quality_estimator.h"
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
const ClientId kTestClientId = ClientId(kBookmarkNamespace, "1234");
const ClientId kTestClientId2 = ClientId(kDownloadNamespace, "1a2b3c4d");
const int kTestFileSize = 444;
const int kTestFileSize2 = 450;
const int kTabId = 1;
const int kBufSize = 1024;
const char kAggregatedRequestResultHistogram[] =
    "OfflinePages.AggregatedRequestResult";

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

  void OnResponseStarted(net::URLRequest* request) override {
    if (!request->status().is_success()) {
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

class TestNetworkQualityEstimator : public net::NetworkQualityEstimator {
 public:
  explicit TestNetworkQualityEstimator(
      const std::map<std::string, std::string>& variation_params)
      : NetworkQualityEstimator(
            std::unique_ptr<net::ExternalEstimateProvider>(),
            variation_params),
        effective_connection_type_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {}
  ~TestNetworkQualityEstimator() override {}

  net::EffectiveConnectionType GetEffectiveConnectionType() const override {
    return effective_connection_type_;
  }

  void set_effective_connection_type(
      net::EffectiveConnectionType effective_connection_type) {
    effective_connection_type_ = effective_connection_type;
  }

 private:
  net::EffectiveConnectionType effective_connection_type_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkQualityEstimator);
};

class ScopedEnableProbihibitivelySlowNetwork {
 public:
  explicit ScopedEnableProbihibitivelySlowNetwork(
      net::URLRequestContext* url_request_context)
      : field_trial_list_(nullptr),
        url_request_context_(url_request_context) {
    previews::EnableOfflinePreviewsForTesting();

    test_network_quality_estimator_ =
        base::MakeUnique<TestNetworkQualityEstimator>
            (network_quality_estimator_params_);
    test_network_quality_estimator_->set_effective_connection_type(
        net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
    url_request_context_->set_network_quality_estimator(
        test_network_quality_estimator_.get());
  }

  ~ScopedEnableProbihibitivelySlowNetwork() {
    url_request_context_->set_network_quality_estimator(nullptr);
    variations::testing::ClearAllVariationParams();
  }

 private:
  base::FieldTrialList field_trial_list_;
  std::map<std::string, std::string> network_quality_estimator_params_;
  std::unique_ptr<TestNetworkQualityEstimator> test_network_quality_estimator_;
  net::URLRequestContext* url_request_context_;
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
                     int64_t archive_id,
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

  void InterceptRequest(const GURL& url,
                        const std::string& method,
                        const std::string& extra_header_name,
                        const std::string& extra_header_value,
                        content::ResourceType resource_type);

  void ExpectAggregatedRequestResultHistogram(
      OfflinePageRequestJob::AggregatedRequestResult result);

  net::TestURLRequestContext* url_request_context() {
    return test_url_request_context_.get();
  }
  Profile* profile() { return profile_; }
  OfflinePageTabHelper* offline_page_tab_helper() const {
    return offline_page_tab_helper_;
  }
  int64_t offline_id() const { return offline_id_; }
  int64_t offline_id2() const { return offline_id2_; }
  int bytes_read() const { return bytes_read_; }

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
  net::TestNetworkDelegate network_delegate_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  base::HistogramTester histogram_tester_;
  OfflinePageTabHelper* offline_page_tab_helper_;  // Not owned.
  std::unique_ptr<net::URLRequest> request_;
  int64_t offline_id_;
  int64_t offline_id2_;
  int bytes_read_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageRequestJobTest);
};

OfflinePageRequestJobTest::OfflinePageRequestJobTest()
    : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
      network_change_notifier_(new TestNetworkChangeNotifier()),
      profile_manager_(TestingBrowserProcess::GetGlobal()),
      offline_id_(-1),
      offline_id2_(-1),
      bytes_read_(0)  {
}

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

  model->SavePage(
      kTestUrl, kTestClientId, 0, std::move(archiver),
      base::Bind(&OfflinePageRequestJobTest::OnSavePageDone,
                base::Unretained(this)));
  RunUntilIdle();

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

  model->SavePage(
      kTestUrl, kTestClientId2, 0, std::move(archiver2),
      base::Bind(&OfflinePageRequestJobTest::OnSavePageDone,
                base::Unretained(this)));
  RunUntilIdle();

  // Create a context with delayed initialization.
  test_url_request_context_.reset(new net::TestURLRequestContext(true));

  // Install the interceptor.
  std::unique_ptr<net::URLRequestInterceptor> interceptor(
      new OfflinePageRequestInterceptor());
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
      request.get(),
      resource_type,
      nullptr,
      1,     /* render_process_id */
      -1,    /* render_view_id */
      1,     /* render_frame_id */
      true,  /* is_main_frame */
      false, /* parent_is_main_frame */
      true,  /* allow_download */
      true,  /* is_async */
      false  /* is_using_lofi */);

  return request;
}

void OfflinePageRequestJobTest::ExpectAggregatedRequestResultHistogram(
    OfflinePageRequestJob::AggregatedRequestResult result) {
  histogram_tester_.ExpectUniqueSample(
      kAggregatedRequestResultHistogram, static_cast<int>(result), 1);
}

void OfflinePageRequestJobTest::OnSavePageDone(SavePageResult result,
                                               int64_t offline_id) {
  ASSERT_EQ(SavePageResult::SUCCESS, result);
  if (offline_id_ == -1)
    offline_id_ = offline_id;
  else if (offline_id2_ == -1)
    offline_id2_ = offline_id;
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
  ExpectAggregatedRequestResultHistogram(
      OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK);
}

TEST_F(OfflinePageRequestJobTest, PageNotFoundOnDisconnectedNetwork) {
  SimulateHasNetworkConnectivity(false);

  InterceptRequest(kTestUrl2, "GET", "", "", content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
  ExpectAggregatedRequestResultHistogram(
      OfflinePageRequestJob::AggregatedRequestResult::
          PAGE_NOT_FOUND_ON_DISCONNECTED_NETWORK);
}

TEST_F(OfflinePageRequestJobTest, LoadOfflinePageOnProhibitivelySlowNetwork) {
  SimulateHasNetworkConnectivity(true);

  ScopedEnableProbihibitivelySlowNetwork scoped(url_request_context());

  InterceptRequest(kTestUrl, "GET", "", "", content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(kTestFileSize2, bytes_read());
  ASSERT_TRUE(offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(offline_id2(),
            offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);
  ExpectAggregatedRequestResultHistogram(
      OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_PROHIBITIVELY_SLOW_NETWORK);
}

TEST_F(OfflinePageRequestJobTest, PageNotFoundOnProhibitivelySlowNetwork) {
  SimulateHasNetworkConnectivity(true);

  ScopedEnableProbihibitivelySlowNetwork scoped(url_request_context());

  InterceptRequest(kTestUrl2, "GET", "", "", content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
  ExpectAggregatedRequestResultHistogram(
      OfflinePageRequestJob::AggregatedRequestResult::
          PAGE_NOT_FOUND_ON_PROHIBITIVELY_SLOW_NETWORK);
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
  ExpectAggregatedRequestResultHistogram(
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
  ExpectAggregatedRequestResultHistogram(
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
  ExpectAggregatedRequestResultHistogram(
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
  ExpectAggregatedRequestResultHistogram(
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
  ExpectAggregatedRequestResultHistogram(
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
  ExpectAggregatedRequestResultHistogram(
      OfflinePageRequestJob::AggregatedRequestResult::
          PAGE_NOT_FOUND_ON_CONNECTED_NETWORK);
}

}  // namespace offline_pages
