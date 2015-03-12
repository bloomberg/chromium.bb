// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/history/chrome_history_client.h"
#include "chrome/browser/history/chrome_history_client_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/database_manager.h"
#include "chrome/browser/safe_browsing/incident_reporting/off_domain_inclusion_detector.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/history/content/browser/content_visit_delegate.h"
#include "components/history/content/browser/history_database_helper.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/service_access_type.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/request_priority.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::Eq;
using testing::NiceMock;
using testing::Return;
using testing::Values;

namespace {

const int kFakeRenderProcessId = 123;

// A BrowserContextKeyedServiceFactory::TestingFactoryFunction that creates a
// HistoryService for a TestingProfile.
KeyedService* BuildHistoryService(content::BrowserContext* context) {
  TestingProfile* profile = static_cast<TestingProfile*>(context);

  // Delete the file before creating the service.
  base::FilePath history_path(
      profile->GetPath().Append(history::kHistoryFilename));
  if (!base::DeleteFile(history_path, false) ||
      base::PathExists(history_path)) {
    ADD_FAILURE() << "failed to delete history db file "
                  << history_path.value();
    return NULL;
  }

  history::HistoryService* history_service = new history::HistoryService(
      ChromeHistoryClientFactory::GetForProfile(profile),
      scoped_ptr<history::VisitDelegate>());
  if (history_service->Init(
          profile->GetPrefs()->GetString(prefs::kAcceptLanguages),
          history::HistoryDatabaseParamsForPath(profile->GetPath()))) {
    return history_service;
  }

  ADD_FAILURE() << "failed to initialize history service";
  delete history_service;
  return NULL;
}

}  // namespace

namespace safe_browsing {

using AnalysisEvent = OffDomainInclusionDetector::AnalysisEvent;

const content::ResourceType kResourceTypesObservedIfParentIsMainFrame[] = {
    content::RESOURCE_TYPE_SUB_FRAME,
};

const content::ResourceType kResourceTypesObservedIfInMainFrame[] = {
    content::RESOURCE_TYPE_STYLESHEET,
    content::RESOURCE_TYPE_SCRIPT,
    content::RESOURCE_TYPE_IMAGE,
    content::RESOURCE_TYPE_FONT_RESOURCE,
    content::RESOURCE_TYPE_SUB_RESOURCE,
    content::RESOURCE_TYPE_OBJECT,
    content::RESOURCE_TYPE_MEDIA,
    content::RESOURCE_TYPE_XHR,
};

const content::ResourceType kResourceTypesIgnored[] = {
    content::RESOURCE_TYPE_MAIN_FRAME,
    content::RESOURCE_TYPE_WORKER,
    content::RESOURCE_TYPE_SHARED_WORKER,
    content::RESOURCE_TYPE_PREFETCH,
    content::RESOURCE_TYPE_FAVICON,
    content::RESOURCE_TYPE_PING,
    content::RESOURCE_TYPE_SERVICE_WORKER,
};

static_assert(
    arraysize(kResourceTypesObservedIfParentIsMainFrame) +
        arraysize(kResourceTypesObservedIfInMainFrame) +
        arraysize(kResourceTypesIgnored) == content::RESOURCE_TYPE_LAST_TYPE,
    "Expected resource types list aren't comprehensive");

// A set of test cases to run each parametrized test case below through.
enum class TestCase {
  WHITELISTED,
  IN_HISTORY,
  IN_HISTORY_AND_WHITELISTED,
  UNKNOWN,
};

class MockSafeBrowsingDatabaseManager : public SafeBrowsingDatabaseManager {
 public:
  explicit MockSafeBrowsingDatabaseManager(
      const scoped_refptr<SafeBrowsingService>& service)
      : SafeBrowsingDatabaseManager(service) {}

  MOCK_METHOD1(MatchInclusionWhitelistUrl, bool(const GURL& url));

 protected:
  virtual ~MockSafeBrowsingDatabaseManager() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSafeBrowsingDatabaseManager);
};

