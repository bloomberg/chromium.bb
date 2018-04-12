// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_request_job.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/histogram_tester.h"
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
#include "components/offline_pages/core/archive_validator.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/model/offline_page_model_taskified.h"
#include "components/offline_pages/core/offline_page_metadata_store_sql.h"
#include "components/offline_pages/core/request_header/offline_page_navigation_ui_data.h"
#include "components/offline_pages/core/system_download_manager_stub.h"
#include "components/previews/core/previews_decider.h"
#include "components/previews/core/previews_experiments.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/filename_util.h"
#include "net/http/http_request_headers.h"
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

const GURL kUrl("http://test.org/page");
const GURL kUrl2("http://test.org/another");
const base::FilePath kFilename1(FILE_PATH_LITERAL("hello.mhtml"));
const base::FilePath kFilename2(FILE_PATH_LITERAL("test.mhtml"));
const base::FilePath kNonexistentFilename(
    FILE_PATH_LITERAL("nonexistent.mhtml"));
const int kFileSize1 = 471;  // Real size of hello.mhtml.
const int kFileSize2 = 444;  // Real size of test.mhtml.
const int kMismatchedFileSize = 99999;
const std::string kDigest1(
    "\x43\x60\x62\x02\x06\x15\x0f\x3e\x77\x99\x3d\xed\xdc\xd4\xe2\x0d\xbe\xbd"
    "\x77\x1a\xfb\x32\x00\x51\x7e\x63\x7d\x3b\x2e\x46\x63\xf6",
    32);  // SHA256 Hash of hello.mhtml.
const std::string kDigest2(
    "\xBD\xD3\x37\x79\xDA\x7F\x4E\x6A\x16\x66\xED\x49\x67\x18\x54\x48\xC6\x8E"
    "\xA1\x47\x16\xA5\x44\x45\x43\xD0\x0E\x04\x9F\x4C\x45\xDC",
    32);  // SHA256 Hash of test.mhtml.
const std::string kMismatchedDigest(
    "\xff\x64\xF9\x7C\x94\xE5\x9E\x91\x83\x3D\x41\xB0\x36\x90\x0A\xDF\xB3\xB1"
    "\x5C\x13\xBE\xB8\x35\x8C\xF6\x5B\xC4\xB5\x5A\xFC\x3A\xCC",
    32);  // Wrong SHA256 Hash.

const int kTabId = 1;
const int kBufSize = 1024;

const char kAggregatedRequestResultHistogram[] =
    "OfflinePages.AggregatedRequestResult2";
const char kOpenFileErrorCodeHistogram[] =
    "OfflinePages.RequestJob.OpenFileErrorCode";
const char kSeekFileErrorCodeHistogram[] =
    "OfflinePages.RequestJob.SeekFileErrorCode";
const char kAccessEntryPointHistogram[] = "OfflinePages.AccessEntryPoint.";
const char kPageSizeAccessOfflineHistogramBase[] =
    "OfflinePages.PageSizeOnAccess.Offline.";
const char kPageSizeAccessOnlineHistogramBase[] =
    "OfflinePages.PageSizeOnAccess.Online.";

const int64_t kDownloadId = 42LL;

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
  typedef base::Callback<void(const std::string&, int)> ReadCompletedCallback;

  explicit TestURLRequestDelegate(const ReadCompletedCallback& callback)
      : read_completed_callback_(callback),
        buffer_(new net::IOBuffer(kBufSize)),
        request_status_(net::ERR_IO_PENDING) {}

  void OnResponseStarted(net::URLRequest* request, int net_error) override {
    DCHECK_NE(net::ERR_IO_PENDING, net_error);
    if (net_error != net::OK) {
      OnReadCompleted(request, net_error);
      return;
    }
    // Initiate the first read.
    int bytes_read = request->Read(buffer_.get(), kBufSize);
    if (bytes_read >= 0) {
      OnReadCompleted(request, bytes_read);
    } else if (bytes_read != net::ERR_IO_PENDING) {
      request_status_ = bytes_read;
      OnResponseCompleted();
    }
  }

  void OnReadCompleted(net::URLRequest* request, int bytes_read) override {
    if (bytes_read > 0)
      data_received_.append(buffer_->data(), bytes_read);

    // If it was not end of stream, request to read more.
    while (bytes_read > 0) {
      bytes_read = request->Read(buffer_.get(), kBufSize);
      if (bytes_read > 0)
        data_received_.append(buffer_->data(), bytes_read);
    }

    request_status_ = (bytes_read >= 0) ? net::OK : bytes_read;
    if (bytes_read != net::ERR_IO_PENDING)
      OnResponseCompleted();
  }

 private:
  void OnResponseCompleted() {
    if (request_status_ != net::OK)
      data_received_.clear();
    if (!read_completed_callback_.is_null())
      read_completed_callback_.Run(data_received_, request_status_);
  }

  ReadCompletedCallback read_completed_callback_;
  scoped_refptr<net::IOBuffer> buffer_;
  std::string data_received_;
  int request_status_;

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
          std::make_unique<OfflinePageRequestJobTestDelegate>(web_contents_,
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

  bool online() const { return online_; }
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

  bool IsURLAllowedForPreview(const net::URLRequest& request,
                              previews::PreviewsType type) const override {
    return should_allow_preview_;
  }

  bool should_allow_preview() const { return should_allow_preview_; }
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
                     content::WebContents* web_contents,
                     const CreateArchiveCallback& callback) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, this, ArchiverResult::SUCCESSFULLY_CREATED, url_,
                   archive_file_path_, base::string16(), archive_file_size_,
                   digest_));
  }

  void PublishArchive(
      const OfflinePageItem& offline_page,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
      const base::FilePath& new_file_path,
      SystemDownloadManager* download_manager,
      PublishArchiveDoneCallback publish_done_callback) override {
    publish_archive_result_.move_result = SavePageResult::SUCCESS;
    publish_archive_result_.new_file_path = offline_page.file_path;
    publish_archive_result_.download_id = 0;
    std::move(publish_done_callback)
        .Run(offline_page, &publish_archive_result_);
  }

 private:
  const GURL url_;
  const base::FilePath archive_file_path_;
  const int archive_file_size_;
  const std::string digest_;
  PublishArchiveResult publish_archive_result_;

  DISALLOW_COPY_AND_ASSIGN(TestOfflinePageArchiver);
};

