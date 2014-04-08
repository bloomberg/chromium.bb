// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "chrome/browser/extensions/activity_log/activity_actions.h"
#include "chrome/browser/extensions/activity_log/ad_injection_util.h"
#include "components/rappor/byte_vector_utils.h"
#include "components/rappor/proto/rappor_metric.pb.h"
#include "components/rappor/rappor_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

scoped_refptr<Action> CreateAction(const std::string& api_name) {
  scoped_refptr<Action> action = new Action("id",
                                            base::Time::Now(),
                                            Action::ACTION_DOM_ACCESS,
                                            api_name);
  action->set_arg_url(GURL("http://www.notarealhost.notarealtld"));
  return action;
}

}  // namespace

class TestRapporService : public rappor::RapporService {
 public:
  TestRapporService();
  virtual ~TestRapporService();

  // Returns the active reports. This also clears the internal map of metrics
  // as a biproduct, so if comparing numbers of reports, the comparison should
  // be from the last time GetReports() was called (not from the beginning of
  // the test).
  rappor::RapporReports GetReports();
};

TestRapporService::TestRapporService() {
  // Initialize the RapporService for testing.
  SetCohortForTesting(0);
  SetSecretForTesting(rappor::HmacByteVectorGenerator::GenerateEntropyInput());
}

TestRapporService::~TestRapporService() {}

rappor::RapporReports TestRapporService::GetReports() {
  rappor::RapporReports result;
  rappor::RapporService::ExportMetrics(&result);
  return result;
}

TEST(AdInjectionUtilUnittest, CheckActionForAdInjectionTest) {
  TestRapporService rappor_service;
  rappor::RapporReports reports = rappor_service.GetReports();
  EXPECT_EQ(0, reports.report_size());

  scoped_refptr<Action> modify_iframe_src =
      CreateAction("HTMLIFrameElement.src");
  ad_injection_util::CheckActionForAdInjection(modify_iframe_src,
                                               &rappor_service);
  reports = rappor_service.GetReports();
  EXPECT_EQ(1, reports.report_size());

  scoped_refptr<Action> modify_embed_src =
      CreateAction("HTMLEmbedElement.src");
  ad_injection_util::CheckActionForAdInjection(modify_embed_src,
                                               &rappor_service);
  reports = rappor_service.GetReports();
  EXPECT_EQ(1, reports.report_size());

  scoped_refptr<Action> modify_anchor_href =
      CreateAction("HTMLAnchorElement.href");
  ad_injection_util::CheckActionForAdInjection(modify_anchor_href,
                                               &rappor_service);
  reports = rappor_service.GetReports();
  EXPECT_EQ(1, reports.report_size());

  scoped_refptr<Action> harmless_action = CreateAction("Location.replace");
  ad_injection_util::CheckActionForAdInjection(harmless_action,
                                               &rappor_service);
  reports = rappor_service.GetReports();
  EXPECT_EQ(0, reports.report_size());
}

}  // namespace extensions