// A mock OffDomainInclusionDetector which mocks out
// |ProfileFromRenderProcessId()| so that tests can inject their TestingProfile
// into the OffDomainInclusionDetector.
class MockOffDomainInclusionDetector
    : public NiceMock<OffDomainInclusionDetector> {
 public:
  MockOffDomainInclusionDetector(
      const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
      const ReportAnalysisEventCallback& report_analysis_event_callback)
      : NiceMock<OffDomainInclusionDetector>(database_manager,
                                             report_analysis_event_callback) {}

  MOCK_METHOD1(ProfileFromRenderProcessId, Profile*(int render_process_id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockOffDomainInclusionDetector);
};

// Adds |url| to |history_service| upon being constructed and removes it upon
// being destructed.
class ScopedHistoryEntry {
 public:
  ScopedHistoryEntry(history::HistoryService* history_service, const GURL& url)
      : history_service_(history_service), url_(url) {
    base::RunLoop run_loop;

    history_service_->AddPage(url_, base::Time::Now(), history::SOURCE_BROWSED);

    // Flush tasks posted on the history thread.
    history_service_->FlushForTest(run_loop.QuitClosure());
    run_loop.Run();
    // Make sure anything bounced back to the main thread has been handled.
    base::RunLoop().RunUntilIdle();
  }

  ~ScopedHistoryEntry() {
    base::RunLoop run_loop;

    history_service_->DeleteURL(url_);

    history_service_->FlushForTest(run_loop.QuitClosure());
    run_loop.Run();
    base::RunLoop().RunUntilIdle();
  }

 private:
  history::HistoryService* history_service_;
  const GURL url_;

  DISALLOW_COPY_AND_ASSIGN(ScopedHistoryEntry);
};

class OffDomainInclusionDetectorTest : public testing::TestWithParam<TestCase> {
 protected:
  OffDomainInclusionDetectorTest()
      : testing_profile_(nullptr),
        observed_analysis_event_(AnalysisEvent::NO_EVENT) {}

  void SetUp() override {
    InitTestProfile();
    InitOffDomainInclusionDetector();
  }

  void TearDown() override {
    // Shut down the history service.
    testing_profile_->AsTestingProfile()->DestroyHistoryService();
    profile_manager_.reset();
    TestingBrowserProcess::DeleteInstance();
  }

  AnalysisEvent GetLastEventAndReset() {
    const AnalysisEvent last_event = observed_analysis_event_;
    observed_analysis_event_ = AnalysisEvent::NO_EVENT;
    return last_event;
  }

  void SimulateTestURLRequest(const std::string& url_str,
                              const std::string& referrer,
                              content::ResourceType resource_type,
                              bool is_main_frame,
                              bool parent_is_main_frame) const {
    const GURL url(url_str);

    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            testing_profile_, ServiceAccessType::EXPLICIT_ACCESS);
    scoped_ptr<ScopedHistoryEntry> scoped_history_entry;
    if (ShouldAddSimulatedURLsToHistory())
      scoped_history_entry.reset(new ScopedHistoryEntry(history_service, url));

    scoped_ptr<net::URLRequest> url_request(
        context_.CreateRequest(url, net::DEFAULT_PRIORITY, NULL, NULL));

    if (!referrer.empty())
      url_request->SetReferrer(referrer);

    content::ResourceRequestInfo::AllocateForTesting(
        url_request.get(),
        resource_type,
        NULL,                  // resource_context
        kFakeRenderProcessId,  // render_process_id
        0,                     // render_view_id
        0,                     // render_frame_id
        is_main_frame,         // is_main_frame
        parent_is_main_frame,  // parent_is_main_frame
        true,                  // allow_download
        false);                // is_async

    off_domain_inclusion_detector_->OnResourceRequest(url_request.get());

    // OffDomainInclusionDetector::OnResourceRequest() may complete
    // asynchronously, run all message loops (i.e. this message loop in unit
    // tests) until idle.
    base::RunLoop().RunUntilIdle();

    // Make sure any task posted by the OffDomainInclusionDetector to the
    // history thread has occurred.
    base::RunLoop run_loop;
    history_service->FlushForTest(run_loop.QuitClosure());
    run_loop.Run();

    // Finally, finish anything the history thread may have bounced back to the
    // main thread.
    base::RunLoop().RunUntilIdle();
  }

  // Returns the expected AnalysisEvent produced when facing an off-domain
  // inclusion in the current test configuration.
  AnalysisEvent GetExpectedOffDomainInclusionEventType() {
    switch (GetParam()) {
      case TestCase::WHITELISTED:
        return AnalysisEvent::OFF_DOMAIN_INCLUSION_WHITELISTED;
      case TestCase::IN_HISTORY:
        return AnalysisEvent::OFF_DOMAIN_INCLUSION_IN_HISTORY;
      case TestCase::IN_HISTORY_AND_WHITELISTED:
        return AnalysisEvent::OFF_DOMAIN_INCLUSION_WHITELISTED;
      case TestCase::UNKNOWN:
        return AnalysisEvent::OFF_DOMAIN_INCLUSION_SUSPICIOUS;
    }
    NOTREACHED();
    return AnalysisEvent::NO_EVENT;
  }

 private:
  // Returns true if the inclusion whitelist should be mocked to return a match
  // on all requests.
  bool ShouldWhitelistEverything() const {
    return GetParam() == TestCase::WHITELISTED ||
           GetParam() == TestCase::IN_HISTORY_AND_WHITELISTED;
  }

  // Returns true if simulated URLs should be added to history.
  bool ShouldAddSimulatedURLsToHistory() const {
    return GetParam() == TestCase::IN_HISTORY ||
           GetParam() == TestCase::IN_HISTORY_AND_WHITELISTED;
  }

  void OnOffDomainInclusionEvent(AnalysisEvent event) {
    // Make sure no other AnalysisEvent was previously reported.
    EXPECT_EQ(AnalysisEvent::NO_EVENT, observed_analysis_event_);
    observed_analysis_event_ = event;
  }

  // Initializes |profile_manager_| and |testing_profile_|.
  void InitTestProfile() {
    ASSERT_FALSE(profile_manager_);
    ASSERT_FALSE(testing_profile_);

    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());

    // Set up a mock keyed service factory for the HistoryService.
    TestingProfile::TestingFactories factories;
    factories.push_back(std::make_pair(HistoryServiceFactory::GetInstance(),
                                       &BuildHistoryService));
    // Suppress WebHistoryService since it makes network requests.
    factories.push_back(std::make_pair(
        WebHistoryServiceFactory::GetInstance(),
        static_cast<BrowserContextKeyedServiceFactory::TestingFactoryFunction>(
            NULL)));

    // Create default prefs for the test profile.
    scoped_ptr<TestingPrefServiceSyncable> prefs(
        new TestingPrefServiceSyncable);
    chrome::RegisterUserProfilePrefs(prefs->registry());

    // |testing_profile_| is owned by |profile_manager_|.
    testing_profile_ = profile_manager_->CreateTestingProfile(
        "profile",                  // profile_name
        prefs.Pass(),
        base::UTF8ToUTF16("user"),  // user_name
        0,                          // avatar_id
        std::string(),              // supervised_user_id
        factories);
  }

  // Initializes |off_domain_inclusion_detector_|, assumes |testing_profile_| is
  // already initialized.
  void InitOffDomainInclusionDetector() {
    ASSERT_TRUE(testing_profile_);
    ASSERT_FALSE(off_domain_inclusion_detector_);

    // Only used for initializing MockSafeBrowsingDatabaseManager.
    scoped_refptr<SafeBrowsingService> sb_service =
        SafeBrowsingService::CreateSafeBrowsingService();

    scoped_refptr<MockSafeBrowsingDatabaseManager>
        mock_safe_browsing_database_manager =
            new NiceMock<MockSafeBrowsingDatabaseManager>(sb_service);
    ON_CALL(*mock_safe_browsing_database_manager, MatchInclusionWhitelistUrl(_))
        .WillByDefault(Return(ShouldWhitelistEverything()));

    off_domain_inclusion_detector_.reset(new MockOffDomainInclusionDetector(
        mock_safe_browsing_database_manager,
        base::Bind(&OffDomainInclusionDetectorTest::OnOffDomainInclusionEvent,
                   base::Unretained(this))));
    // ProfileFromRenderProcessId doesn't *have* to be called, but if it is, it
    // should always request the profile for |kFakeRenderProcessId| and this
    // test will always return it |testing_profile_|.
    ON_CALL(*off_domain_inclusion_detector_,
            ProfileFromRenderProcessId(Eq(kFakeRenderProcessId)))
        .WillByDefault(Return(testing_profile_));
  }

  Profile* testing_profile_;
  scoped_ptr<TestingProfileManager> profile_manager_;

  content::TestBrowserThreadBundle thread_bundle_;
  net::TestURLRequestContext context_;

  AnalysisEvent observed_analysis_event_;

  scoped_ptr<MockOffDomainInclusionDetector> off_domain_inclusion_detector_;

  DISALLOW_COPY_AND_ASSIGN(OffDomainInclusionDetectorTest);
};

