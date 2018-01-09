// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_request_job.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

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
#include "base/time/default_clock.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/offline_pages/offline_page_request_interceptor.h"
#include "chrome/browser/offline_pages/offline_page_tab_helper.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/model/offline_page_model_taskified.h"
#include "components/offline_pages/core/offline_page_metadata_store_sql.h"
#include "components/offline_pages/core/request_header/offline_page_navigation_ui_data.h"
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

const char kPrivateOfflineFileDir[] = "offline_pages";
const char kPublicOfflineFileDir[] = "public_offline_pages";

const GURL kTestUrl1("http://test.org/page1");
const GURL kTestUrl2("http://test.org/page2");
const GURL kTestUrl3("http://test.org/page3");
const GURL kTestUrl3WithFragment("http://test.org/page3#ref1");
const GURL kTestUrl4("http://test.org/page4");
const GURL kTestUrl5("http://test.org/page5");
const GURL kTestUrl6("http://test.org/page6");
const GURL kTestUrl7("http://test.org/page7");
const GURL kTestUrlRedirectsTo3("http://test.org/first");
const GURL kTestUrl8("http://test.org/page8");
const GURL kTestUrl9("http://test.org/page9");

const ClientId kTestClientId1 = ClientId(kBookmarkNamespace, "1234");
const ClientId kTestClientId2 = ClientId(kDownloadNamespace, "1a2b3c4d");
const ClientId kTestClientId3 = ClientId(kAsyncNamespace, "3456abcd");
const ClientId kTestClientId4 = ClientId(kNTPSuggestionsNamespace, "5678");
const ClientId kTestClientId5 = ClientId(kBrowserActionsNamespace, "9999");
const ClientId kTestClientId6 = ClientId(kDownloadNamespace, "a1");
const ClientId kTestClientId7 = ClientId(kDownloadNamespace, "b2");
const ClientId kTestClientId8 = ClientId(kDownloadNamespace, "c3");
const ClientId kTestClientId9 = ClientId(kDownloadNamespace, "d4");
const ClientId kTestClientId10 = ClientId(kDownloadNamespace, "e5");

// Note: as most file size values are below 1 KiB, the file size samples added
// to page size histograms will mostly fall into the 0-sized bucket. So when
// checking for correct reporting a 0 will be directly used instead of the
// specific file size constant divided by 1024.
const int kTestFileSize1 = 444;  // Real size of test.mhtml.
const int kTestFileSize2 = 450;  // Real size of hello.mhtml.
const int kTestFileSize3 = 450;  // Real size of hello.mhtml.
const int kTestFileSize4NonExistent = 9999;
const int kTestFileSize5 = 450;           // Real size of hello.mhtml.
const int kTestFileSize6Mismatch = 1450;  // Wrong size of hello.mhtml.
const int kTestFileSize7 = 450;           // Real size of hello.mhtml.
const int kTestFileSize8 = 450;           // Real size of hello.mhtml.
const int kTestFileSize9 = 444;           // Real size of test.mhtml.
const int kTestFileSize10 = 450;          // Real size of hello.mhtml.

const std::string kTestDigest2(
    "\x90\x64\xF9\x7C\x94\xE5\x9E\x91\x83\x3D\x41\xB0\x36\x90\x0A\xDF\xB3\xB1"
    "\x5C\x13\xBE\xB8\x35\x8C\xF6\x5B\xC4\xB5\x5A\xFC\x3A\xCC",
    32);                                       // SHA256 Hash of hello.mhtml.
const std::string kTestDigest6(kTestDigest2);  // SHA256 Hash of hello.mhtml.
const std::string kTestDigest7Mismatch(
    "\xff\x64\xF9\x7C\x94\xE5\x9E\x91\x83\x3D\x41\xB0\x36\x90\x0A\xDF\xB3\xB1"
    "\x5C\x13\xBE\xB8\x35\x8C\xF6\x5B\xC4\xB5\x5A\xFC\x3A\xCC",
    32);  // Wrong SHA256 Hash of hello.mhtml.
const std::string kTestDigest10Mismatch(
    "\xff\x64\xF9\x7C\x94\xE5\x9E\x91\x83\x3D\x41\xB0\x36\x90\x0A\xDF\xB3\xB1"
    "\x5C\x13\xBE\xB8\x35\x8C\xF6\x5B\xC4\xB5\x5A\xFC\x3A\xCC",
    32);  // Wrong SHA256 Hash of hello.mhtml.

const int kTabId = 1;
const int kBufSize = 1024;

const char kAggregatedRequestResultHistogram[] =
    "OfflinePages.AggregatedRequestResult2";
const char kOpenFileErrorCodeHistogram[] =
    "OfflinePages.RequestJob.OpenFileErrorCode";
const char kAccessEntryPointHistogram[] = "OfflinePages.AccessEntryPoint.";
const char kPageSizeAccessOfflineHistogramBase[] =
    "OfflinePages.PageSizeOnAccess.Offline.";
const char kPageSizeAccessOnlineHistogramBase[] =
    "OfflinePages.PageSizeOnAccess.Online.";

class OfflinePageRequestJobTestDelegate
    : public OfflinePageRequestJob::Delegate {
 public:
  OfflinePageRequestJobTestDelegate(content::WebContents* web_content,
                                    int tab_id)
      : web_content_(web_content), tab_id_(tab_id) {}

  content::ResourceRequestInfo::WebContentsGetter GetWebContentsGetter(
      net::URLRequest* request) const override {
    return base::Bind(&OfflinePageRequestJobTestDelegate::GetWebContents,
                      web_content_);
  }

  OfflinePageRequestJob::Delegate::TabIdGetter GetTabIdGetter() const override {
    return base::Bind(&OfflinePageRequestJobTestDelegate::GetTabId, tab_id_);
  }

 private:
  static content::WebContents* GetWebContents(
      content::WebContents* web_content) {
    return web_content;
  }

  static bool GetTabId(int tab_id_value,
                       content::WebContents* web_content,
                       int* tab_id) {
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
      OnReadCompleted(request, 0);
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
      : net::URLRequestInterceptingJobFactory(std::move(job_factory),
                                              std::move(interceptor)),
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
          base::MakeUnique<OfflinePageRequestJobTestDelegate>(web_contents_,
                                                              kTabId));
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

  bool ShouldAllowPreviewAtECT(
      const net::URLRequest& request,
      previews::PreviewsType type,
      net::EffectiveConnectionType effective_connection_type_threshold,
      const std::vector<std::string>& host_blacklist_from_server)
      const override {
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
                          int archive_file_size,
                          const std::string& digest)
      : url_(url),
        archive_file_path_(archive_file_path),
        archive_file_size_(archive_file_size),
        digest_(digest) {}
  ~TestOfflinePageArchiver() override {}

  void CreateArchive(const base::FilePath& archives_dir,
                     const CreateArchiveParams& create_archive_params,
                     const CreateArchiveCallback& callback) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, this, ArchiverResult::SUCCESSFULLY_CREATED, url_,
                   archive_file_path_, base::string16(), archive_file_size_,
                   digest_));
  }

 private:
  const GURL url_;
  const base::FilePath archive_file_path_;
  const int archive_file_size_;
  const std::string digest_;

  DISALLOW_COPY_AND_ASSIGN(TestOfflinePageArchiver);
};

