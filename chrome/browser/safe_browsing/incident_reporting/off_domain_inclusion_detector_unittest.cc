// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "chrome/browser/safe_browsing/incident_reporting/off_domain_inclusion_detector.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "ipc/ipc_message.h"
#include "net/base/request_priority.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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

class OffDomainInclusionDetectorTest : public testing::Test {
 public:
  OffDomainInclusionDetectorTest()
      : off_domain_inclusion_detector_(
            base::Bind(
                &OffDomainInclusionDetectorTest::OnOffDomainInclusionEvent,
                base::Unretained(this))),
        observed_analysis_event_(AnalysisEvent::NO_EVENT) {}

 protected:
  AnalysisEvent GetLastEventAndReset() {
    const AnalysisEvent last_event = observed_analysis_event_;
    observed_analysis_event_ = AnalysisEvent::NO_EVENT;
    return last_event;
  }

  scoped_ptr<net::URLRequest> GetTestURLRequest(
      const std::string& url,
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

    return url_request.Pass();
  }

  OffDomainInclusionDetector off_domain_inclusion_detector_;

 private:
  void OnOffDomainInclusionEvent(AnalysisEvent event) {
    // Make sure no other AnalysisEvent was previously reported.
    EXPECT_EQ(AnalysisEvent::NO_EVENT, observed_analysis_event_);
    observed_analysis_event_ = event;
  }

  content::TestBrowserThreadBundle thread_bundle_;
  net::TestURLRequestContext context_;

  AnalysisEvent observed_analysis_event_;
};

TEST_F(OffDomainInclusionDetectorTest, NoEventForIgnoredResourceTypes) {
  for (content::ResourceType tested_type : kResourceTypesIgnored) {
    SCOPED_TRACE(tested_type);

    scoped_ptr<net::URLRequest> request_for_tested_type(
        GetTestURLRequest("http://offdomain.com/foo",
                          "http://mydomain.com/bar",
                          tested_type,
                          true,     // is_main_frame
                          false));  // parent_is_main_frame

    off_domain_inclusion_detector_.OnResourceRequest(
        request_for_tested_type.get());

    EXPECT_EQ(AnalysisEvent::NO_EVENT, GetLastEventAndReset());
  }
}

TEST_F(OffDomainInclusionDetectorTest, NoEventForSameDomainInclusions) {
  for (content::ResourceType tested_type :
       kResourceTypesObservedIfInMainFrame) {
    SCOPED_TRACE(tested_type);

    scoped_ptr<net::URLRequest> request_for_tested_type(
        GetTestURLRequest("http://mydomain.com/foo",
                          "http://mydomain.com/bar",
                          tested_type,
                          true,     // is_main_frame
                          false));  // parent_is_main_frame

    off_domain_inclusion_detector_.OnResourceRequest(
        request_for_tested_type.get());

    EXPECT_EQ(AnalysisEvent::NO_EVENT, GetLastEventAndReset());
  }
}

TEST_F(OffDomainInclusionDetectorTest, OffDomainInclusionInMainFrame) {
  for (content::ResourceType tested_type :
       kResourceTypesObservedIfInMainFrame) {
    SCOPED_TRACE(tested_type);

    scoped_ptr<net::URLRequest> request_for_tested_type(
        GetTestURLRequest("http://offdomain.com/foo",
                          "http://mydomain.com/bar",
                          tested_type,
                          true,     // is_main_frame
                          false));  // parent_is_main_frame

    off_domain_inclusion_detector_.OnResourceRequest(
        request_for_tested_type.get());

    EXPECT_EQ(AnalysisEvent::OFF_DOMAIN_INCLUSION_DETECTED,
              GetLastEventAndReset());
  }
}

TEST_F(OffDomainInclusionDetectorTest, HttpsOffDomainInclusionInMainFrame) {
  for (content::ResourceType tested_type :
       kResourceTypesObservedIfInMainFrame) {
    SCOPED_TRACE(tested_type);

    scoped_ptr<net::URLRequest> request_for_tested_type(
        GetTestURLRequest("https://offdomain.com/foo",
                          "https://mydomain.com/bar",
                          tested_type,
                          true,     // is_main_frame
                          false));  // parent_is_main_frame

    off_domain_inclusion_detector_.OnResourceRequest(
        request_for_tested_type.get());

    EXPECT_EQ(AnalysisEvent::OFF_DOMAIN_INCLUSION_DETECTED,
              GetLastEventAndReset());
  }
}

TEST_F(OffDomainInclusionDetectorTest,
       NoEventForNonHttpOffDomainInclusionInMainFrame) {
  for (content::ResourceType tested_type :
       kResourceTypesObservedIfInMainFrame) {
    SCOPED_TRACE(tested_type);

    scoped_ptr<net::URLRequest> request_for_tested_type(
        GetTestURLRequest("ftp://offdomain.com/foo",
                          "http://mydomain.com/bar",
                          tested_type,
                          true,     // is_main_frame
                          false));  // parent_is_main_frame

    off_domain_inclusion_detector_.OnResourceRequest(
        request_for_tested_type.get());

    EXPECT_EQ(AnalysisEvent::NO_EVENT, GetLastEventAndReset());
  }
}