TEST_P(OffDomainInclusionDetectorTest, NoEventForIgnoredResourceTypes) {
  for (content::ResourceType tested_type : kResourceTypesIgnored) {
    SCOPED_TRACE(tested_type);

    SimulateTestURLRequest("http://offdomain.com/foo",
                           "http://mydomain.com/bar",
                           tested_type,
                           true,    // is_main_frame
                           false);  // parent_is_main_frame

    EXPECT_EQ(AnalysisEvent::NO_EVENT, GetLastEventAndReset());
  }
}

TEST_P(OffDomainInclusionDetectorTest, NoEventForSameDomainInclusions) {
  for (content::ResourceType tested_type :
       kResourceTypesObservedIfInMainFrame) {
    SCOPED_TRACE(tested_type);

    SimulateTestURLRequest("http://mydomain.com/foo",
                           "http://mydomain.com/bar",
                           tested_type,
                           true,    // is_main_frame
                           false);  // parent_is_main_frame

    EXPECT_EQ(AnalysisEvent::NO_EVENT, GetLastEventAndReset());
  }
}

TEST_P(OffDomainInclusionDetectorTest, OffDomainInclusionInMainFrame) {
  for (content::ResourceType tested_type :
       kResourceTypesObservedIfInMainFrame) {
    SCOPED_TRACE(tested_type);

    SimulateTestURLRequest("http://offdomain.com/foo",
                           "http://mydomain.com/bar",
                           tested_type,
                           true,    // is_main_frame
                           false);  // parent_is_main_frame

    EXPECT_EQ(GetExpectedOffDomainInclusionEventType(), GetLastEventAndReset());
  }
}