std::unique_ptr<KeyedService> BuildTestOfflinePageModel(
    content::BrowserContext* context) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::ThreadTaskRunnerHandle::Get();

  base::FilePath store_path =
      context->GetPath().Append(chrome::kOfflinePageMetadataDirname);
  std::unique_ptr<OfflinePageMetadataStoreSQL> metadata_store(
      new OfflinePageMetadataStoreSQL(task_runner, store_path));

  base::FilePath test_data_dir_path;
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_path);
  base::FilePath private_archives_dir =
      test_data_dir_path.AppendASCII("offline_pages");
  base::FilePath public_archives_dir(FILE_PATH_LITERAL("/sdcard/Download"));

  // We're not interested in saving any temporary file in this test.
  base::FilePath temporary_archives_dir;

  std::unique_ptr<ArchiveManager> archive_manager(
      new ArchiveManager(temporary_archives_dir, private_archives_dir,
                         public_archives_dir, task_runner));
  std::unique_ptr<base::Clock> clock(new base::DefaultClock);

  return std::unique_ptr<KeyedService>(new OfflinePageModelTaskified(
      std::move(metadata_store), std::move(archive_manager), task_runner,
      std::move(clock)));
}

}  // namespace

class OfflinePageRequestJobTest : public testing::Test {
 public:
  OfflinePageRequestJobTest();
  ~OfflinePageRequestJobTest() override {}

  void SetUp() override;
  void TearDown() override;

  void SimulateHasNetworkConnectivity(bool has_connectivity);
  void RunUntilIdle();
  void WaitForAsyncOperation();

  void SavePage(const GURL& url,
                const ClientId& client_id,
                const GURL& original_url,
                std::unique_ptr<OfflinePageArchiver> archiver);

  OfflinePageItem GetPage(int64_t offline_id);

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
      OfflinePageRequestJob::AggregatedRequestResult result,
      int count);
  // Expect one count of |result| UMA reported. Other buckets may have samples
  // as well.
  void ExpectOneNonuniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult result);
  // Expect no samples to have been reported to the aggregated results
  // histogram.
  void ExpectNoSamplesInAggregatedRequestResult();

  void ExpectOpenFileErrorCode(int result);

  void ExpectAccessEntryPoint(
      const ClientId& client_id,
      OfflinePageRequestJob::AccessEntryPoint entry_point);
  void ExpectNoAccessEntryPoint();

  void ExpectOfflinePageSizeUniqueSample(ClientId client_id,
                                         int bucket,
                                         int count);
  void ExpectOfflinePageSizeTotalSuffixCount(int count);
  void ExpectOnlinePageSizeUniqueSample(ClientId client_id,
                                        int bucket,
                                        int count);
  void ExpectOnlinePageSizeTotalSuffixCount(int count);

  net::TestURLRequestContext* url_request_context() {
    return test_url_request_context_.get();
  }
  Profile* profile() { return profile_; }
  OfflinePageTabHelper* offline_page_tab_helper() const {
    return offline_page_tab_helper_;
  }
  const std::vector<int64_t>& offline_ids() const { return offline_ids_; }
  int bytes_read() const { return bytes_read_; }
  bool is_offline_page_set_in_navigation_data() const {
    return is_offline_page_set_in_navigation_data_;
  }

  TestPreviewsDecider* test_previews_decider() {
    return test_previews_decider_.get();
  }

 private:
  void OnSavePageDone(SavePageResult result, int64_t offline_id);
  std::unique_ptr<net::URLRequest> CreateRequest(
      const GURL& url,
      const std::string& method,
      content::ResourceType resource_type);
  void OnGetPageByOfflineIdDone(const OfflinePageItem* pages);
  void ReadCompleted(int bytes_read,
                     bool is_offline_page_set_in_navigation_data);

  // Runs on IO thread.
  void SetUpNetworkObjectsOnIO();
  void TearDownNetworkObjectsOnIO();
  void InterceptRequestOnIO(const GURL& url,
                            const std::string& method,
                            const std::string& extra_header_name,
                            const std::string& extra_header_value,
                            content::ResourceType resource_type);
  void ReadCompletedOnIO(int bytes_read);
  void TearDownOnReadCompletedOnIO(int bytes_read,
                                   bool is_offline_page_set_in_navigation_data);

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<base::SimpleTestClock> clock_;
  base::SimpleTestClock* clock_ptr_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  base::HistogramTester histogram_tester_;
  OfflinePageTabHelper* offline_page_tab_helper_;  // Not owned.
  std::vector<int64_t> offline_ids_;
  int bytes_read_;
  bool is_offline_page_set_in_navigation_data_;
  OfflinePageItem page_;

  // These are not thread-safe. But they can be used in the pattern that
  // setting the state is done first from one thread and reading this state
  // can be from any other thread later.
  std::unique_ptr<TestNetworkChangeNotifier> network_change_notifier_;
  std::unique_ptr<TestPreviewsDecider> test_previews_decider_;

  // These should only be accessed purely from IO thread.
  std::unique_ptr<net::TestURLRequestContext> test_url_request_context_;
  std::unique_ptr<net::URLRequestJobFactoryImpl> url_request_job_factory_;
  std::unique_ptr<net::URLRequestInterceptingJobFactory>
      intercepting_job_factory_;
  std::unique_ptr<TestURLRequestDelegate> url_request_delegate_;
  std::unique_ptr<net::URLRequest> request_;

  bool async_operation_completed_ = false;
  base::Closure async_operation_completed_callback_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageRequestJobTest);
};

