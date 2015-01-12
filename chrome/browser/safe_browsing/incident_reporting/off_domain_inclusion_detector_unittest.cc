// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/safe_browsing/database_manager.h"
#include "chrome/browser/safe_browsing/incident_reporting/off_domain_inclusion_detector.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "ipc/ipc_message.h"
#include "net/base/request_priority.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::Values;

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
enum class OffDomainInclusionTestCases {
  OFF_DOMAIN_INCLUSION_WHITELISTED,
  OFF_DOMAIN_INCLUSION_UNKNOWN,
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

class OffDomainInclusionDetectorTest
    : public testing::TestWithParam<OffDomainInclusionTestCases> {
 protected:
  OffDomainInclusionDetectorTest()
      : observed_analysis_event_(AnalysisEvent::NO_EVENT) {}

  void SetUp() override {
    // Only used for initializing MockSafeBrowsingDatabaseManager.
    scoped_refptr<SafeBrowsingService> sb_service =
        SafeBrowsingService::CreateSafeBrowsingService();

    scoped_refptr<MockSafeBrowsingDatabaseManager>
        mock_safe_browsing_database_manager =
            new NiceMock<MockSafeBrowsingDatabaseManager>(sb_service);
    ON_CALL(*mock_safe_browsing_database_manager, MatchInclusionWhitelistUrl(_))
        .WillByDefault(Return(
            GetParam() ==
            OffDomainInclusionTestCases::OFF_DOMAIN_INCLUSION_WHITELISTED));

    off_domain_inclusion_detector_.reset(new OffDomainInclusionDetector(
        mock_safe_browsing_database_manager,
        base::Bind(&OffDomainInclusionDetectorTest::OnOffDomainInclusionEvent,
                   base::Unretained(this))));
  }

  void TearDown() override {
    // The SafeBrowsingService held by the MockSafeBrowsingDatabaseManager in
    // |off_domain_inclusion_detector_| is deleted asynchronously and we thus
    // need to cleanup explicitly here or the test will leak.
    off_domain_inclusion_detector_.reset();
    base::RunLoop().RunUntilIdle();
  }

  AnalysisEvent GetLastEventAndReset() {
    const AnalysisEvent last_event = observed_analysis_event_;
    observed_analysis_event_ = AnalysisEvent::NO_EVENT;
    return last_event;
  }

  void SimulateTestURLRequest(const std::string& url,
                              const std::string& referrer,
                              content::ResourceType resource_type,
                              bool is_main_frame,
                              bool parent_is_main_frame) const {
    scoped_ptr<net::URLRequest> url_request(
        context_.CreateRequest(GURL(url), net::DEFAULT_PRIORITY, NULL, NULL));

    if (!referrer.empty())
      url_request->SetReferrer(referrer);

    content::ResourceRequestInfo::AllocateForTesting(
        url_request.get(),
        resource_type,
        NULL,                  // resource_context
        0,                     // render_process_id
        0,                     // render_view_id
        MSG_ROUTING_NONE,      // render_frame_id
        is_main_frame,         // is_main_frame
        parent_is_main_frame,  // parent_is_main_frame
        true,                  // allow_download
        false);                // is_async

    off_domain_inclusion_detector_->OnResourceRequest(url_request.get());

    // OffDomainInclusionDetector::OnResourceRequest() sometimes completes
    // asynchronously, run all message loops (i.e. this message loop in unit
    // tests) until idle.
    base::RunLoop().RunUntilIdle();
  }

  // Returns the expected AnalysisEvent produced when facing an off-domain
  // inclusion in the current test configuration.
  AnalysisEvent GetExpectedOffDomainInclusionEventType() {
    return GetParam() ==
                   OffDomainInclusionTestCases::OFF_DOMAIN_INCLUSION_WHITELISTED
               ? AnalysisEvent::OFF_DOMAIN_INCLUSION_WHITELISTED
               : AnalysisEvent::OFF_DOMAIN_INCLUSION_SUSPICIOUS;
  }


 private:
  void OnOffDomainInclusionEvent(AnalysisEvent event) {
    // Make sure no other AnalysisEvent was previously reported.
    EXPECT_EQ(AnalysisEvent::NO_EVENT, observed_analysis_event_);
    observed_analysis_event_ = event;
  }

  content::TestBrowserThreadBundle thread_bundle_;
  net::TestURLRequestContext context_;

  AnalysisEvent observed_analysis_event_;

  scoped_ptr<OffDomainInclusionDetector> off_domain_inclusion_detector_;
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

    EXPECT_EQ(AnalysisEvent::EMPTY_MAIN_FRAME_URL, GetLastEventAndReset());
  }
}

INSTANTIATE_TEST_CASE_P(
    OffDomainInclusionDetectorTestInstance,
    OffDomainInclusionDetectorTest,
    Values(OffDomainInclusionTestCases::OFF_DOMAIN_INCLUSION_WHITELISTED,
           OffDomainInclusionTestCases::OFF_DOMAIN_INCLUSION_UNKNOWN));

}  // namespace safe_browsing