// Helper function to make a character array filled with |size| bytes of
// test content.
std::string MakeContentOfSize(int size) {
  EXPECT_GE(size, 0);
  std::string result;
  result.reserve(size);
  for (int i = 0; i < size; i++)
    result.append(1, static_cast<char>(i % 256));
  return result;
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

  base::FilePath CreateFileWithContent(const std::string& content);

  // Returns an offline id of the saved page.
  // |file_path| in SavePublicPage and SaveInternalPage can be either absolute
  // or relative. If relative, |file_path| will be appended to public/internal
  // archive directory used for the testing.
  // |file_path| in SavePage should be absolute.
  int64_t SavePublicPage(const GURL& url,
                         const GURL& original_url,
                         const base::FilePath& file_path,
                         int64_t file_size,
                         const std::string& digest);
  int64_t SaveInternalPage(const GURL& url,
                           const GURL& original_url,
                           const base::FilePath& file_path,
                           int64_t file_size,
                           const std::string& digest);
  int64_t SavePage(const GURL& url,
                   const GURL& original_url,
                   const base::FilePath& file_path,
                   int64_t file_size,
                   const std::string& digest);

  OfflinePageItem GetPage(int64_t offline_id);

  void LoadPage(const GURL& url);
  void LoadPageWithHeaders(const GURL& url,
                           const net::HttpRequestHeaders& extra_headers);

  void InterceptRequest(const GURL& url,
                        const std::string& method,
                        const net::HttpRequestHeaders& extra_headers,
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
  void ExpectSeekFileErrorCode(int result);

  void ExpectAccessEntryPoint(
      OfflinePageRequestJob::AccessEntryPoint entry_point);
  void ExpectNoAccessEntryPoint();

  void ExpectOfflinePageSizeUniqueSample(int bucket, int count);
  void ExpectOfflinePageSizeTotalSuffixCount(int count);
  void ExpectOnlinePageSizeUniqueSample(int bucket, int count);
  void ExpectOnlinePageSizeTotalSuffixCount(int count);
  void ExpectOfflinePageAccessCount(int64_t offline_id, int count);

  void ExpectNoOfflinePageServed(
      int64_t offline_id,
      OfflinePageRequestJob::AggregatedRequestResult expected_request_result);
  void ExpectOfflinePageServed(
      int64_t expected_offline_id,
      int expected_file_size,
      OfflinePageRequestJob::AggregatedRequestResult expected_request_result);

  // Use the offline header with specific reason and offline_id. Return the
  // full header string.
  std::string UseOfflinePageHeader(OfflinePageHeader::Reason reason,
                                   int64_t offline_id);
  std::string UseOfflinePageHeaderForIntent(OfflinePageHeader::Reason reason,
                                            int64_t offline_id,
                                            const GURL& intent_url);

  net::TestURLRequestContext* url_request_context() {
    return test_url_request_context_.get();
  }
  Profile* profile() { return profile_; }
  OfflinePageTabHelper* offline_page_tab_helper() const {
    return offline_page_tab_helper_;
  }
  int request_status() const { return request_status_; }
  int bytes_read() const { return data_received_.length(); }
  const std::string& data_received() const { return data_received_; }
  bool is_offline_page_set_in_navigation_data() const {
    return is_offline_page_set_in_navigation_data_;
  }

  TestPreviewsDecider* test_previews_decider() {
    return test_previews_decider_.get();
  }

  bool is_connected_with_good_network() {
    return network_change_notifier_->online() &&
           // Exclude prohibitively slow network.
           !test_previews_decider_->should_allow_preview() &&
           // Exclude flaky network.
           offline_page_header_.reason != OfflinePageHeader::Reason::NET_ERROR;
  }

 private:
  static std::unique_ptr<KeyedService> BuildTestOfflinePageModel(
      content::BrowserContext* context);

  // TODO(https://crbug.com/809610): The static members below will be removed
  // once the reference to BuildTestOfflinePageModel in SetUp is converted to a
  // base::OnceCallback.
  static base::FilePath private_archives_dir_;
  static base::FilePath public_archives_dir_;

  OfflinePageRequestJob::AccessEntryPoint GetExpectedAccessEntryPoint() const;

  void OnSavePageDone(SavePageResult result, int64_t offline_id);
  std::unique_ptr<net::URLRequest> CreateRequest(
      const GURL& url,
      const std::string& method,
      content::ResourceType resource_type);
  void OnGetPageByOfflineIdDone(const OfflinePageItem* pages);
  void ReadCompleted(const std::string& data_received,
                     int request_status,
                     bool is_offline_page_set_in_navigation_data);

  // Runs on IO thread.
  void CreateFileWithContentOnIO(const std::string& content,
                                 const base::Closure& callback);
  void SetUpNetworkObjectsOnIO();
  void TearDownNetworkObjectsOnIO();
  void InterceptRequestOnIO(const GURL& url,
                            const std::string& method,
                            const net::HttpRequestHeaders& extra_headers,
                            content::ResourceType resource_type);
  void ReadCompletedOnIO(const std::string& data_received, int request_status);
  void TearDownOnReadCompletedOnIO(const std::string& data_received,
                                   int request_status,
                                   bool is_offline_page_set_in_navigation_data);

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  OfflinePageTabHelper* offline_page_tab_helper_;  // Not owned.
  int64_t last_offline_id_;
  std::string data_received_;
  int request_status_;
  bool is_offline_page_set_in_navigation_data_;
  OfflinePageItem page_;
  OfflinePageHeader offline_page_header_;

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
  base::ScopedTempDir private_archives_temp_base_dir_;
  base::ScopedTempDir public_archives_temp_base_dir_;
  base::ScopedTempDir temp_dir_;
  base::FilePath temp_file_path_;
  int file_name_sequence_num_ = 0;

  bool async_operation_completed_ = false;
  base::Closure async_operation_completed_callback_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageRequestJobTest);
};

OfflinePageRequestJobTest::OfflinePageRequestJobTest()
    : thread_bundle_(content::TestBrowserThreadBundle::REAL_IO_THREAD),
      profile_manager_(TestingBrowserProcess::GetGlobal()),
      last_offline_id_(0),
      request_status_(net::ERR_IO_PENDING),
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
  // Note: The extra dir into the temp folder is needed so that the helper
  // dir-copy operation works properly. That operation copies the source dir
  // final path segment into the destination, and not only its immediate
  // contents so this same-named path here makes the archive dir variable point
  // to the correct location.
  // TODO(romax): add the more recent "temporary" dir here instead of reusing
  // the private one.
  ASSERT_TRUE(private_archives_temp_base_dir_.CreateUniqueTempDir());
  private_archives_dir_ = private_archives_temp_base_dir_.GetPath().AppendASCII(
      kPrivateOfflineFileDir);
  ASSERT_TRUE(public_archives_temp_base_dir_.CreateUniqueTempDir());
  public_archives_dir_ = public_archives_temp_base_dir_.GetPath().AppendASCII(
      kPublicOfflineFileDir);
  OfflinePageModelFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), &OfflinePageRequestJobTest::BuildTestOfflinePageModel);

  // Initialize OfflinePageModel.
  OfflinePageModelTaskified* model = static_cast<OfflinePageModelTaskified*>(
      OfflinePageModelFactory::GetForBrowserContext(profile()));

  // Skip the logic to clear the original URL if it is same as final URL.
  // This is needed in order to test that offline page request handler can
  // omit the redirect under this circumstance, for compatibility with the
  // metadata already written to the store.
  model->SetSkipClearingOriginalUrlForTesting();

  // Avoid running the model's maintenance tasks.
  model->DoNotRunMaintenanceTasksForTesting();

  // Move test data files into their respective temporary test directories. The
  // model's maintenance tasks must not be executed in the meantime otherwise
  // these files will be wiped by consistency checks.
  base::FilePath test_data_dir_path;
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_path);
  base::FilePath test_data_private_archives_dir =
      test_data_dir_path.AppendASCII(kPrivateOfflineFileDir);
  ASSERT_TRUE(base::CopyDirectory(test_data_private_archives_dir,
                                  private_archives_dir_.DirName(), true));
  base::FilePath test_data_public_archives_dir =
      test_data_dir_path.AppendASCII(kPublicOfflineFileDir);
  ASSERT_TRUE(base::CopyDirectory(test_data_public_archives_dir,
                                  public_archives_dir_.DirName(), true));

  histogram_tester_ = std::make_unique<base::HistogramTester>();
}

void OfflinePageRequestJobTest::TearDown() {
  EXPECT_TRUE(private_archives_temp_base_dir_.Delete());
  EXPECT_TRUE(public_archives_temp_base_dir_.Delete());
  // This check confirms that the model's maintenance tasks were not executed
  // during the test run.
  histogram_tester_->ExpectTotalCount("OfflinePages.ClearTemporaryPages.Result",
                                      0);
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

void OfflinePageRequestJobTest::CreateFileWithContentOnIO(
    const std::string& content,
    const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!temp_dir_.IsValid()) {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }
  std::string file_name("test");
  file_name += base::IntToString(file_name_sequence_num_++);
  file_name += ".mht";
  temp_file_path_ = temp_dir_.GetPath().AppendASCII(file_name);
  ASSERT_TRUE(base::WriteFile(temp_file_path_, content.c_str(),
                              content.length()) != -1);
  callback.Run();
}

