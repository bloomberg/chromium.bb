// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_receiver.h"
#include "chrome/browser/safe_browsing/incident_reporting/mock_incident_receiver.h"
#include "chrome/browser/safe_browsing/incident_reporting/resource_request_detector.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "crypto/sha2.h"
#include "ipc/ipc_message.h"
#include "net/base/request_priority.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::IsNull;
using ::testing::StrictMock;
using ::testing::WithArg;
using ::testing::_;

namespace {
const char kScriptMatchingTestUrl[] = "http://example.com/foo/bar/baz.js";
const char kScriptMatchingTestUrlParams[] =
    "http://example.com/foo/bar/baz.js#abc?foo=bar";
const char kScriptNonMatchingTestUrl[] =
    "http://example.com/nonmatching/bar/baz.js";

const char kDomainMatchingTestUrl[] =
    "http://example892904760932459706783457.com/some/request";
const char kDomainMatchingTestUrlScript[] =
    "http://example892904760932459706783457.com/path/to/script.js";
const char kDomainNonMatchingTestUrl[] =
    "http://example892904760932459706783458.com/some/request";
}  // namespace

namespace safe_browsing {

class FakeResourceRequestDetector : public ResourceRequestDetector {
 public:
  explicit FakeResourceRequestDetector(
      scoped_ptr<IncidentReceiver> incident_receiver)
      : ResourceRequestDetector(incident_receiver.Pass()) {
    FakeResourceRequestDetector::set_allow_null_profile_for_testing(true);
  }
};

class ResourceRequestDetectorTest : public testing::Test {
 protected:
  ResourceRequestDetectorTest()
      : mock_incident_receiver_(
            new StrictMock<safe_browsing::MockIncidentReceiver>()),
        fake_resource_request_detector_(
            make_scoped_ptr(mock_incident_receiver_)) {}

  scoped_ptr<net::URLRequest> GetTestURLRequest(
      const std::string& url,
      content::ResourceType resource_type) const {
    scoped_ptr<net::URLRequest> url_request(
        context_.CreateRequest(GURL(url), net::DEFAULT_PRIORITY, NULL));

    content::ResourceRequestInfo::AllocateForTesting(
        url_request.get(), resource_type,
        NULL,              // resource_context
        0,                 // render_process_id
        0,                 // render_view_id
        MSG_ROUTING_NONE,  // render_frame_id
        true,              // is_main_frame
        false,             // parent_is_main_frame
        true,              // allow_download
        false,             // is_async
        false);            // is_using_lofi

    return url_request.Pass();
  }

  void ExpectNoIncident(const std::string& url,
                        content::ResourceType resource_type) {
    scoped_ptr<net::URLRequest> request(GetTestURLRequest(url, resource_type));

    EXPECT_CALL(*mock_incident_receiver_, DoAddIncidentForProfile(IsNull(), _))
        .Times(0);

    fake_resource_request_detector_.OnResourceRequest(request.get());
    base::RunLoop().RunUntilIdle();
  }

  void ExpectIncidentAdded(
      const std::string& url,
      content::ResourceType resource_type,
      ClientIncidentReport_IncidentData_ResourceRequestIncident_Type
          expected_type) {
    scoped_ptr<net::URLRequest> request(GetTestURLRequest(url, resource_type));
    scoped_ptr<Incident> incident;
    EXPECT_CALL(*mock_incident_receiver_, DoAddIncidentForProfile(IsNull(), _))
        .WillOnce(WithArg<1>(TakeIncident(&incident)));

    fake_resource_request_detector_.OnResourceRequest(request.get());
    base::RunLoop().RunUntilIdle();

    scoped_ptr<ClientIncidentReport_IncidentData> incident_data =
        incident->TakePayload();
    ASSERT_TRUE(incident_data->has_resource_request());
    const ClientIncidentReport_IncidentData_ResourceRequestIncident&
        script_request_incident = incident_data->resource_request();
    EXPECT_TRUE(script_request_incident.has_digest());
    EXPECT_TRUE(script_request_incident.type() == expected_type);
  }

  StrictMock<safe_browsing::MockIncidentReceiver>* mock_incident_receiver_;
  FakeResourceRequestDetector fake_resource_request_detector_;

 private:
  // UrlRequest requires a message loop. This provides one.
  content::TestBrowserThreadBundle thread_bundle_;
  net::TestURLRequestContext context_;
};

// Script request tests

TEST_F(ResourceRequestDetectorTest, NoEventForIgnoredResourceTypes) {
  ExpectNoIncident(kScriptNonMatchingTestUrl, content::RESOURCE_TYPE_IMAGE);
}

TEST_F(ResourceRequestDetectorTest, NoEventForNonMatchingScript) {
  ExpectNoIncident(kScriptNonMatchingTestUrl, content::RESOURCE_TYPE_SCRIPT);
}

TEST_F(ResourceRequestDetectorTest, EventForBaseMatchingScript) {
  ExpectIncidentAdded(
      kScriptMatchingTestUrl, content::RESOURCE_TYPE_SCRIPT,
      ClientIncidentReport_IncidentData_ResourceRequestIncident::TYPE_SCRIPT);
}

TEST_F(ResourceRequestDetectorTest, EventForMatchingScriptWithParams) {
  ExpectIncidentAdded(
      kScriptMatchingTestUrlParams, content::RESOURCE_TYPE_SCRIPT,
      ClientIncidentReport_IncidentData_ResourceRequestIncident::TYPE_SCRIPT);
}

// Domain request tests

TEST_F(ResourceRequestDetectorTest, NoEventForNonMatchingDomainSubFrame) {
  ExpectNoIncident(kDomainNonMatchingTestUrl, content::RESOURCE_TYPE_SUB_FRAME);
}

TEST_F(ResourceRequestDetectorTest, NoEventForMatchingDomainTopLevel) {
  ExpectNoIncident(kDomainMatchingTestUrl, content::RESOURCE_TYPE_MAIN_FRAME);
}

TEST_F(ResourceRequestDetectorTest, EventForMatchingDomainSubFrame) {
  ExpectIncidentAdded(
      kDomainMatchingTestUrl, content::RESOURCE_TYPE_SUB_FRAME,
      ClientIncidentReport_IncidentData_ResourceRequestIncident::TYPE_DOMAIN);
}

TEST_F(ResourceRequestDetectorTest, EventForMatchingDomainScript) {
  ExpectIncidentAdded(
      kDomainMatchingTestUrlScript, content::RESOURCE_TYPE_SCRIPT,
      ClientIncidentReport_IncidentData_ResourceRequestIncident::TYPE_DOMAIN);
}

}  // namespace safe_browsing