OfflinePageRequestJobTest::OfflinePageRequestJobTest()
    : thread_bundle_(content::TestBrowserThreadBundle::REAL_IO_THREAD),
      clock_(new base::SimpleTestClock),
      clock_ptr_(clock_.get()),
      profile_manager_(TestingBrowserProcess::GetGlobal()),
      bytes_read_(0),
      is_offline_page_set_in_navigation_data_(false),
      network_change_notifier_(new TestNetworkChangeNotifier),
      test_previews_decider_(new TestPreviewsDecider) {}

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

  OfflinePageModelTaskified* model = static_cast<OfflinePageModelTaskified*>(
      OfflinePageModelFactory::GetForBrowserContext(profile()));

  // Hook up a test clock such that we can control the time when the offline
  // page is created.
  clock_ptr_->SetNow(base::Time::Now());
  model->SetClockForTesting(std::move(clock_));

  // Skip the logic to clear the original URL if it is same as final URL.
  // This is needed in order to test that offline page request handler can
  // omit the redirect under this circumstance, for compatibility with the
  // metadata already written to the store.
  model->SetSkipClearingOriginalUrlForTesting();

  // All offline pages being created below will point to real archive files
  // residing in test data directory.
  base::FilePath test_data_dir_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_path));

  // Save an offline page.
  base::FilePath archive_file_path =
      test_data_dir_path.AppendASCII(kPrivateOfflineFileDir)
          .AppendASCII("test.mhtml");
  std::unique_ptr<TestOfflinePageArchiver> archiver(new TestOfflinePageArchiver(
      kTestUrl1, archive_file_path, kTestFileSize1, std::string()));

  SavePage(kTestUrl1, kTestClientId1, GURL(), std::move(archiver));

  // Save another offline page associated with same online URL as above, but
  // pointing to different archive file.
  base::FilePath archive_file_path2 =
      test_data_dir_path.AppendASCII(kPrivateOfflineFileDir)
          .AppendASCII("hello.mhtml");
  std::unique_ptr<TestOfflinePageArchiver> archiver2(
      new TestOfflinePageArchiver(kTestUrl1, archive_file_path2, kTestFileSize2,
                                  kTestDigest2));

  // Make sure that the creation time of 2nd offline file is later.
  clock_ptr_->Advance(base::TimeDelta::FromMinutes(10));

  SavePage(kTestUrl1, kTestClientId2, GURL(), std::move(archiver2));

  // Save an offline page associated with online URL that has a fragment
  // identifier.
  base::FilePath archive_file_path3 =
      test_data_dir_path.AppendASCII(kPrivateOfflineFileDir)
          .AppendASCII("hello.mhtml");
  std::unique_ptr<TestOfflinePageArchiver> archiver3(
      new TestOfflinePageArchiver(kTestUrl3WithFragment, archive_file_path3,
                                  kTestFileSize3, std::string()));

  SavePage(kTestUrl3WithFragment, kTestClientId3, kTestUrlRedirectsTo3,
           std::move(archiver3));

  // Save an offline page pointing to non-existent archive file.
  base::FilePath archive_file_path4 =
      test_data_dir_path.AppendASCII(kPrivateOfflineFileDir)
          .AppendASCII("nonexistent.mhtml");
  std::unique_ptr<TestOfflinePageArchiver> archiver4(
      new TestOfflinePageArchiver(kTestUrl4, archive_file_path4,
                                  kTestFileSize4NonExistent, std::string()));

  SavePage(kTestUrl4, kTestClientId4, GURL(), std::move(archiver4));

  // Save an offline page with same original URL and final URL.
  base::FilePath archive_file_path5 =
      test_data_dir_path.AppendASCII(kPrivateOfflineFileDir)
          .AppendASCII("hello.mhtml");
  std::unique_ptr<TestOfflinePageArchiver> archiver5(
      new TestOfflinePageArchiver(kTestUrl5, archive_file_path5, kTestFileSize5,
                                  std::string()));

  SavePage(kTestUrl5, kTestClientId5, kTestUrl5, std::move(archiver5));

  // Check if the original URL is still present.
  OfflinePageItem page = GetPage(offline_ids()[5]);
  EXPECT_EQ(kTestUrl5, page.original_url);

  // Save an offline page in public location with mismatched file size.
  base::FilePath archive_file_path6 =
      test_data_dir_path.AppendASCII(kPublicOfflineFileDir)
          .AppendASCII("hello.mhtml");
  std::unique_ptr<TestOfflinePageArchiver> archiver6(
      new TestOfflinePageArchiver(kTestUrl6, archive_file_path6,
                                  kTestFileSize6Mismatch, kTestDigest6));

  SavePage(kTestUrl6, kTestClientId6, GURL(), std::move(archiver6));

  // Save an offline page in public location with mismatched digest.
  base::FilePath archive_file_path7 =
      test_data_dir_path.AppendASCII(kPublicOfflineFileDir)
          .AppendASCII("hello.mhtml");
  std::unique_ptr<TestOfflinePageArchiver> archiver7(
      new TestOfflinePageArchiver(kTestUrl7, archive_file_path7, kTestFileSize7,
                                  kTestDigest7Mismatch));

  SavePage(kTestUrl7, kTestClientId7, GURL(), std::move(archiver7));

  // Save an offline page in public location with no digest.
  base::FilePath archive_file_path8 =
      test_data_dir_path.AppendASCII(kPublicOfflineFileDir)
          .AppendASCII("hello.mhtml");
  std::unique_ptr<TestOfflinePageArchiver> archiver8(
      new TestOfflinePageArchiver(kTestUrl8, archive_file_path8, kTestFileSize8,
                                  std::string()));

  SavePage(kTestUrl8, kTestClientId8, GURL(), std::move(archiver8));

  // Save 2 offline pages associated with same online URL, one in private
  // location, while another in public location with mismatched digest.
  base::FilePath archive_file_path9 =
      test_data_dir_path.AppendASCII(kPrivateOfflineFileDir)
          .AppendASCII("test.mhtml");
  std::unique_ptr<TestOfflinePageArchiver> archiver9(
      new TestOfflinePageArchiver(kTestUrl9, archive_file_path9, kTestFileSize9,
                                  std::string()));

  SavePage(kTestUrl9, kTestClientId9, GURL(), std::move(archiver9));

  base::FilePath archive_file_path10 =
      test_data_dir_path.AppendASCII(kPublicOfflineFileDir)
          .AppendASCII("hello.mhtml");
  std::unique_ptr<TestOfflinePageArchiver> archiver10(
      new TestOfflinePageArchiver(kTestUrl9, archive_file_path10,
                                  kTestFileSize10, kTestDigest10Mismatch));

  SavePage(kTestUrl9, kTestClientId10, GURL(), std::move(archiver10));
}