base::FilePath OfflinePageRequestJobTest::CreateFileWithContent(
    const std::string& content) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::RunLoop run_loop;
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&OfflinePageRequestJobTest::CreateFileWithContentOnIO,
                 base::Unretained(this), content, run_loop.QuitClosure()));
  run_loop.Run();
  return temp_file_path_;
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
  url_request_delegate_ = std::make_unique<TestURLRequestDelegate>(base::Bind(
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
  histogram_tester_->ExpectUniqueSample(kAggregatedRequestResultHistogram,
                                        static_cast<int>(result), 1);
}

void OfflinePageRequestJobTest::
    ExpectMultiUniqueSampleForAggregatedRequestResult(
        OfflinePageRequestJob::AggregatedRequestResult result,
        int count) {
  histogram_tester_->ExpectUniqueSample(kAggregatedRequestResultHistogram,
                                        static_cast<int>(result), count);
}

void OfflinePageRequestJobTest::
    ExpectOneNonuniqueSampleForAggregatedRequestResult(
        OfflinePageRequestJob::AggregatedRequestResult result) {
  histogram_tester_->ExpectBucketCount(kAggregatedRequestResultHistogram,
                                       static_cast<int>(result), 1);
}

void OfflinePageRequestJobTest::ExpectNoSamplesInAggregatedRequestResult() {
  histogram_tester_->ExpectTotalCount(kAggregatedRequestResultHistogram, 0);
}

void OfflinePageRequestJobTest::ExpectOpenFileErrorCode(int result) {
  histogram_tester_->ExpectUniqueSample(kOpenFileErrorCodeHistogram, -result,
                                        1);
}

void OfflinePageRequestJobTest::ExpectSeekFileErrorCode(int result) {
  histogram_tester_->ExpectUniqueSample(kSeekFileErrorCodeHistogram, -result,
                                        1);
}

void OfflinePageRequestJobTest::ExpectAccessEntryPoint(
    OfflinePageRequestJob::AccessEntryPoint entry_point) {
  histogram_tester_->ExpectUniqueSample(
      std::string(kAccessEntryPointHistogram) + kDownloadNamespace,
      static_cast<int>(entry_point), 1);
}

void OfflinePageRequestJobTest::ExpectNoAccessEntryPoint() {
  EXPECT_TRUE(
      histogram_tester_->GetTotalCountsForPrefix(kAccessEntryPointHistogram)
          .empty());
}

void OfflinePageRequestJobTest::ExpectOfflinePageSizeUniqueSample(
    int bucket,
    int count) {
  histogram_tester_->ExpectUniqueSample(
      std::string(kPageSizeAccessOfflineHistogramBase) + kDownloadNamespace,
      bucket, count);
}

void OfflinePageRequestJobTest::ExpectOfflinePageSizeTotalSuffixCount(
    int count) {
  int total_offline_count = 0;
  base::HistogramTester::CountsMap all_offline_counts =
      histogram_tester_->GetTotalCountsForPrefix(
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
    int bucket,
    int count) {
  histogram_tester_->ExpectUniqueSample(
      std::string(kPageSizeAccessOnlineHistogramBase) + kDownloadNamespace,
      bucket, count);
}

void OfflinePageRequestJobTest::ExpectOnlinePageSizeTotalSuffixCount(
    int count) {
  int online_count = 0;
  base::HistogramTester::CountsMap all_online_counts =
      histogram_tester_->GetTotalCountsForPrefix(
          kPageSizeAccessOnlineHistogramBase);
  for (const std::pair<std::string, base::HistogramBase::Count>&
           namespace_and_count : all_online_counts) {
    online_count += namespace_and_count.second;
  }
  EXPECT_EQ(count, online_count)
      << "Wrong histogram samples count under prefix "
      << kPageSizeAccessOnlineHistogramBase << "*";
}

void OfflinePageRequestJobTest::ExpectOfflinePageAccessCount(int64_t offline_id,
                                                             int count) {
  OfflinePageItem offline_page = GetPage(offline_id);
  EXPECT_EQ(count, offline_page.access_count);
}

void OfflinePageRequestJobTest::ExpectNoOfflinePageServed(
    int64_t offline_id,
    OfflinePageRequestJob::AggregatedRequestResult expected_request_result) {
  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(is_offline_page_set_in_navigation_data());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
  if (expected_request_result !=
      OfflinePageRequestJob::AggregatedRequestResult::
          AGGREGATED_REQUEST_RESULT_MAX) {
    ExpectOneUniqueSampleForAggregatedRequestResult(expected_request_result);
  }
  ExpectNoAccessEntryPoint();
  ExpectOfflinePageSizeTotalSuffixCount(0);
  ExpectOnlinePageSizeTotalSuffixCount(0);
  ExpectOfflinePageAccessCount(offline_id, 0);
}

void OfflinePageRequestJobTest::ExpectOfflinePageServed(
    int64_t expected_offline_id,
    int expected_file_size,
    OfflinePageRequestJob::AggregatedRequestResult expected_request_result) {
  EXPECT_EQ(net::OK, request_status_);
  EXPECT_EQ(expected_file_size, bytes_read());
  EXPECT_TRUE(is_offline_page_set_in_navigation_data());
  ASSERT_TRUE(offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(expected_offline_id,
            offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);
  OfflinePageTrustedState expected_trusted_state =
      private_archives_dir_.IsParent(
          offline_page_tab_helper()->GetOfflinePageForTest()->file_path)
          ? OfflinePageTrustedState::TRUSTED_AS_IN_INTERNAL_DIR
          : OfflinePageTrustedState::TRUSTED_AS_UNMODIFIED_AND_IN_PUBLIC_DIR;
  EXPECT_EQ(expected_trusted_state,
            offline_page_tab_helper()->GetTrustedStateForTest());
  if (expected_request_result !=
      OfflinePageRequestJob::AggregatedRequestResult::
          AGGREGATED_REQUEST_RESULT_MAX) {
    ExpectOneUniqueSampleForAggregatedRequestResult(expected_request_result);
  }
  OfflinePageRequestJob::AccessEntryPoint expected_entry_point =
      GetExpectedAccessEntryPoint();
  ExpectAccessEntryPoint(expected_entry_point);
  if (is_connected_with_good_network()) {
    ExpectOnlinePageSizeUniqueSample(expected_file_size / 1024, 1);
    ExpectOfflinePageSizeTotalSuffixCount(0);
  } else {
    ExpectOfflinePageSizeUniqueSample(expected_file_size / 1024, 1);
    ExpectOnlinePageSizeTotalSuffixCount(0);
  }
  ExpectOfflinePageAccessCount(expected_offline_id, 1);
}

OfflinePageRequestJob::AccessEntryPoint
OfflinePageRequestJobTest::GetExpectedAccessEntryPoint() const {
  switch (offline_page_header_.reason) {
    case OfflinePageHeader::Reason::DOWNLOAD:
      return OfflinePageRequestJob::AccessEntryPoint::DOWNLOADS;
    case OfflinePageHeader::Reason::NOTIFICATION:
      return OfflinePageRequestJob::AccessEntryPoint::NOTIFICATION;
    case OfflinePageHeader::Reason::FILE_URL_INTENT:
      return OfflinePageRequestJob::AccessEntryPoint::FILE_URL_INTENT;
    case OfflinePageHeader::Reason::CONTENT_URL_INTENT:
      return OfflinePageRequestJob::AccessEntryPoint::CONTENT_URL_INTENT;
    default:
      return OfflinePageRequestJob::AccessEntryPoint::LINK;
  }
}

std::string OfflinePageRequestJobTest::UseOfflinePageHeader(
    OfflinePageHeader::Reason reason,
    int64_t offline_id) {
  DCHECK_NE(OfflinePageHeader::Reason::NONE, reason);
  offline_page_header_.reason = reason;
  if (offline_id)
    offline_page_header_.id = base::Int64ToString(offline_id);
  return offline_page_header_.GetCompleteHeaderString();
}

std::string OfflinePageRequestJobTest::UseOfflinePageHeaderForIntent(
    OfflinePageHeader::Reason reason,
    int64_t offline_id,
    const GURL& intent_url) {
  DCHECK_NE(OfflinePageHeader::Reason::NONE, reason);
  DCHECK(offline_id);
  offline_page_header_.reason = reason;
  offline_page_header_.id = base::Int64ToString(offline_id);
  offline_page_header_.intent_url = intent_url;
  return offline_page_header_.GetCompleteHeaderString();
}

int64_t OfflinePageRequestJobTest::SavePublicPage(
    const GURL& url,
    const GURL& original_url,
    const base::FilePath& file_path,
    int64_t file_size,
    const std::string& digest) {
  base::FilePath final_path;
  if (file_path.IsAbsolute()) {
    final_path = file_path;
  } else {
    final_path = public_archives_dir_.Append(file_path);
  }

  return SavePage(url, original_url, final_path, file_size, digest);
}

int64_t OfflinePageRequestJobTest::SaveInternalPage(
    const GURL& url,
    const GURL& original_url,
    const base::FilePath& file_path,
    int64_t file_size,
    const std::string& digest) {
  base::FilePath final_path;
  if (file_path.IsAbsolute()) {
    final_path = file_path;
  } else {
    final_path = private_archives_dir_.Append(file_path);
  }

  return SavePage(url, original_url, final_path, file_size, digest);
}

int64_t OfflinePageRequestJobTest::SavePage(const GURL& url,
                                            const GURL& original_url,
                                            const base::FilePath& file_path,
                                            int64_t file_size,
                                            const std::string& digest) {
  DCHECK(file_path.IsAbsolute());

  static int item_counter = 0;
  ++item_counter;

  std::unique_ptr<TestOfflinePageArchiver> archiver(
      new TestOfflinePageArchiver(url, file_path, file_size, digest));

  async_operation_completed_ = false;
  OfflinePageModel::SavePageParams save_page_params;
  save_page_params.url = url;
  save_page_params.client_id =
      ClientId(kDownloadNamespace, base::IntToString(item_counter));
  save_page_params.original_url = original_url;
  OfflinePageModelFactory::GetForBrowserContext(profile())->SavePage(
      save_page_params, std::move(archiver), nullptr,
      base::Bind(&OfflinePageRequestJobTest::OnSavePageDone,
                 base::Unretained(this)));
  WaitForAsyncOperation();
  return last_offline_id_;
}

// static
std::unique_ptr<KeyedService>
OfflinePageRequestJobTest::BuildTestOfflinePageModel(
    content::BrowserContext* context) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::ThreadTaskRunnerHandle::Get();

  base::FilePath store_path =
      context->GetPath().Append(chrome::kOfflinePageMetadataDirname);
  std::unique_ptr<OfflinePageMetadataStoreSQL> metadata_store(
      new OfflinePageMetadataStoreSQL(task_runner, store_path));
  std::unique_ptr<SystemDownloadManager> download_manager(
      new SystemDownloadManagerStub(kDownloadId, true));

  // Since we're not saving page into temporary dir, it's set the same as the
  // private dir.
  std::unique_ptr<ArchiveManager> archive_manager(
      new ArchiveManager(private_archives_dir_, private_archives_dir_,
                         public_archives_dir_, task_runner));

  return std::unique_ptr<KeyedService>(new OfflinePageModelTaskified(
      std::move(metadata_store), std::move(archive_manager),
      std::move(download_manager), task_runner,
      base::DefaultClock::GetInstance()));
}

// static
base::FilePath OfflinePageRequestJobTest::private_archives_dir_;
base::FilePath OfflinePageRequestJobTest::public_archives_dir_;

void OfflinePageRequestJobTest::OnSavePageDone(SavePageResult result,
                                               int64_t offline_id) {
  ASSERT_EQ(SavePageResult::SUCCESS, result);
  last_offline_id_ = offline_id;

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
    const net::HttpRequestHeaders& extra_headers,
    content::ResourceType resource_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  SetUpNetworkObjectsOnIO();

  request_ = CreateRequest(url, method, resource_type);
  if (!extra_headers.IsEmpty())
    request_->SetExtraRequestHeaders(extra_headers);
  request_->Start();
}

void OfflinePageRequestJobTest::InterceptRequest(
    const GURL& url,
    const std::string& method,
    const net::HttpRequestHeaders& extra_headers,
    content::ResourceType resource_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&OfflinePageRequestJobTest::InterceptRequestOnIO,
                 base::Unretained(this), url, method, extra_headers,
                 resource_type));
  base::RunLoop().Run();
}