TEST_P(OffDomainInclusionDetectorTest, HttpsOffDomainInclusionInMainFrame) {
  for (content::ResourceType tested_type :
       kResourceTypesObservedIfInMainFrame) {
    SCOPED_TRACE(tested_type);

    SimulateTestURLRequest("https://offdomain.com/foo",
                           "https://mydomain.com/bar",
                           tested_type,
                           true,    // is_main_frame
                           false);  // parent_is_main_frame

    EXPECT_EQ(GetExpectedOffDomainInclusionEventType(), GetLastEventAndReset());
  }
}

TEST_P(OffDomainInclusionDetectorTest,
       NoEventForNonHttpOffDomainInclusionInMainFrame) {
  for (content::ResourceType tested_type :
       kResourceTypesObservedIfInMainFrame) {
    SCOPED_TRACE(tested_type);

    SimulateTestURLRequest("ftp://offdomain.com/foo",
                           "http://mydomain.com/bar",
                           tested_type,
                           true,    // is_main_frame
                           false);  // parent_is_main_frame

    EXPECT_EQ(AnalysisEvent::NO_EVENT, GetLastEventAndReset());
  }
}

TEST_P(OffDomainInclusionDetectorTest, NoEventForSameTopLevelDomain) {
  for (content::ResourceType tested_type :
       kResourceTypesObservedIfInMainFrame) {
    SCOPED_TRACE(tested_type);

    SimulateTestURLRequest("http://a.mydomain.com/foo",
                           "http://b.mydomain.com/bar",
                           tested_type,
                           true,    // is_main_frame
                           false);  // parent_is_main_frame

    EXPECT_EQ(AnalysisEvent::NO_EVENT, GetLastEventAndReset());
  }
}

