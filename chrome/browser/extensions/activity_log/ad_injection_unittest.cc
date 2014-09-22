// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/testing_pref_service.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/activity_log/activity_actions.h"
#include "components/rappor/byte_vector_utils.h"
#include "components/rappor/proto/rappor_metric.pb.h"
#include "components/rappor/rappor_service.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

scoped_refptr<Action> CreateAction(const std::string& api_name,
                                   const std::string& element,
                                   const std::string& attr) {
  scoped_refptr<Action> action = new Action("id",
                                            base::Time::Now(),
                                            Action::ACTION_DOM_ACCESS,
                                            api_name);
  action->set_args(ListBuilder()
                   .Append(element)
                   .Append(attr)
                   .Append("http://www.google.co.uk")
                   .Append("http://www.google.co.uk")
                   .Build());
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

 protected:
  TestingPrefServiceSimple prefs_;
};

TestRapporService::TestRapporService()
  : rappor::RapporService(&prefs_) {
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

// Test that the actions properly upload the URLs to the RAPPOR service if
// the action may have injected the ad.
TEST(AdInjectionUnittest, CheckActionForAdInjectionTest) {
  TestRapporService rappor_service;
  rappor::RapporReports reports = rappor_service.GetReports();
  EXPECT_EQ(0, reports.report_size());

  scoped_refptr<Action> modify_iframe_src =
      CreateAction("blinkSetAttribute", "iframe", "src");
  modify_iframe_src->DidInjectAd(&rappor_service);
  reports = rappor_service.GetReports();
  EXPECT_EQ(1, reports.report_size());

  scoped_refptr<Action> modify_anchor_href =
      CreateAction("blinkSetAttribute", "a", "href");
  modify_anchor_href->DidInjectAd(&rappor_service);
  reports = rappor_service.GetReports();
  EXPECT_EQ(1, reports.report_size());

  scoped_refptr<Action> harmless_action =
      CreateAction("Location.replace", "", "");
  harmless_action->DidInjectAd(&rappor_service);
  reports = rappor_service.GetReports();
  EXPECT_EQ(0, reports.report_size());
}

}  // namespace extensions