void OfflinePageRequestJobTest::LoadPage(const GURL& url) {
  InterceptRequest(url, "GET", net::HttpRequestHeaders(),
                   content::RESOURCE_TYPE_MAIN_FRAME);
}

void OfflinePageRequestJobTest::LoadPageWithHeaders(
    const GURL& url,
    const net::HttpRequestHeaders& extra_headers) {
  InterceptRequest(url, "GET", extra_headers,
                   content::RESOURCE_TYPE_MAIN_FRAME);
}

void OfflinePageRequestJobTest::ReadCompletedOnIO(
    const std::string& data_received,
    int request_status) {
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
                 base::Unretained(this), data_received, request_status,
                 is_offline_page_set_in_navigation_data));
}

void OfflinePageRequestJobTest::TearDownOnReadCompletedOnIO(
    const std::string& data_received,
    int request_status,
    bool is_offline_page_set_in_navigation_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  TearDownNetworkObjectsOnIO();

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&OfflinePageRequestJobTest::ReadCompleted,
                 base::Unretained(this), data_received, request_status,
                 is_offline_page_set_in_navigation_data));
}

void OfflinePageRequestJobTest::ReadCompleted(
    const std::string& data_received,
    int request_status,
    bool is_offline_page_set_in_navigation_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  data_received_ = data_received;
  request_status_ = request_status;
  is_offline_page_set_in_navigation_data_ =
      is_offline_page_set_in_navigation_data;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
}

TEST_F(OfflinePageRequestJobTest, FailedToCreateRequestJob) {
  SimulateHasNetworkConnectivity(false);

  // Must be http/https URL.
  InterceptRequest(GURL("ftp://host/doc"), "GET", net::HttpRequestHeaders(),
                   content::RESOURCE_TYPE_MAIN_FRAME);
  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());

  InterceptRequest(GURL("file:///path/doc"), "GET", net::HttpRequestHeaders(),
                   content::RESOURCE_TYPE_MAIN_FRAME);
  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());

  // Must be GET method.
  InterceptRequest(kUrl, "POST", net::HttpRequestHeaders(),
                   content::RESOURCE_TYPE_MAIN_FRAME);
  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());

  InterceptRequest(kUrl, "HEAD", net::HttpRequestHeaders(),
                   content::RESOURCE_TYPE_MAIN_FRAME);
  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());

  // Must be main resource.
  InterceptRequest(kUrl, "POST", net::HttpRequestHeaders(),
                   content::RESOURCE_TYPE_SUB_FRAME);
  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());

  InterceptRequest(kUrl, "POST", net::HttpRequestHeaders(),
                   content::RESOURCE_TYPE_IMAGE);
  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());

  ExpectNoSamplesInAggregatedRequestResult();
  ExpectOfflinePageSizeTotalSuffixCount(0);
  ExpectOnlinePageSizeTotalSuffixCount(0);
}

TEST_F(OfflinePageRequestJobTest, LoadOfflinePageOnDisconnectedNetwork) {
  SimulateHasNetworkConnectivity(false);

  int64_t offline_id =
      SaveInternalPage(kUrl, GURL(), kFilename1, kFileSize1, std::string());

  LoadPage(kUrl);

  ExpectOfflinePageServed(offline_id, kFileSize1,
                          OfflinePageRequestJob::AggregatedRequestResult::
                              SHOW_OFFLINE_ON_DISCONNECTED_NETWORK);
}