void OfflinePageRequestJobTest::TearDown() {
  OfflinePageModel* model =
      OfflinePageModelFactory::GetForBrowserContext(profile());
  static_cast<OfflinePageModelTaskified*>(model)->SetClockForTesting(nullptr);
}

void OfflinePageRequestJobTest::SimulateHasNetworkConnectivity(bool online) {
  network_change_notifier_->set_online(online);
}

void OfflinePageRequestJobTest::RunUntilIdle() {
  base::RunLoop().RunUntilIdle();
}

void OfflinePageRequestJobTest::WaitForAsyncOperation() {
  // No need to wait if async operation is not needed.
  if (async_operation_completed_)
    return;
  base::RunLoop run_loop;
  async_operation_completed_callback_ = run_loop.QuitClosure();
  run_loop.Run();
}

void OfflinePageRequestJobTest::SetUpNetworkObjectsOnIO() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (test_url_request_context_.get())
    return;

  url_request_job_factory_.reset(new net::URLRequestJobFactoryImpl);

  // Create a context with delayed initialization.
  test_url_request_context_.reset(new net::TestURLRequestContext(true));

  // Install the interceptor.
  std::unique_ptr<net::URLRequestInterceptor> interceptor(
      new OfflinePageRequestInterceptor(test_previews_decider_.get()));
  std::unique_ptr<net::URLRequestJobFactoryImpl> job_factory_impl(
      new net::URLRequestJobFactoryImpl());
  intercepting_job_factory_.reset(new TestURLRequestInterceptingJobFactory(
      std::move(job_factory_impl), std::move(interceptor),
      web_contents_.get()));

  test_url_request_context_->set_job_factory(intercepting_job_factory_.get());
  test_url_request_context_->Init();
}

void OfflinePageRequestJobTest::TearDownNetworkObjectsOnIO() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  request_.reset();
  url_request_delegate_.reset();
  intercepting_job_factory_.reset();
  url_request_job_factory_.reset();
  test_url_request_context_.reset();
}

std::unique_ptr<net::URLRequest> OfflinePageRequestJobTest::CreateRequest(
    const GURL& url,
    const std::string& method,
    content::ResourceType resource_type) {
  url_request_delegate_ = base::MakeUnique<TestURLRequestDelegate>(base::Bind(
      &OfflinePageRequestJobTest::ReadCompletedOnIO, base::Unretained(this)));

  std::unique_ptr<net::URLRequest> request =
      url_request_context()->CreateRequest(url, net::DEFAULT_PRIORITY,
                                           url_request_delegate_.get());
  request->set_method(method);

  content::ResourceRequestInfo::AllocateForTesting(
      request.get(), resource_type, nullptr, /*render_process_id=*/1,
      /*render_view_id=*/-1,
      /*render_frame_id=*/1,
      /*is_main_frame=*/true,
      /*allow_download=*/true,
      /*is_async=*/true, content::PREVIEWS_OFF,
      std::make_unique<ChromeNavigationUIData>());

  return request;
}

void OfflinePageRequestJobTest::ExpectOneUniqueSampleForAggregatedRequestResult(
    OfflinePageRequestJob::AggregatedRequestResult result) {
  histogram_tester_.ExpectUniqueSample(kAggregatedRequestResultHistogram,
                                       static_cast<int>(result), 1);
}

void OfflinePageRequestJobTest::
    ExpectMultiUniqueSampleForAggregatedRequestResult(
        OfflinePageRequestJob::AggregatedRequestResult result,
        int count) {
  histogram_tester_.ExpectUniqueSample(kAggregatedRequestResultHistogram,
                                       static_cast<int>(result), count);
}

void OfflinePageRequestJobTest::
    ExpectOneNonuniqueSampleForAggregatedRequestResult(
        OfflinePageRequestJob::AggregatedRequestResult result) {
  histogram_tester_.ExpectBucketCount(kAggregatedRequestResultHistogram,
                                      static_cast<int>(result), 1);
}

void OfflinePageRequestJobTest::ExpectNoSamplesInAggregatedRequestResult() {
  histogram_tester_.ExpectTotalCount(kAggregatedRequestResultHistogram, 0);
}

void OfflinePageRequestJobTest::ExpectOpenFileErrorCode(int result) {
  histogram_tester_.ExpectUniqueSample(kOpenFileErrorCodeHistogram, -result, 1);
}

void OfflinePageRequestJobTest::ExpectAccessEntryPoint(
    const ClientId& client_id,
    OfflinePageRequestJob::AccessEntryPoint entry_point) {
  histogram_tester_.ExpectUniqueSample(
      kAccessEntryPointHistogram + client_id.name_space,
      static_cast<int>(entry_point), 1);
}

void OfflinePageRequestJobTest::ExpectNoAccessEntryPoint() {
  EXPECT_TRUE(
      histogram_tester_.GetTotalCountsForPrefix(kAccessEntryPointHistogram)
          .empty());
}

void OfflinePageRequestJobTest::ExpectOfflinePageSizeUniqueSample(
    ClientId client_id,
    int bucket,
    int count) {
  histogram_tester_.ExpectUniqueSample(
      kPageSizeAccessOfflineHistogramBase + client_id.name_space, bucket,
      count);
}

void OfflinePageRequestJobTest::ExpectOfflinePageSizeTotalSuffixCount(
    int count) {
  int total_offline_count = 0;
  base::HistogramTester::CountsMap all_offline_counts =
      histogram_tester_.GetTotalCountsForPrefix(
          kPageSizeAccessOfflineHistogramBase);
  for (const std::pair<std::string, base::HistogramBase::Count>&
           namespace_and_count : all_offline_counts) {
    total_offline_count += namespace_and_count.second;
  }
  EXPECT_EQ(count, total_offline_count)
      << "Wrong histogram samples count under prefix "
      << kPageSizeAccessOfflineHistogramBase << "*";
}

void OfflinePageRequestJobTest::ExpectOnlinePageSizeUniqueSample(
    ClientId client_id,
    int bucket,
    int count) {
  histogram_tester_.ExpectUniqueSample(
      kPageSizeAccessOnlineHistogramBase + client_id.name_space, bucket, count);
}

void OfflinePageRequestJobTest::ExpectOnlinePageSizeTotalSuffixCount(
    int count) {
  int online_count = 0;
  base::HistogramTester::CountsMap all_online_counts =
      histogram_tester_.GetTotalCountsForPrefix(
          kPageSizeAccessOnlineHistogramBase);
  for (const std::pair<std::string, base::HistogramBase::Count>&
           namespace_and_count : all_online_counts) {
    online_count += namespace_and_count.second;
  }
  EXPECT_EQ(count, online_count)
      << "Wrong histogram samples count under prefix "
      << kPageSizeAccessOnlineHistogramBase << "*";
}

