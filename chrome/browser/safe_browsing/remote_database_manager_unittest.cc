// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/remote_database_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_api_handler.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestSafeBrowsingApiHandler : public SafeBrowsingApiHandler {
 public:
  void StartURLCheck(const URLCheckCallback& callback,
                     const GURL& url,
                     const std::vector<SBThreatType>& threat_types) override {}
};

class RemoteDatabaseManagerTest : public testing::Test {
 protected:
  RemoteDatabaseManagerTest() : field_trials_(new base::FieldTrialList(NULL)) {}

  void SetUp() override {
    SafeBrowsingApiHandler::SetInstance(&api_handler_);
    db_ = new RemoteSafeBrowsingDatabaseManager();
  }

  // Setup the two field trial params.  These are read in db_'s ctor.
  void SetFieldTrialParams(const std::string enabled_val,
                           const std::string types_to_check_val) {
    // Destroy the existing FieldTrialList before creating a new one to avoid
    // a DCHECK.
    field_trials_.reset();
    field_trials_.reset(new base::FieldTrialList(NULL));
    variations::testing::ClearAllVariationIDs();
    variations::testing::ClearAllVariationParams();

    const std::string group_name = "GroupFoo";  // Value not used
    const std::string experiment_name = "SafeBrowsingAndroid";
    ASSERT_TRUE(
        base::FieldTrialList::CreateFieldTrial(experiment_name, group_name));

    std::map<std::string, std::string> params;
    if (!enabled_val.empty())
      params["enabled"] = enabled_val;
    if (!types_to_check_val.empty())
      params["types_to_check"] = types_to_check_val;

    ASSERT_TRUE(variations::AssociateVariationParams(experiment_name,
                                                     group_name, params));
  }

  scoped_ptr<base::FieldTrialList> field_trials_;
  TestSafeBrowsingApiHandler api_handler_;
  scoped_refptr<RemoteSafeBrowsingDatabaseManager> db_;
};

TEST_F(RemoteDatabaseManagerTest, DisabledViaNullOrTrial) {
  EXPECT_FALSE(db_->IsSupported());

  SetFieldTrialParams("true", "");
  db_ = new RemoteSafeBrowsingDatabaseManager();
  EXPECT_TRUE(db_->IsSupported());

  SafeBrowsingApiHandler::SetInstance(nullptr);
  EXPECT_FALSE(db_->IsSupported());
}

TEST_F(RemoteDatabaseManagerTest, TypesToCheckDefault) {
  EXPECT_TRUE(db_->CanCheckResourceType(content::RESOURCE_TYPE_MAIN_FRAME));
  EXPECT_TRUE(db_->CanCheckResourceType(content::RESOURCE_TYPE_SUB_FRAME));
  // The rest are false.
  for (int t = content::RESOURCE_TYPE_SUB_FRAME + 1;
       t < content::RESOURCE_TYPE_LAST_TYPE; t++) {
    EXPECT_FALSE(
        db_->CanCheckResourceType(static_cast<content::ResourceType>(t)));
  }
}
TEST_F(RemoteDatabaseManagerTest, TypesToCheckFromTrial) {
  SetFieldTrialParams("true", "1,2,blah, 9");
  db_ = new RemoteSafeBrowsingDatabaseManager();
  EXPECT_TRUE(db_->CanCheckResourceType(
      content::RESOURCE_TYPE_MAIN_FRAME));  // defaulted
  EXPECT_TRUE(db_->CanCheckResourceType(content::RESOURCE_TYPE_SUB_FRAME));
  EXPECT_TRUE(db_->CanCheckResourceType(content::RESOURCE_TYPE_STYLESHEET));
  EXPECT_FALSE(db_->CanCheckResourceType(content::RESOURCE_TYPE_SCRIPT));
  EXPECT_FALSE(db_->CanCheckResourceType(content::RESOURCE_TYPE_IMAGE));
  // ...
  EXPECT_FALSE(db_->CanCheckResourceType(content::RESOURCE_TYPE_MEDIA));
  EXPECT_TRUE(db_->CanCheckResourceType(content::RESOURCE_TYPE_WORKER));
}

}  // namespace