TEST_F(OfflinePageRequestJobTest, PageNotFoundOnDisconnectedNetwork) {
  SimulateHasNetworkConnectivity(false);

  int64_t offline_id =
      SaveInternalPage(kUrl, GURL(), kFilename1, kFileSize1, std::string());

  LoadPage(kUrl2);

  ExpectNoOfflinePageServed(offline_id,
                            OfflinePageRequestJob::AggregatedRequestResult::
                                PAGE_NOT_FOUND_ON_DISCONNECTED_NETWORK);
}

TEST_F(OfflinePageRequestJobTest, LoadOfflinePageOnProhibitivelySlowNetwork) {
  SimulateHasNetworkConnectivity(true);
  test_previews_decider()->set_should_allow_preview(true);

  int64_t offline_id =
      SaveInternalPage(kUrl, GURL(), kFilename1, kFileSize1, std::string());

  LoadPage(kUrl);

  ExpectOfflinePageServed(offline_id, kFileSize1,
                          OfflinePageRequestJob::AggregatedRequestResult::
                              SHOW_OFFLINE_ON_PROHIBITIVELY_SLOW_NETWORK);
}

TEST_F(OfflinePageRequestJobTest,
       DontLoadReloadOfflinePageOnProhibitivelySlowNetwork) {
  SimulateHasNetworkConnectivity(true);
  test_previews_decider()->set_should_allow_preview(true);

  int64_t offline_id =
      SaveInternalPage(kUrl, GURL(), kFilename1, kFileSize1, std::string());

  // Treat this as a reloaded page.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(
      UseOfflinePageHeader(OfflinePageHeader::Reason::RELOAD, 0));
  LoadPageWithHeaders(kUrl, extra_headers);

  // The existentce of RELOAD header will force to treat the network as
  // connected regardless current network condition. So we will fall back to
  // the default handling immediately and no request result should be reported.
  // Passing AGGREGATED_REQUEST_RESULT_MAX to skip checking request result in
  // the helper function.
  ExpectNoOfflinePageServed(offline_id,
                            OfflinePageRequestJob::AggregatedRequestResult::
                                AGGREGATED_REQUEST_RESULT_MAX);
}

TEST_F(OfflinePageRequestJobTest, PageNotFoundOnProhibitivelySlowNetwork) {
  SimulateHasNetworkConnectivity(true);
  test_previews_decider()->set_should_allow_preview(true);

  int64_t offline_id =
      SaveInternalPage(kUrl, GURL(), kFilename1, kFileSize1, std::string());

  LoadPage(kUrl2);

  ExpectNoOfflinePageServed(offline_id,
                            OfflinePageRequestJob::AggregatedRequestResult::
                                PAGE_NOT_FOUND_ON_PROHIBITIVELY_SLOW_NETWORK);
}

TEST_F(OfflinePageRequestJobTest, LoadOfflinePageOnFlakyNetwork) {
  SimulateHasNetworkConnectivity(true);

  int64_t offline_id =
      SaveInternalPage(kUrl, GURL(), kFilename1, kFileSize1, std::string());

  // When custom offline header exists and contains "reason=error", it means
  // that net error is hit in last request due to flaky network.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(
      UseOfflinePageHeader(OfflinePageHeader::Reason::NET_ERROR, 0));
  LoadPageWithHeaders(kUrl, extra_headers);

  ExpectOfflinePageServed(offline_id, kFileSize1,
                          OfflinePageRequestJob::AggregatedRequestResult::
                              SHOW_OFFLINE_ON_FLAKY_NETWORK);
}

TEST_F(OfflinePageRequestJobTest, PageNotFoundOnFlakyNetwork) {
  SimulateHasNetworkConnectivity(true);

  int64_t offline_id =
      SaveInternalPage(kUrl, GURL(), kFilename1, kFileSize1, std::string());

  // When custom offline header exists and contains "reason=error", it means
  // that net error is hit in last request due to flaky network.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(
      UseOfflinePageHeader(OfflinePageHeader::Reason::NET_ERROR, 0));
  LoadPageWithHeaders(kUrl2, extra_headers);

  ExpectNoOfflinePageServed(offline_id,
                            OfflinePageRequestJob::AggregatedRequestResult::
                                PAGE_NOT_FOUND_ON_FLAKY_NETWORK);
}

TEST_F(OfflinePageRequestJobTest, ForceLoadOfflinePageOnConnectedNetwork) {
  SimulateHasNetworkConnectivity(true);

  int64_t offline_id =
      SaveInternalPage(kUrl, GURL(), kFilename1, kFileSize1, std::string());

  // When custom offline header exists and contains value other than
  // "reason=error", it means that offline page is forced to load.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(
      UseOfflinePageHeader(OfflinePageHeader::Reason::DOWNLOAD, 0));
  LoadPageWithHeaders(kUrl, extra_headers);

  ExpectOfflinePageServed(offline_id, kFileSize1,
                          OfflinePageRequestJob::AggregatedRequestResult::
                              SHOW_OFFLINE_ON_CONNECTED_NETWORK);
}

TEST_F(OfflinePageRequestJobTest, PageNotFoundOnConnectedNetwork) {
  SimulateHasNetworkConnectivity(true);

  // Save an offline page.
  int64_t offline_id =
      SaveInternalPage(kUrl, GURL(), kFilename1, kFileSize1, std::string());

  // When custom offline header exists and contains value other than
  // "reason=error", it means that offline page is forced to load.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(
      UseOfflinePageHeader(OfflinePageHeader::Reason::DOWNLOAD, 0));
  LoadPageWithHeaders(kUrl2, extra_headers);

  ExpectNoOfflinePageServed(offline_id,
                            OfflinePageRequestJob::AggregatedRequestResult::
                                PAGE_NOT_FOUND_ON_CONNECTED_NETWORK);
}

TEST_F(OfflinePageRequestJobTest, DoNotLoadOfflinePageOnConnectedNetwork) {
  SimulateHasNetworkConnectivity(true);

  int64_t offline_id =
      SaveInternalPage(kUrl, GURL(), kFilename1, kFileSize1, std::string());

  LoadPage(kUrl);

  // When the network is good, we will fall back to the default handling
  // immediately. So no request result should be reported. Passing
  // AGGREGATED_REQUEST_RESULT_MAX to skip checking request result in
  // the helper function.
  ExpectNoOfflinePageServed(offline_id,
                            OfflinePageRequestJob::AggregatedRequestResult::
                                AGGREGATED_REQUEST_RESULT_MAX);
}

TEST_F(OfflinePageRequestJobTest, DISABLED_LoadMostRecentlyCreatedOfflinePage) {
  SimulateHasNetworkConnectivity(false);

  // Save 2 offline pages associated with same online URL, but pointing to
  // different archive file.
  int64_t offline_id1 =
      SaveInternalPage(kUrl, GURL(), kFilename1, kFileSize1, std::string());
  int64_t offline_id2 =
      SaveInternalPage(kUrl, GURL(), kFilename2, kFileSize2, std::string());

  // Load an URL that matches multiple offline pages. Expect that the most
  // recently created offline page is fetched.
  LoadPage(kUrl);

  ExpectOfflinePageServed(offline_id2, kFileSize2,
                          OfflinePageRequestJob::AggregatedRequestResult::
                              SHOW_OFFLINE_ON_DISCONNECTED_NETWORK);
  ExpectOfflinePageAccessCount(offline_id1, 0);
}