void OfflinePageRequestJobTest::SavePage(
    const GURL& url,
    const ClientId& client_id,
    const GURL& original_url,
    std::unique_ptr<OfflinePageArchiver> archiver) {
  async_operation_completed_ = false;
  OfflinePageModel::SavePageParams save_page_params;
  save_page_params.url = url;
  save_page_params.client_id = client_id;
  save_page_params.original_url = original_url;
  OfflinePageModelFactory::GetForBrowserContext(profile())->SavePage(
      save_page_params, std::move(archiver),
      base::Bind(&OfflinePageRequestJobTest::OnSavePageDone,
                 base::Unretained(this)));
  WaitForAsyncOperation();
}

void OfflinePageRequestJobTest::OnSavePageDone(SavePageResult result,
                                               int64_t offline_id) {
  ASSERT_EQ(SavePageResult::SUCCESS, result);
  // Make a dummy item at index 0 to make offline_ids_ look like to start from
  // index 1 in order to match all the indices used in constants.
  if (offline_ids_.empty())
    offline_ids_.push_back(0);
  offline_ids_.push_back(offline_id);

  async_operation_completed_ = true;
  if (!async_operation_completed_callback_.is_null())
    async_operation_completed_callback_.Run();
}

OfflinePageItem OfflinePageRequestJobTest::GetPage(int64_t offline_id) {
  OfflinePageModelFactory::GetForBrowserContext(profile())->GetPageByOfflineId(
      offline_id,
      base::Bind(&OfflinePageRequestJobTest::OnGetPageByOfflineIdDone,
                 base::Unretained(this)));
  RunUntilIdle();
  return page_;
}

void OfflinePageRequestJobTest::OnGetPageByOfflineIdDone(
    const OfflinePageItem* page) {
  ASSERT_TRUE(page);
  page_ = *page;
}

void OfflinePageRequestJobTest::InterceptRequestOnIO(
    const GURL& url,
    const std::string& method,
    const std::string& extra_header_name,
    const std::string& extra_header_value,
    content::ResourceType resource_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  SetUpNetworkObjectsOnIO();

  request_ = CreateRequest(url, method, resource_type);
  if (!extra_header_name.empty()) {
    request_->SetExtraRequestHeaderByName(extra_header_name, extra_header_value,
                                          true);
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

  bool is_offline_page_set_in_navigation_data = false;
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request_.get());
  ChromeNavigationUIData* navigation_data =
      static_cast<ChromeNavigationUIData*>(info->GetNavigationUIData());
  if (navigation_data) {
    offline_pages::OfflinePageNavigationUIData* offline_page_data =
        navigation_data->GetOfflinePageNavigationUIData();
    if (offline_page_data && offline_page_data->is_offline_page())
      is_offline_page_set_in_navigation_data = true;
  }

  // Since the caller is still holding a request object which we want to dispose
  // as part of tearing down on IO thread, we need to do it in a separate task.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&OfflinePageRequestJobTest::TearDownOnReadCompletedOnIO,
                 base::Unretained(this), bytes_read,
                 is_offline_page_set_in_navigation_data));
}

void OfflinePageRequestJobTest::TearDownOnReadCompletedOnIO(
    int bytes_read,
    bool is_offline_page_set_in_navigation_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  TearDownNetworkObjectsOnIO();

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&OfflinePageRequestJobTest::ReadCompleted,
                 base::Unretained(this), bytes_read,
                 is_offline_page_set_in_navigation_data));
}

void OfflinePageRequestJobTest::ReadCompleted(
    int bytes_read,
    bool is_offline_page_set_in_navigation_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  bytes_read_ = bytes_read;
  is_offline_page_set_in_navigation_data_ =
      is_offline_page_set_in_navigation_data;
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
  InterceptRequest(kTestUrl1, "POST", "", "",
                   content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();
  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());

  InterceptRequest(kTestUrl1, "HEAD", "", "",
                   content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();
  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());

  // Must be main resource.
  InterceptRequest(kTestUrl1, "POST", "", "", content::RESOURCE_TYPE_SUB_FRAME);
  base::RunLoop().Run();
  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());

  InterceptRequest(kTestUrl1, "POST", "", "", content::RESOURCE_TYPE_IMAGE);
  base::RunLoop().Run();
  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());

  ExpectNoSamplesInAggregatedRequestResult();
  ExpectOfflinePageSizeTotalSuffixCount(0);
  ExpectOnlinePageSizeTotalSuffixCount(0);
}

