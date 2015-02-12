// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_receiver.h"
#include "chrome/browser/safe_browsing/incident_reporting/mock_incident_receiver.h"
#include "chrome/browser/safe_browsing/incident_reporting/script_request_detector.h"
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
const char kMatchingTestUrl[] = "http://example.com/foo/bar/baz.js";
const char kMatchingTestUrlParams[] =
    "http://example.com/foo/bar/baz.js#abc?foo=bar";
const char kNonMatchingTestUrl[] = "http://example.com/nonmatching/bar/baz.js";
}

namespace safe_browsing {

class FakeScriptRequestDetector : public ScriptRequestDetector {
 public:
  explicit FakeScriptRequestDetector(
      scoped_ptr<IncidentReceiver> incident_receiver)
      : ScriptRequestDetector(incident_receiver.Pass()) {
    FakeScriptRequestDetector::set_allow_null_profile_for_testing(true);
  }
};

class ScriptRequestDetectorTest : public testing::Test {
 protected:
  ScriptRequestDetectorTest()
      : mock_incident_receiver_(
            new StrictMock<safe_browsing::MockIncidentReceiver>()),
        fake_script_request_detector_(
            make_scoped_ptr(mock_incident_receiver_)) {}

  scoped_ptr<net::URLRequest> GetTestURLRequest(
      const std::string& url,
      content::ResourceType resource_type) const {
    scoped_ptr<net::URLRequest> url_request(
        context_.CreateRequest(GURL(url), net::DEFAULT_PRIORITY, NULL, NULL));

    content::ResourceRequestInfo::AllocateForTesting(
        url_request.get(), resource_type,
        NULL,              // resource_context
        0,                 // render_process_id
        0,                 // render_view_id
        MSG_ROUTING_NONE,  // render_frame_id
        true,              // is_main_frame
        false,             // parent_is_main_frame
        true,              // allow_download
        false);            // is_async

    return url_request.Pass();
  }

  StrictMock<safe_browsing::MockIncidentReceiver>* mock_incident_receiver_;
  FakeScriptRequestDetector fake_script_request_detector_;

 private:
  // UrlRequest requires a message loop. This provides one.
  content::TestBrowserThreadBundle thread_bundle_;
  net::TestURLRequestContext context_;
};

TEST_F(ScriptRequestDetectorTest, NoEventForIgnoredResourceTypes) {
  scoped_ptr<net::URLRequest> ignored_request(
      GetTestURLRequest(kNonMatchingTestUrl, content::RESOURCE_TYPE_IMAGE));

  fake_script_request_detector_.OnResourceRequest(ignored_request.get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(ScriptRequestDetectorTest, NoEventForNonMatchingScript) {
  scoped_ptr<net::URLRequest> ignored_request(
      GetTestURLRequest(kNonMatchingTestUrl, content::RESOURCE_TYPE_SCRIPT));

  fake_script_request_detector_.OnResourceRequest(ignored_request.get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(ScriptRequestDetectorTest, EventForBaseMatchingScript) {
  GURL url(kMatchingTestUrl);
  scoped_ptr<net::URLRequest> request(
      GetTestURLRequest(kMatchingTestUrl, content::RESOURCE_TYPE_SCRIPT));
  scoped_ptr<Incident> incident;
  EXPECT_CALL(*mock_incident_receiver_, DoAddIncidentForProfile(IsNull(), _))
      .WillOnce(WithArg<1>(TakeIncident(&incident)));

  fake_script_request_detector_.OnResourceRequest(request.get());
  base::RunLoop().RunUntilIdle();

  scoped_ptr<ClientIncidentReport_IncidentData> incident_data =
      incident->TakePayload();
  ASSERT_TRUE(incident_data->has_script_request());
  const ClientIncidentReport_IncidentData_ScriptRequestIncident&
      script_request_incident = incident_data->script_request();
  EXPECT_TRUE(script_request_incident.has_script_digest());
}

TEST_F(ScriptRequestDetectorTest, EventForMatchingScriptWithParams) {
  GURL url(kMatchingTestUrlParams);
  scoped_ptr<net::URLRequest> request(
      GetTestURLRequest(kMatchingTestUrlParams, content::RESOURCE_TYPE_SCRIPT));
  scoped_ptr<Incident> incident;
  EXPECT_CALL(*mock_incident_receiver_, DoAddIncidentForProfile(IsNull(), _))
      .WillOnce(WithArg<1>(TakeIncident(&incident)));

  fake_script_request_detector_.OnResourceRequest(request.get());
  base::RunLoop().RunUntilIdle();

  scoped_ptr<ClientIncidentReport_IncidentData> incident_data =
      incident->TakePayload();
  ASSERT_TRUE(incident_data->has_script_request());
  const ClientIncidentReport_IncidentData_ScriptRequestIncident&
      script_request_incident = incident_data->script_request();
  EXPECT_TRUE(script_request_incident.has_script_digest());
}

}  // namespace safe_browsing