TEST_F(OfflinePageRequestJobTest, LoadOfflinePageByOfflineID) {
  SimulateHasNetworkConnectivity(true);

  // Save 2 offline pages associated with same online URL, but pointing to
  // different archive file.
  int64_t offline_id1 =
      SaveInternalPage(kUrl, GURL(), kFilename1, kFileSize1, std::string());
  int64_t offline_id2 =
      SaveInternalPage(kUrl, GURL(), kFilename2, kFileSize2, std::string());

  // Load an URL with a specific offline ID designated in the custom header.
  // Expect the offline page matching the offline id is fetched.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(
      UseOfflinePageHeader(OfflinePageHeader::Reason::DOWNLOAD, offline_id1));
  LoadPageWithHeaders(kUrl, extra_headers);

  ExpectOfflinePageServed(offline_id1, kFileSize1,
                          OfflinePageRequestJob::AggregatedRequestResult::
                              SHOW_OFFLINE_ON_CONNECTED_NETWORK);
  ExpectOfflinePageAccessCount(offline_id2, 0);
}

TEST_F(OfflinePageRequestJobTest, FailToLoadByOfflineIDOnUrlMismatch) {
  SimulateHasNetworkConnectivity(true);

  int64_t offline_id =
      SaveInternalPage(kUrl, GURL(), kFilename1, kFileSize1, std::string());

  // The offline page found with specific offline ID does not match the passed
  // online URL. Should fall back to find the offline page based on the online
  // URL.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(
      UseOfflinePageHeader(OfflinePageHeader::Reason::DOWNLOAD, offline_id));
  LoadPageWithHeaders(kUrl2, extra_headers);

  ExpectNoOfflinePageServed(offline_id,
                            OfflinePageRequestJob::AggregatedRequestResult::
                                PAGE_NOT_FOUND_ON_CONNECTED_NETWORK);
}

TEST_F(OfflinePageRequestJobTest, DISABLED_LoadOfflinePageForUrlWithFragment) {
  SimulateHasNetworkConnectivity(false);

  // Save an offline page associated with online URL without fragment.
  int64_t offline_id1 =
      SaveInternalPage(kUrl, GURL(), kFilename1, kFileSize1, std::string());

  // Save another offline page associated with online URL that has a fragment.
  GURL url2_with_fragment(kUrl2.spec() + "#ref");
  int64_t offline_id2 = SaveInternalPage(url2_with_fragment, GURL(), kFilename2,
                                         kFileSize2, std::string());

  ExpectOfflinePageAccessCount(offline_id1, 0);
  ExpectOfflinePageAccessCount(offline_id2, 0);

  // Loads an url with fragment, that will match the offline URL without the
  // fragment.
  GURL url_with_fragment(kUrl.spec() + "#ref");
  LoadPage(url_with_fragment);

  ExpectOfflinePageServed(offline_id1, kFileSize1,
                          OfflinePageRequestJob::AggregatedRequestResult::
                              SHOW_OFFLINE_ON_DISCONNECTED_NETWORK);
  ExpectOfflinePageAccessCount(offline_id2, 0);

  // Loads an url without fragment, that will match the offline URL with the
  // fragment.
  LoadPage(kUrl2);

  EXPECT_EQ(kFileSize2, bytes_read());
  ASSERT_TRUE(offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(offline_id2,
            offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);
  ExpectMultiUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK, 2);
  ExpectOfflinePageSizeTotalSuffixCount(2);
  ExpectOnlinePageSizeTotalSuffixCount(0);
  ExpectOfflinePageAccessCount(offline_id1, 1);
  ExpectOfflinePageAccessCount(offline_id2, 1);

  // Loads an url with fragment, that will match the offline URL with different
  // fragment.
  GURL url2_with_different_fragment(kUrl2.spec() + "#different_ref");
  LoadPage(url2_with_different_fragment);

  EXPECT_EQ(kFileSize2, bytes_read());
  ASSERT_TRUE(offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(offline_id2,
            offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);
  ExpectMultiUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK, 3);
  ExpectOfflinePageSizeTotalSuffixCount(3);
  ExpectOnlinePageSizeTotalSuffixCount(0);
  ExpectOfflinePageAccessCount(offline_id1, 1);
  ExpectOfflinePageAccessCount(offline_id2, 2);
}

TEST_F(OfflinePageRequestJobTest, LoadOfflinePageAfterRedirect) {
  SimulateHasNetworkConnectivity(false);

  // Save an offline page with same original URL and final URL.
  int64_t offline_id =
      SaveInternalPage(kUrl, kUrl2, kFilename1, kFileSize1, std::string());

  // This should trigger redirect first.
  LoadPage(kUrl2);

  // Passing AGGREGATED_REQUEST_RESULT_MAX to skip checking request result in
  // the helper function. Different checks will be done after that.
  ExpectOfflinePageServed(offline_id, kFileSize1,
                          OfflinePageRequestJob::AggregatedRequestResult::
                              AGGREGATED_REQUEST_RESULT_MAX);
  ExpectOneNonuniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          REDIRECTED_ON_DISCONNECTED_NETWORK);
  ExpectOneNonuniqueSampleForAggregatedRequestResult(
      OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK);
}

TEST_F(OfflinePageRequestJobTest, NoRedirectForOfflinePageWithSameOriginalURL) {
  SimulateHasNetworkConnectivity(false);

  // Skip the logic to clear the original URL if it is same as final URL.
  // This is needed in order to test that offline page request handler can
  // omit the redirect under this circumstance, for compatibility with the
  // metadata already written to the store.
  OfflinePageModelTaskified* model = static_cast<OfflinePageModelTaskified*>(
      OfflinePageModelFactory::GetForBrowserContext(profile()));
  model->SetSkipClearingOriginalUrlForTesting();

  // Save an offline page with same original URL and final URL.
  int64_t offline_id =
      SaveInternalPage(kUrl, kUrl, kFilename1, kFileSize1, std::string());

  // Check if the original URL is still present.
  OfflinePageItem page = GetPage(offline_id);
  EXPECT_EQ(kUrl, page.original_url);

  // No redirect should be triggered when original URL is same as final URL.
  LoadPage(kUrl);

  ExpectOfflinePageServed(offline_id, kFileSize1,
                          OfflinePageRequestJob::AggregatedRequestResult::
                              SHOW_OFFLINE_ON_DISCONNECTED_NETWORK);
}

TEST_F(OfflinePageRequestJobTest, LoadOfflinePageFromNonExistentInternalFile) {
  SimulateHasNetworkConnectivity(false);

  // Save an offline page pointing to non-existent internal archive file.
  int64_t offline_id = SaveInternalPage(kUrl, GURL(), kNonexistentFilename,
                                        kFileSize1, std::string());

  LoadPage(kUrl);

  ExpectNoOfflinePageServed(
      offline_id,
      OfflinePageRequestJob::AggregatedRequestResult::FILE_NOT_FOUND);
}

TEST_F(OfflinePageRequestJobTest, LoadOfflinePageFromNonExistentPublicFile) {
  SimulateHasNetworkConnectivity(false);

  // Save an offline page pointing to non-existent public archive file.
  int64_t offline_id =
      SavePublicPage(kUrl, GURL(), kNonexistentFilename, kFileSize1, kDigest1);

  LoadPage(kUrl);

  ExpectNoOfflinePageServed(
      offline_id,
      OfflinePageRequestJob::AggregatedRequestResult::FILE_NOT_FOUND);
}

TEST_F(OfflinePageRequestJobTest, FileSizeMismatchOnDisconnectedNetwork) {
  SimulateHasNetworkConnectivity(false);

  // Save an offline page in public location with mismatched file size.
  int64_t offline_id =
      SavePublicPage(kUrl, GURL(), kFilename1, kMismatchedFileSize, kDigest1);

  LoadPage(kUrl);

  ExpectNoOfflinePageServed(offline_id,
                            OfflinePageRequestJob::AggregatedRequestResult::
                                DIGEST_MISMATCH_ON_DISCONNECTED_NETWORK);
}

TEST_F(OfflinePageRequestJobTest, FileSizeMismatchOnProhibitivelySlowNetwork) {
  SimulateHasNetworkConnectivity(true);
  test_previews_decider()->set_should_allow_preview(true);

  // Save an offline page in public location with mismatched file size.
  int64_t offline_id =
      SavePublicPage(kUrl, GURL(), kFilename1, kMismatchedFileSize, kDigest1);

  LoadPage(kUrl);

  ExpectNoOfflinePageServed(offline_id,
                            OfflinePageRequestJob::AggregatedRequestResult::
                                DIGEST_MISMATCH_ON_PROHIBITIVELY_SLOW_NETWORK);
}