TEST_F(OfflinePageRequestJobTest, LoadOfflinePageOnDisconnectedNetwork) {
  SimulateHasNetworkConnectivity(false);

  InterceptRequest(kTestUrl1, "GET", "", "", content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(kTestFileSize2, bytes_read());
  EXPECT_TRUE(is_offline_page_set_in_navigation_data());
  ASSERT_TRUE(offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(offline_ids()[2],
            offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK);
  ExpectAccessEntryPoint(kTestClientId2,
                         OfflinePageRequestJob::AccessEntryPoint::LINK);
  ExpectOfflinePageSizeUniqueSample(kTestClientId2, 0, 1);
  ExpectOnlinePageSizeTotalSuffixCount(0);
}

TEST_F(OfflinePageRequestJobTest, PageNotFoundOnDisconnectedNetwork) {
  SimulateHasNetworkConnectivity(false);

  InterceptRequest(kTestUrl2, "GET", "", "", content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(is_offline_page_set_in_navigation_data());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          PAGE_NOT_FOUND_ON_DISCONNECTED_NETWORK);
  ExpectNoAccessEntryPoint();
  ExpectOfflinePageSizeTotalSuffixCount(0);
  ExpectOnlinePageSizeTotalSuffixCount(0);
}

TEST_F(OfflinePageRequestJobTest, LoadOfflinePageOnProhibitivelySlowNetwork) {
  SimulateHasNetworkConnectivity(true);

  test_previews_decider()->set_should_allow_preview(true);

  InterceptRequest(kTestUrl1, "GET", "", "", content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(kTestFileSize2, bytes_read());
  EXPECT_TRUE(is_offline_page_set_in_navigation_data());
  ASSERT_TRUE(offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(offline_ids()[2],
            offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_PROHIBITIVELY_SLOW_NETWORK);
  ExpectAccessEntryPoint(kTestClientId2,
                         OfflinePageRequestJob::AccessEntryPoint::LINK);
  ExpectOfflinePageSizeUniqueSample(kTestClientId2, 0, 1);
  ExpectOnlinePageSizeTotalSuffixCount(0);
}

TEST_F(OfflinePageRequestJobTest,
       DontLoadReloadOfflinePageOnProhibitivelySlowNetwork) {
  SimulateHasNetworkConnectivity(true);

  test_previews_decider()->set_should_allow_preview(true);

  // Treat this as a reloaded page.
  InterceptRequest(kTestUrl1, "GET", kOfflinePageHeader,
                   std::string(kOfflinePageHeaderReasonKey) + "=" +
                       kOfflinePageHeaderReasonValueReload,
                   content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(is_offline_page_set_in_navigation_data());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
  ExpectNoAccessEntryPoint();
  ExpectNoSamplesInAggregatedRequestResult();
  ExpectOfflinePageSizeTotalSuffixCount(0);
  ExpectOnlinePageSizeTotalSuffixCount(0);
}

TEST_F(OfflinePageRequestJobTest, PageNotFoundOnProhibitivelySlowNetwork) {
  SimulateHasNetworkConnectivity(true);

  test_previews_decider()->set_should_allow_preview(true);

  InterceptRequest(kTestUrl2, "GET", "", "", content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(is_offline_page_set_in_navigation_data());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          PAGE_NOT_FOUND_ON_PROHIBITIVELY_SLOW_NETWORK);
  ExpectNoAccessEntryPoint();
  ExpectOfflinePageSizeTotalSuffixCount(0);
  ExpectOnlinePageSizeTotalSuffixCount(0);
}

TEST_F(OfflinePageRequestJobTest, LoadOfflinePageOnFlakyNetwork) {
  SimulateHasNetworkConnectivity(true);

  // When custom offline header exists and contains "reason=error", it means
  // that net error is hit in last request due to flaky network.
  InterceptRequest(kTestUrl1, "GET", kOfflinePageHeader,
                   std::string(kOfflinePageHeaderReasonKey) + "=" +
                       kOfflinePageHeaderReasonValueDueToNetError,
                   content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(kTestFileSize2, bytes_read());
  EXPECT_TRUE(is_offline_page_set_in_navigation_data());
  ASSERT_TRUE(offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(offline_ids()[2],
            offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_FLAKY_NETWORK);
  ExpectAccessEntryPoint(kTestClientId2,
                         OfflinePageRequestJob::AccessEntryPoint::LINK);
  ExpectOfflinePageSizeUniqueSample(kTestClientId2, 0, 1);
  ExpectOnlinePageSizeTotalSuffixCount(0);
}

TEST_F(OfflinePageRequestJobTest, PageNotFoundOnFlakyNetwork) {
  SimulateHasNetworkConnectivity(true);

  // When custom offline header exists and contains "reason=error", it means
  // that net error is hit in last request due to flaky network.
  InterceptRequest(kTestUrl2, "GET", kOfflinePageHeader,
                   std::string(kOfflinePageHeaderReasonKey) + "=" +
                       kOfflinePageHeaderReasonValueDueToNetError,
                   content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(is_offline_page_set_in_navigation_data());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          PAGE_NOT_FOUND_ON_FLAKY_NETWORK);
  ExpectNoAccessEntryPoint();
  ExpectOfflinePageSizeTotalSuffixCount(0);
  ExpectOnlinePageSizeTotalSuffixCount(0);
}

TEST_F(OfflinePageRequestJobTest, ForceLoadOfflinePageOnConnectedNetwork) {
  SimulateHasNetworkConnectivity(true);

  // When custom offline header exists and contains value other than
  // "reason=error", it means that offline page is forced to load.
  InterceptRequest(kTestUrl1, "GET", kOfflinePageHeader,
                   std::string(kOfflinePageHeaderReasonKey) + "=download",
                   content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(kTestFileSize2, bytes_read());
  EXPECT_TRUE(is_offline_page_set_in_navigation_data());
  ASSERT_TRUE(offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(offline_ids()[2],
            offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_CONNECTED_NETWORK);
  ExpectAccessEntryPoint(kTestClientId2,
                         OfflinePageRequestJob::AccessEntryPoint::DOWNLOADS);
  ExpectOfflinePageSizeTotalSuffixCount(0);
  ExpectOnlinePageSizeUniqueSample(kTestClientId2, 0, 1);
}

TEST_F(OfflinePageRequestJobTest, PageNotFoundOnConnectedNetwork) {
  SimulateHasNetworkConnectivity(true);

  // When custom offline header exists and contains value other than
  // "reason=error", it means that offline page is forced to load.
  InterceptRequest(kTestUrl2, "GET", kOfflinePageHeader,
                   std::string(kOfflinePageHeaderReasonKey) + "=download",
                   content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(is_offline_page_set_in_navigation_data());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          PAGE_NOT_FOUND_ON_CONNECTED_NETWORK);
  ExpectNoAccessEntryPoint();
  ExpectOfflinePageSizeTotalSuffixCount(0);
  ExpectOnlinePageSizeTotalSuffixCount(0);
}

TEST_F(OfflinePageRequestJobTest, DoNotLoadOfflinePageOnConnectedNetwork) {
  SimulateHasNetworkConnectivity(true);

  InterceptRequest(kTestUrl1, "GET", "", "", content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(is_offline_page_set_in_navigation_data());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
  ExpectNoAccessEntryPoint();
  ExpectNoSamplesInAggregatedRequestResult();
  ExpectOfflinePageSizeTotalSuffixCount(0);
  ExpectOnlinePageSizeTotalSuffixCount(0);
}

TEST_F(OfflinePageRequestJobTest, LoadOfflinePageByOfflineID) {
  SimulateHasNetworkConnectivity(true);

  InterceptRequest(kTestUrl1, "GET", kOfflinePageHeader,
                   std::string(kOfflinePageHeaderReasonKey) + "=download " +
                       kOfflinePageHeaderIDKey + "=" +
                       base::Int64ToString(offline_ids()[1]),
                   content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(kTestFileSize1, bytes_read());
  EXPECT_TRUE(is_offline_page_set_in_navigation_data());
  ASSERT_TRUE(offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(offline_ids()[1],
            offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_CONNECTED_NETWORK);
  ExpectAccessEntryPoint(kTestClientId1,
                         OfflinePageRequestJob::AccessEntryPoint::DOWNLOADS);
  ExpectOfflinePageSizeTotalSuffixCount(0);
  ExpectOnlinePageSizeUniqueSample(kTestClientId1, 0, 1);
}

TEST_F(OfflinePageRequestJobTest, FailToLoadByOfflineIDOnUrlMismatch) {
  SimulateHasNetworkConnectivity(true);

  // The offline page found with specific offline ID does not match the passed
  // online URL. Should fall back to find the offline page based on the online
  // URL.
  InterceptRequest(kTestUrl2, "GET", kOfflinePageHeader,
                   std::string(kOfflinePageHeaderReasonKey) + "=download " +
                       kOfflinePageHeaderIDKey + "=" +
                       base::Int64ToString(offline_ids()[1]),
                   content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(is_offline_page_set_in_navigation_data());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          PAGE_NOT_FOUND_ON_CONNECTED_NETWORK);
  ExpectNoAccessEntryPoint();
  ExpectOfflinePageSizeTotalSuffixCount(0);
  ExpectOnlinePageSizeTotalSuffixCount(0);
}

TEST_F(OfflinePageRequestJobTest, LoadOfflinePageForUrlWithFragment) {
  SimulateHasNetworkConnectivity(false);

  // Loads an url with fragment, that will match the offline URL without the
  // fragment.
  GURL url_with_fragment(kTestUrl1.spec() + "#ref");
  InterceptRequest(
      url_with_fragment, "GET", "", "", content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(kTestFileSize2, bytes_read());
  ASSERT_TRUE(offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(offline_ids()[2],
            offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK);
  ExpectOfflinePageSizeUniqueSample(kTestClientId2, 0, 1);
  ExpectOfflinePageSizeTotalSuffixCount(1);
  ExpectOnlinePageSizeTotalSuffixCount(0);

  // Loads an url without fragment, that will match the offline URL with the
  // fragment.
  InterceptRequest(kTestUrl3, "GET", "", "", content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(kTestFileSize3, bytes_read());
  ASSERT_TRUE(offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(offline_ids()[3],
            offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);
  ExpectMultiUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK, 2);
  ExpectOfflinePageSizeUniqueSample(kTestClientId2, 0, 1);
  ExpectOfflinePageSizeUniqueSample(kTestClientId3, 0, 1);
  ExpectOfflinePageSizeTotalSuffixCount(2);
  ExpectOnlinePageSizeTotalSuffixCount(0);

  // Loads an url with fragment, that will match the offline URL with different
  // fragment.
  GURL url3_with_different_fragment(kTestUrl3.spec() + "#different_ref");
  InterceptRequest(url3_with_different_fragment, "GET", "", "",
                   content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(kTestFileSize3, bytes_read());
  ASSERT_TRUE(offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(offline_ids()[3],
            offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);
  ExpectMultiUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK, 3);
  ExpectOfflinePageSizeUniqueSample(kTestClientId2, 0, 1);
  ExpectOfflinePageSizeUniqueSample(kTestClientId3, 0, 2);
  ExpectOfflinePageSizeTotalSuffixCount(3);
  ExpectOnlinePageSizeTotalSuffixCount(0);
}

TEST_F(OfflinePageRequestJobTest, LoadOfflinePageAfterRedirect) {
  SimulateHasNetworkConnectivity(false);

  // This should trigger redirect first.
  InterceptRequest(kTestUrlRedirectsTo3, "GET", "", "",
                   content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(kTestFileSize3, bytes_read());
  EXPECT_TRUE(is_offline_page_set_in_navigation_data());
  ASSERT_TRUE(offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(offline_ids()[3],
            offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);
  ExpectOneNonuniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          REDIRECTED_ON_DISCONNECTED_NETWORK);
  ExpectOneNonuniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK);
  ExpectAccessEntryPoint(kTestClientId3,
                         OfflinePageRequestJob::AccessEntryPoint::LINK);
  ExpectOfflinePageSizeUniqueSample(kTestClientId3, 0, 1);
  ExpectOnlinePageSizeTotalSuffixCount(0);
}

TEST_F(OfflinePageRequestJobTest, NoRedirectForOfflinePageWithSameOriginalURL) {
  SimulateHasNetworkConnectivity(false);

  // No redirect should be triggered when original URL is same as final URL.
  InterceptRequest(kTestUrl5, "GET", "", "", content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(kTestFileSize5, bytes_read());
  EXPECT_TRUE(is_offline_page_set_in_navigation_data());
  ASSERT_TRUE(offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(offline_ids()[5],
            offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK);
  ExpectAccessEntryPoint(kTestClientId5,
                         OfflinePageRequestJob::AccessEntryPoint::LINK);
  ExpectOfflinePageSizeUniqueSample(kTestClientId5, 0, 1);
  ExpectOnlinePageSizeTotalSuffixCount(0);
}

TEST_F(OfflinePageRequestJobTest, LoadOfflinePageFromNonExistentFile) {
  SimulateHasNetworkConnectivity(false);

  InterceptRequest(kTestUrl4, "GET", "", "", content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_read());
  EXPECT_TRUE(is_offline_page_set_in_navigation_data());
  ASSERT_TRUE(offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(offline_ids()[4],
            offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK);
  ExpectOpenFileErrorCode(net::ERR_FILE_NOT_FOUND);
  ExpectAccessEntryPoint(kTestClientId4,
                         OfflinePageRequestJob::AccessEntryPoint::LINK);
  ExpectOfflinePageSizeUniqueSample(kTestClientId4,
                                    kTestFileSize4NonExistent / 1024, 1);
  ExpectOnlinePageSizeTotalSuffixCount(0);
}

TEST_F(OfflinePageRequestJobTest, FileSizeMismatchOnDisconnectedNetwork) {
  SimulateHasNetworkConnectivity(false);

  InterceptRequest(kTestUrl6, "GET", "", "", content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(is_offline_page_set_in_navigation_data());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          DIGEST_MISMATCH_ON_DISCONNECTED_NETWORK);
  ExpectNoAccessEntryPoint();
  ExpectOfflinePageSizeTotalSuffixCount(0);
  ExpectOnlinePageSizeTotalSuffixCount(0);
}

TEST_F(OfflinePageRequestJobTest, FileSizeMismatchOnProhibitivelySlowNetwork) {
  SimulateHasNetworkConnectivity(true);
  test_previews_decider()->set_should_allow_preview(true);

  InterceptRequest(kTestUrl6, "GET", "", "", content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(is_offline_page_set_in_navigation_data());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          DIGEST_MISMATCH_ON_PROHIBITIVELY_SLOW_NETWORK);
  ExpectNoAccessEntryPoint();
  ExpectOfflinePageSizeTotalSuffixCount(0);
  ExpectOnlinePageSizeTotalSuffixCount(0);
}

TEST_F(OfflinePageRequestJobTest, FileSizeMismatchOnConnectedNetwork) {
  SimulateHasNetworkConnectivity(true);

  // When custom offline header exists and contains value other than
  // "reason=error", it means that offline page is forced to load.
  InterceptRequest(kTestUrl6, "GET", kOfflinePageHeader,
                   std::string(kOfflinePageHeaderReasonKey) + "=download",
                   content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(is_offline_page_set_in_navigation_data());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          DIGEST_MISMATCH_ON_CONNECTED_NETWORK);
  ExpectNoAccessEntryPoint();
  ExpectOfflinePageSizeTotalSuffixCount(0);
  ExpectOnlinePageSizeTotalSuffixCount(0);
}

TEST_F(OfflinePageRequestJobTest, FileSizeMismatchOnFlakyNetwork) {
  SimulateHasNetworkConnectivity(true);

  // When custom offline header exists and contains "reason=error", it means
  // that net error is hit in last request due to flaky network.
  InterceptRequest(kTestUrl6, "GET", kOfflinePageHeader,
                   std::string(kOfflinePageHeaderReasonKey) + "=" +
                       kOfflinePageHeaderReasonValueDueToNetError,
                   content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(is_offline_page_set_in_navigation_data());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          DIGEST_MISMATCH_ON_FLAKY_NETWORK);
  ExpectNoAccessEntryPoint();
  ExpectOfflinePageSizeTotalSuffixCount(0);
  ExpectOnlinePageSizeTotalSuffixCount(0);
}

TEST_F(OfflinePageRequestJobTest, DigestMismatchOnDisconnectedNetwork) {
  SimulateHasNetworkConnectivity(false);

  InterceptRequest(kTestUrl7, "GET", "", "", content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(is_offline_page_set_in_navigation_data());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          DIGEST_MISMATCH_ON_DISCONNECTED_NETWORK);
  ExpectNoAccessEntryPoint();
  ExpectOfflinePageSizeTotalSuffixCount(0);
  ExpectOnlinePageSizeTotalSuffixCount(0);
}

TEST_F(OfflinePageRequestJobTest, DigestMismatchOnProhibitivelySlowNetwork) {
  SimulateHasNetworkConnectivity(true);
  test_previews_decider()->set_should_allow_preview(true);

  InterceptRequest(kTestUrl7, "GET", "", "", content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(is_offline_page_set_in_navigation_data());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          DIGEST_MISMATCH_ON_PROHIBITIVELY_SLOW_NETWORK);
  ExpectNoAccessEntryPoint();
  ExpectOfflinePageSizeTotalSuffixCount(0);
  ExpectOnlinePageSizeTotalSuffixCount(0);
}

TEST_F(OfflinePageRequestJobTest, DigestMismatchOnConnectedNetwork) {
  SimulateHasNetworkConnectivity(true);

  // When custom offline header exists and contains value other than
  // "reason=error", it means that offline page is forced to load.
  InterceptRequest(kTestUrl7, "GET", kOfflinePageHeader,
                   std::string(kOfflinePageHeaderReasonKey) + "=download",
                   content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(is_offline_page_set_in_navigation_data());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          DIGEST_MISMATCH_ON_CONNECTED_NETWORK);
  ExpectNoAccessEntryPoint();
  ExpectOfflinePageSizeTotalSuffixCount(0);
  ExpectOnlinePageSizeTotalSuffixCount(0);
}

TEST_F(OfflinePageRequestJobTest, DigestMismatchOnFlakyNetwork) {
  SimulateHasNetworkConnectivity(true);

  // When custom offline header exists and contains "reason=error", it means
  // that net error is hit in last request due to flaky network.
  InterceptRequest(kTestUrl7, "GET", kOfflinePageHeader,
                   std::string(kOfflinePageHeaderReasonKey) + "=" +
                       kOfflinePageHeaderReasonValueDueToNetError,
                   content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(is_offline_page_set_in_navigation_data());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          DIGEST_MISMATCH_ON_FLAKY_NETWORK);
  ExpectNoAccessEntryPoint();
  ExpectOfflinePageSizeTotalSuffixCount(0);
  ExpectOnlinePageSizeTotalSuffixCount(0);
}

TEST_F(OfflinePageRequestJobTest, FailOnNoDigestForPublicArchiveFile) {
  SimulateHasNetworkConnectivity(false);

  InterceptRequest(kTestUrl8, "GET", "", "", content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(is_offline_page_set_in_navigation_data());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          DIGEST_MISMATCH_ON_DISCONNECTED_NETWORK);
  ExpectNoAccessEntryPoint();
  ExpectOfflinePageSizeTotalSuffixCount(0);
  ExpectOnlinePageSizeTotalSuffixCount(0);
}

TEST_F(OfflinePageRequestJobTest, FailToLoadByOfflineIDOnDigestMismatch) {
  SimulateHasNetworkConnectivity(true);

  // The offline page found with specific offline ID does not pass the
  // validation. Though there is another page with the same URL, it will not be
  // fetched. Instead, fall back to load the online URL.
  InterceptRequest(kTestUrl9, "GET", kOfflinePageHeader,
                   std::string(kOfflinePageHeaderReasonKey) + "=download " +
                       kOfflinePageHeaderIDKey + "=" +
                       base::Int64ToString(offline_ids()[10]),
                   content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(is_offline_page_set_in_navigation_data());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          DIGEST_MISMATCH_ON_CONNECTED_NETWORK);
  ExpectNoAccessEntryPoint();
  ExpectOfflinePageSizeTotalSuffixCount(0);
  ExpectOnlinePageSizeTotalSuffixCount(0);
}

TEST_F(OfflinePageRequestJobTest, LoadOtherPageOnDigestMismatch) {
  SimulateHasNetworkConnectivity(false);

  // There're 2 offline pages matching kTestUrl9. The most recently created one
  // (kTestClientId10) should fail on mistmatched digest. The second most
  // recently created offline page (kTestClientId9) should be fetched.
  InterceptRequest(kTestUrl9, "GET", "", "", content::RESOURCE_TYPE_MAIN_FRAME);
  base::RunLoop().Run();

  EXPECT_EQ(kTestFileSize9, bytes_read());
  EXPECT_TRUE(is_offline_page_set_in_navigation_data());
  ASSERT_TRUE(offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(offline_ids()[9],
            offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);
  ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK);
  ExpectAccessEntryPoint(kTestClientId9,
                         OfflinePageRequestJob::AccessEntryPoint::LINK);
  ExpectOfflinePageSizeUniqueSample(kTestClientId9, 0, 1);
  ExpectOnlinePageSizeTotalSuffixCount(0);
}

}  // namespace offline_pages