TEST_P(OffDomainInclusionDetectorTest,
       OffDomainInclusionForSameTopLevelRegistryButDifferentDomain) {
  for (content::ResourceType tested_type :
       kResourceTypesObservedIfInMainFrame) {
    SCOPED_TRACE(tested_type);

    SimulateTestURLRequest("http://a.appspot.com/foo",
                           "http://b.appspot.com/bar",
                           tested_type,
                           true,    // is_main_frame
                           false);  // parent_is_main_frame

    EXPECT_EQ(GetExpectedOffDomainInclusionEventType(), GetLastEventAndReset());
  }
}

TEST_P(OffDomainInclusionDetectorTest,
       NoEventForOffDomainRegularResourceInSubframe) {
  for (content::ResourceType tested_type :
       kResourceTypesObservedIfInMainFrame) {
    SCOPED_TRACE(tested_type);

    SimulateTestURLRequest("http://offdomain.com/foo",
                           "http://mydomain.com/bar",
                           tested_type,
                           false,  // is_main_frame
                           true);  // parent_is_main_frame

    EXPECT_EQ(AnalysisEvent::NO_EVENT, GetLastEventAndReset());
  }
}

TEST_P(OffDomainInclusionDetectorTest,
       NoEventForOffDomainSubSubFrameInclusion) {
  for (content::ResourceType tested_type :
       kResourceTypesObservedIfParentIsMainFrame) {
    SCOPED_TRACE(tested_type);

    SimulateTestURLRequest("http://offdomain.com/foo",
                           "http://mydomain.com/bar",
                           tested_type,
                           false,   // is_main_frame
                           false);  // parent_is_main_frame

    EXPECT_EQ(AnalysisEvent::NO_EVENT, GetLastEventAndReset());
  }
}

TEST_P(OffDomainInclusionDetectorTest,
       OffDomainInclusionForOffDomainResourcesObservedIfParentIsMainFrame) {
  for (content::ResourceType tested_type :
       kResourceTypesObservedIfParentIsMainFrame) {
    SCOPED_TRACE(tested_type);

    SimulateTestURLRequest("http://offdomain.com/foo",
                           "http://mydomain.com/bar",
                           tested_type,
                           false,  // is_main_frame
                           true);  // parent_is_main_frame

    EXPECT_EQ(GetExpectedOffDomainInclusionEventType(), GetLastEventAndReset());
  }
}

TEST_P(OffDomainInclusionDetectorTest,
       EmptyEventForOffDomainInclusionWithNoReferrer) {
  for (content::ResourceType tested_type :
       kResourceTypesObservedIfInMainFrame) {
    SCOPED_TRACE(tested_type);

    SimulateTestURLRequest("https://offdomain.com/foo",
                           "",
                           tested_type,
                           true,    // is_main_frame
                           false);  // parent_is_main_frame

    EXPECT_EQ(AnalysisEvent::ABORT_EMPTY_MAIN_FRAME_URL,
              GetLastEventAndReset());
  }
}

INSTANTIATE_TEST_CASE_P(OffDomainInclusionDetectorTestInstance,
                        OffDomainInclusionDetectorTest,
                        Values(TestCase::WHITELISTED,
                               TestCase::IN_HISTORY,
                               TestCase::IN_HISTORY_AND_WHITELISTED,
                               TestCase::UNKNOWN));

}  // namespace safe_browsing