TEST_F(OfflinePageRequestJobTest, FileSizeMismatchOnConnectedNetwork) {
  SimulateHasNetworkConnectivity(true);

  // Save an offline page in public location with mismatched file size.
  int64_t offline_id =
      SavePublicPage(kUrl, GURL(), kFilename1, kMismatchedFileSize, kDigest1);

  // When custom offline header exists and contains value other than
  // "reason=error", it means that offline page is forced to load.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(
      UseOfflinePageHeader(OfflinePageHeader::Reason::DOWNLOAD, 0));
  LoadPageWithHeaders(kUrl, extra_headers);

  ExpectNoOfflinePageServed(offline_id,
                            OfflinePageRequestJob::AggregatedRequestResult::
                                DIGEST_MISMATCH_ON_CONNECTED_NETWORK);
}

TEST_F(OfflinePageRequestJobTest, FileSizeMismatchOnFlakyNetwork) {
  SimulateHasNetworkConnectivity(true);

  // Save an offline page in public location with mismatched file size.
  int64_t offline_id =
      SavePublicPage(kUrl, GURL(), kFilename1, kMismatchedFileSize, kDigest1);

  // When custom offline header exists and contains "reason=error", it means
  // that net error is hit in last request due to flaky network.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(
      UseOfflinePageHeader(OfflinePageHeader::Reason::NET_ERROR, 0));
  LoadPageWithHeaders(kUrl, extra_headers);

  ExpectNoOfflinePageServed(offline_id,
                            OfflinePageRequestJob::AggregatedRequestResult::
                                DIGEST_MISMATCH_ON_FLAKY_NETWORK);
}

TEST_F(OfflinePageRequestJobTest, DigestMismatchOnDisconnectedNetwork) {
  SimulateHasNetworkConnectivity(false);

  // Save an offline page in public location with mismatched digest.
  int64_t offline_id =
      SavePublicPage(kUrl, GURL(), kFilename1, kFileSize1, kMismatchedDigest);

  LoadPage(kUrl);

  ExpectNoOfflinePageServed(offline_id,
                            OfflinePageRequestJob::AggregatedRequestResult::
                                DIGEST_MISMATCH_ON_DISCONNECTED_NETWORK);
}

TEST_F(OfflinePageRequestJobTest, DigestMismatchOnProhibitivelySlowNetwork) {
  SimulateHasNetworkConnectivity(true);
  test_previews_decider()->set_should_allow_preview(true);

  // Save an offline page in public location with mismatched digest.
  int64_t offline_id =
      SavePublicPage(kUrl, GURL(), kFilename1, kFileSize1, kMismatchedDigest);

  LoadPage(kUrl);

  ExpectNoOfflinePageServed(offline_id,
                            OfflinePageRequestJob::AggregatedRequestResult::
                                DIGEST_MISMATCH_ON_PROHIBITIVELY_SLOW_NETWORK);
}

TEST_F(OfflinePageRequestJobTest, DigestMismatchOnConnectedNetwork) {
  SimulateHasNetworkConnectivity(true);

  // Save an offline page in public location with mismatched digest.
  int64_t offline_id =
      SavePublicPage(kUrl, GURL(), kFilename1, kFileSize1, kMismatchedDigest);

  // When custom offline header exists and contains value other than
  // "reason=error", it means that offline page is forced to load.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(
      UseOfflinePageHeader(OfflinePageHeader::Reason::DOWNLOAD, 0));
  LoadPageWithHeaders(kUrl, extra_headers);

  ExpectNoOfflinePageServed(offline_id,
                            OfflinePageRequestJob::AggregatedRequestResult::
                                DIGEST_MISMATCH_ON_CONNECTED_NETWORK);
}

TEST_F(OfflinePageRequestJobTest, DigestMismatchOnFlakyNetwork) {
  SimulateHasNetworkConnectivity(true);

  // Save an offline page in public location with mismatched digest.
  int64_t offline_id =
      SavePublicPage(kUrl, GURL(), kFilename1, kFileSize1, kMismatchedDigest);

  // When custom offline header exists and contains "reason=error", it means
  // that net error is hit in last request due to flaky network.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(
      UseOfflinePageHeader(OfflinePageHeader::Reason::NET_ERROR, 0));
  LoadPageWithHeaders(kUrl, extra_headers);

  ExpectNoOfflinePageServed(offline_id,
                            OfflinePageRequestJob::AggregatedRequestResult::
                                DIGEST_MISMATCH_ON_FLAKY_NETWORK);
}

TEST_F(OfflinePageRequestJobTest, FailOnNoDigestForPublicArchiveFile) {
  SimulateHasNetworkConnectivity(false);

  // Save an offline page in public location with no digest.
  int64_t offline_id =
      SavePublicPage(kUrl, GURL(), kFilename1, kFileSize1, std::string());

  LoadPage(kUrl);

  ExpectNoOfflinePageServed(offline_id,
                            OfflinePageRequestJob::AggregatedRequestResult::
                                DIGEST_MISMATCH_ON_DISCONNECTED_NETWORK);
}

TEST_F(OfflinePageRequestJobTest, FailToLoadByOfflineIDOnDigestMismatch) {
  SimulateHasNetworkConnectivity(true);

  // Save 2 offline pages associated with same online URL, one in internal
  // location, while another in public location with mismatched digest.
  int64_t offline_id1 =
      SaveInternalPage(kUrl, GURL(), kFilename1, kFileSize1, std::string());
  int64_t offline_id2 =
      SavePublicPage(kUrl, GURL(), kFilename1, kFileSize1, kMismatchedDigest);

  // The offline page found with specific offline ID does not pass the
  // validation. Though there is another page with the same URL, it will not be
  // fetched. Instead, fall back to load the online URL.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(
      UseOfflinePageHeader(OfflinePageHeader::Reason::DOWNLOAD, offline_id2));
  LoadPageWithHeaders(kUrl, extra_headers);

  ExpectNoOfflinePageServed(offline_id1,
                            OfflinePageRequestJob::AggregatedRequestResult::
                                DIGEST_MISMATCH_ON_CONNECTED_NETWORK);
  ExpectOfflinePageAccessCount(offline_id2, 0);
}

TEST_F(OfflinePageRequestJobTest, LoadOtherPageOnDigestMismatch) {
  SimulateHasNetworkConnectivity(false);

  // Save 2 offline pages associated with same online URL, one in internal
  // location, while another in public location with mismatched digest.
  int64_t offline_id1 =
      SaveInternalPage(kUrl, GURL(), kFilename1, kFileSize1, std::string());
  int64_t offline_id2 =
      SavePublicPage(kUrl, GURL(), kFilename2, kFileSize2, kMismatchedDigest);
  ExpectOfflinePageAccessCount(offline_id1, 0);
  ExpectOfflinePageAccessCount(offline_id2, 0);

  // There're 2 offline pages matching kUrl. The most recently created one
  // should fail on mistmatched digest. The second most recently created offline
  // page should work.
  LoadPage(kUrl);

  ExpectOfflinePageServed(offline_id1, kFileSize1,
                          OfflinePageRequestJob::AggregatedRequestResult::
                              SHOW_OFFLINE_ON_DISCONNECTED_NETWORK);
  ExpectOfflinePageAccessCount(offline_id2, 0);
}