TEST_F(OffDomainInclusionDetectorTest, NoEventForSameTopLevelDomain) {
  for (content::ResourceType tested_type :
       kResourceTypesObservedIfInMainFrame) {
    SCOPED_TRACE(tested_type);

    scoped_ptr<net::URLRequest> request_for_tested_type(
        GetTestURLRequest("http://a.mydomain.com/foo",
                          "http://b.mydomain.com/bar",
                          tested_type,
                          true,     // is_main_frame
                          false));  // parent_is_main_frame

    off_domain_inclusion_detector_.OnResourceRequest(
        request_for_tested_type.get());

    EXPECT_EQ(AnalysisEvent::NO_EVENT, GetLastEventAndReset());
  }
}

TEST_F(OffDomainInclusionDetectorTest,
       OffDomainInclusionForSameTopLevelRegistryButDifferentDomain) {
  for (content::ResourceType tested_type :
       kResourceTypesObservedIfInMainFrame) {
    SCOPED_TRACE(tested_type);

    scoped_ptr<net::URLRequest> request_for_tested_type(
        GetTestURLRequest("http://a.appspot.com/foo",
                          "http://b.appspot.com/bar",
                          tested_type,
                          true,     // is_main_frame
                          false));  // parent_is_main_frame

    off_domain_inclusion_detector_.OnResourceRequest(
        request_for_tested_type.get());

    EXPECT_EQ(AnalysisEvent::OFF_DOMAIN_INCLUSION_DETECTED,
              GetLastEventAndReset());
  }
}

TEST_F(OffDomainInclusionDetectorTest,
       NoEventForOffDomainRegularResourceInSubframe) {
  for (content::ResourceType tested_type :
       kResourceTypesObservedIfInMainFrame) {
    SCOPED_TRACE(tested_type);

    scoped_ptr<net::URLRequest> request_for_tested_type(
        GetTestURLRequest("http://offdomain.com/foo",
                          "http://mydomain.com/bar",
                          tested_type,
                          false,   // is_main_frame
                          true));  // parent_is_main_frame

    off_domain_inclusion_detector_.OnResourceRequest(
        request_for_tested_type.get());

    EXPECT_EQ(AnalysisEvent::NO_EVENT, GetLastEventAndReset());
  }
}

TEST_F(OffDomainInclusionDetectorTest,
       NoEventForOffDomainSubSubFrameInclusion) {
  for (content::ResourceType tested_type :
       kResourceTypesObservedIfParentIsMainFrame) {
    SCOPED_TRACE(tested_type);

    scoped_ptr<net::URLRequest> request_for_tested_type(
        GetTestURLRequest("http://offdomain.com/foo",
                          "http://mydomain.com/bar",
                          tested_type,
                          false,    // is_main_frame
                          false));  // parent_is_main_frame

    off_domain_inclusion_detector_.OnResourceRequest(
        request_for_tested_type.get());

    EXPECT_EQ(AnalysisEvent::NO_EVENT, GetLastEventAndReset());
  }
}

TEST_F(OffDomainInclusionDetectorTest,
       OffDomainInclusionForOffDomainResourcesObservedIfParentIsMainFrame) {
  for (content::ResourceType tested_type :
       kResourceTypesObservedIfParentIsMainFrame) {
    SCOPED_TRACE(tested_type);

    scoped_ptr<net::URLRequest> request_for_tested_type(
        GetTestURLRequest("http://offdomain.com/foo",
                          "http://mydomain.com/bar",
                          tested_type,
                          false,   // is_main_frame
                          true));  // parent_is_main_frame

    off_domain_inclusion_detector_.OnResourceRequest(
        request_for_tested_type.get());

    EXPECT_EQ(AnalysisEvent::OFF_DOMAIN_INCLUSION_DETECTED,
              GetLastEventAndReset());
  }
}

TEST_F(OffDomainInclusionDetectorTest,
       EmptyEventForOffDomainInclusionWithNoReferrer) {
  for (content::ResourceType tested_type :
       kResourceTypesObservedIfInMainFrame) {
    SCOPED_TRACE(tested_type);

    scoped_ptr<net::URLRequest> request_for_tested_type(
        GetTestURLRequest("https://offdomain.com/foo",
                          "",
                          tested_type,
                          true,     // is_main_frame
                          false));  // parent_is_main_frame

    off_domain_inclusion_detector_.OnResourceRequest(
        request_for_tested_type.get());

    EXPECT_EQ(AnalysisEvent::EMPTY_MAIN_FRAME_URL, GetLastEventAndReset());
  }
}

}  // namespace safe_browsing