TEST_F(OfflinePageRequestJobTest, TinyFile) {
  SimulateHasNetworkConnectivity(false);

  std::string expected_data("hello world");
  base::FilePath temp_file_path = CreateFileWithContent(expected_data);
  ArchiveValidator archive_validator;
  archive_validator.Update(expected_data.c_str(), expected_data.length());
  std::string expected_digest = archive_validator.Finish();
  int expected_size = expected_data.length();

  int64_t offline_id = SavePublicPage(kUrl, GURL(), temp_file_path,
                                      expected_size, expected_digest);

  LoadPage(kUrl);

  ExpectOfflinePageServed(offline_id, expected_size,
                          OfflinePageRequestJob::AggregatedRequestResult::
                              SHOW_OFFLINE_ON_DISCONNECTED_NETWORK);
  EXPECT_EQ(expected_data, data_received());
}

TEST_F(OfflinePageRequestJobTest, SmallFile) {
  SimulateHasNetworkConnectivity(false);

  std::string expected_data(MakeContentOfSize(2 * 1024));
  base::FilePath temp_file_path = CreateFileWithContent(expected_data);
  ArchiveValidator archive_validator;
  archive_validator.Update(expected_data.c_str(), expected_data.length());
  std::string expected_digest = archive_validator.Finish();
  int expected_size = expected_data.length();

  int64_t offline_id = SavePublicPage(kUrl, GURL(), temp_file_path,
                                      expected_size, expected_digest);

  LoadPage(kUrl);

  ExpectOfflinePageServed(offline_id, expected_size,
                          OfflinePageRequestJob::AggregatedRequestResult::
                              SHOW_OFFLINE_ON_DISCONNECTED_NETWORK);
  EXPECT_EQ(expected_data, data_received());
}

TEST_F(OfflinePageRequestJobTest, BigFile) {
  SimulateHasNetworkConnectivity(false);

  std::string expected_data(MakeContentOfSize(3 * 1024 * 1024));
  base::FilePath temp_file_path = CreateFileWithContent(expected_data);
  ArchiveValidator archive_validator;
  archive_validator.Update(expected_data.c_str(), expected_data.length());
  std::string expected_digest = archive_validator.Finish();
  int expected_size = expected_data.length();

  int64_t offline_id = SavePublicPage(kUrl, GURL(), temp_file_path,
                                      expected_size, expected_digest);

  LoadPage(kUrl);

  ExpectOfflinePageServed(offline_id, expected_size,
                          OfflinePageRequestJob::AggregatedRequestResult::
                              SHOW_OFFLINE_ON_DISCONNECTED_NETWORK);
  EXPECT_EQ(expected_data, data_received());
}

TEST_F(OfflinePageRequestJobTest, LoadFromFileUrlIntent) {
  SimulateHasNetworkConnectivity(true);

  std::string expected_data(MakeContentOfSize(2 * 1024));
  ArchiveValidator archive_validator;
  archive_validator.Update(expected_data.c_str(), expected_data.length());
  std::string expected_digest = archive_validator.Finish();
  int expected_size = expected_data.length();

  // Create a file with unmodified data. The path to this file will be feed
  // into "intent_url" of extra headers.
  base::FilePath unmodified_file_path = CreateFileWithContent(expected_data);

  // Create a file with modified data. An offline page is created to associate
  // with this file, but with size and digest matching the unmodified version.
  std::string modified_data(expected_data);
  modified_data[10] = '@';
  base::FilePath modified_file_path = CreateFileWithContent(modified_data);

  int64_t offline_id = SavePublicPage(kUrl, GURL(), modified_file_path,
                                      expected_size, expected_digest);

  // Load an URL with custom header that contains "intent_url" pointing to
  // unmodified file. Expect the file from the intent URL is fetched.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(UseOfflinePageHeaderForIntent(
      OfflinePageHeader::Reason::FILE_URL_INTENT, offline_id,
      net::FilePathToFileURL(unmodified_file_path)));
  LoadPageWithHeaders(kUrl, extra_headers);

  ExpectOfflinePageServed(offline_id, expected_size,
                          OfflinePageRequestJob::AggregatedRequestResult::
                              SHOW_OFFLINE_ON_CONNECTED_NETWORK);
  EXPECT_EQ(expected_data, data_received());
}

TEST_F(OfflinePageRequestJobTest, IntentFileNotFound) {
  SimulateHasNetworkConnectivity(true);

  std::string expected_data(MakeContentOfSize(2 * 1024));
  ArchiveValidator archive_validator;
  archive_validator.Update(expected_data.c_str(), expected_data.length());
  std::string expected_digest = archive_validator.Finish();
  int expected_size = expected_data.length();

  // Create a file with unmodified data. An offline page is created to associate
  // with this file.
  base::FilePath unmodified_file_path = CreateFileWithContent(expected_data);

  // Get a path pointing to non-existing file. This path will be feed into
  // "intent_url" of extra headers.
  base::FilePath nonexistent_file_path =
      unmodified_file_path.DirName().AppendASCII("nonexistent");

  int64_t offline_id = SavePublicPage(kUrl, GURL(), unmodified_file_path,
                                      expected_size, expected_digest);

  // Load an URL with custom header that contains "intent_url" pointing to
  // non-existent file. Expect the request fails.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(UseOfflinePageHeaderForIntent(
      OfflinePageHeader::Reason::FILE_URL_INTENT, offline_id,
      net::FilePathToFileURL(nonexistent_file_path)));
  LoadPageWithHeaders(kUrl, extra_headers);

  ExpectOpenFileErrorCode(net::ERR_FILE_NOT_FOUND);
  EXPECT_EQ(net::ERR_FAILED, request_status());
  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(is_offline_page_set_in_navigation_data());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
}

TEST_F(OfflinePageRequestJobTest, IntentFileModifiedInTheMiddle) {
  SimulateHasNetworkConnectivity(true);

  std::string expected_data(MakeContentOfSize(2 * 1024));
  ArchiveValidator archive_validator;
  archive_validator.Update(expected_data.c_str(), expected_data.length());
  std::string expected_digest = archive_validator.Finish();
  int expected_size = expected_data.length();

  // Create a file with modified data in the middle. An offline page is created
  // to associate with this modified file, but with size and digest matching the
  // unmodified version.
  std::string modified_data(expected_data);
  modified_data[10] = '@';
  base::FilePath modified_file_path = CreateFileWithContent(modified_data);

  int64_t offline_id = SavePublicPage(kUrl, GURL(), modified_file_path,
                                      expected_size, expected_digest);

  // Load an URL with custom header that contains "intent_url" pointing to
  // modified file. Expect the request fails.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(UseOfflinePageHeaderForIntent(
      OfflinePageHeader::Reason::FILE_URL_INTENT, offline_id,
      net::FilePathToFileURL(modified_file_path)));
  LoadPageWithHeaders(kUrl, extra_headers);

  EXPECT_EQ(net::ERR_FAILED, request_status());
  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(is_offline_page_set_in_navigation_data());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
}

TEST_F(OfflinePageRequestJobTest, IntentFileModifiedWithMoreDataAppended) {
  SimulateHasNetworkConnectivity(true);

  std::string expected_data(MakeContentOfSize(2 * 1024));
  ArchiveValidator archive_validator;
  archive_validator.Update(expected_data.c_str(), expected_data.length());
  std::string expected_digest = archive_validator.Finish();
  int expected_size = expected_data.length();

  // Create a file with more data appended. An offline page is created to
  // associate with this modified file, but with size and digest matching the
  // unmodified version.
  std::string modified_data(expected_data);
  modified_data += "foo";
  base::FilePath modified_file_path = CreateFileWithContent(modified_data);

  int64_t offline_id = SavePublicPage(kUrl, GURL(), modified_file_path,
                                      expected_size, expected_digest);

  // Load an URL with custom header that contains "intent_url" pointing to
  // modified file. Expect the request fails.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(UseOfflinePageHeaderForIntent(
      OfflinePageHeader::Reason::FILE_URL_INTENT, offline_id,
      net::FilePathToFileURL(modified_file_path)));
  LoadPageWithHeaders(kUrl, extra_headers);

  EXPECT_EQ(net::ERR_FAILED, request_status());
  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(is_offline_page_set_in_navigation_data());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
}

}  // namespace offline_pages
