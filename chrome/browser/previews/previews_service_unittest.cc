// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_service.h"

#include <initializer_list>
#include <map>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "components/previews/core/previews_io_data.h"
#include "components/previews/core/previews_ui_service.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void CreateFieldTrialWithParams(
    const std::string& trial_name,
    const std::string& group_name,
    std::initializer_list<
        typename std::map<std::string, std::string>::value_type> params) {
  EXPECT_TRUE(base::AssociateFieldTrialParams(trial_name, group_name, params));
  EXPECT_TRUE(base::FieldTrialList::CreateFieldTrial(trial_name, group_name));
}

// This test class intercepts the |is_enabled_callback| and is used to test its
// validity.
class TestPreviewsIOData : public previews::PreviewsIOData {
 public:
  TestPreviewsIOData()
      : previews::PreviewsIOData(content::BrowserThread::GetTaskRunnerForThread(
                                     content::BrowserThread::UI),
                                 content::BrowserThread::GetTaskRunnerForThread(
                                     content::BrowserThread::UI)) {}
  ~TestPreviewsIOData() override {}

  void Initialize(
      base::WeakPtr<previews::PreviewsUIService> previews_ui_service,
      std::unique_ptr<previews::PreviewsOptOutStore> previews_opt_out_store,
      const previews::PreviewsIsEnabledCallback& is_enabled_callback) override {
    enabled_callback_ = is_enabled_callback;
  }

  bool IsPreviewEnabled(previews::PreviewsType type) {
    return enabled_callback_.Run(type);
  }

 private:
  previews::PreviewsIsEnabledCallback enabled_callback_;
};

// Class to test the validity of the callback passed to PreviewsIOData from
// PreviewsService.
class PreviewsServiceTest : public testing::Test {
 public:
  PreviewsServiceTest()

      : field_trial_list_(nullptr) {}

  void SetUp() override {
    io_data_ = base::MakeUnique<TestPreviewsIOData>();

    service_ = base::MakeUnique<PreviewsService>();
    base::FilePath file_path;
    service_->Initialize(io_data_.get(),
                         content::BrowserThread::GetTaskRunnerForThread(
                             content::BrowserThread::UI),
                         file_path);
  }

  void TearDown() override { variations::testing::ClearAllVariationParams(); }

  ~PreviewsServiceTest() override {}

  TestPreviewsIOData* io_data() const { return io_data_.get(); }

 private:
  content::TestBrowserThreadBundle threads_;
  base::FieldTrialList field_trial_list_;
  std::unique_ptr<TestPreviewsIOData> io_data_;
  std::unique_ptr<PreviewsService> service_;
};

}  // namespace

TEST_F(PreviewsServiceTest, TestOfflineFieldTrialEnabled) {
  CreateFieldTrialWithParams("ClientSidePreviews", "Enabled",
                             {{"show_offline_pages", "true"}});
  EXPECT_TRUE(io_data()->IsPreviewEnabled(previews::PreviewsType::OFFLINE));
}

TEST_F(PreviewsServiceTest, TestOfflineFieldTrialDisabled) {
  CreateFieldTrialWithParams("ClientSidePreviews", "Disabled",
                             {{"show_offline_pages", "true"}});
  EXPECT_FALSE(io_data()->IsPreviewEnabled(previews::PreviewsType::OFFLINE));
}

TEST_F(PreviewsServiceTest, TestOfflineFieldTrialEnabledNotShowingOffline) {
  CreateFieldTrialWithParams("ClientSidePreviews", "Disabled",
                             {{"show_offline_pages", "false"}});
  EXPECT_FALSE(io_data()->IsPreviewEnabled(previews::PreviewsType::OFFLINE));
}

TEST_F(PreviewsServiceTest, TestOfflineFieldTrialNotSet) {
  EXPECT_FALSE(io_data()->IsPreviewEnabled(previews::PreviewsType::OFFLINE));
}

TEST_F(PreviewsServiceTest, TestClientLoFiFieldTrialEnabled) {
  base::FieldTrialList::CreateFieldTrial("PreviewsClientLoFi", "Enabled");
  EXPECT_TRUE(io_data()->IsPreviewEnabled(previews::PreviewsType::LOFI));
}

TEST_F(PreviewsServiceTest, TestClientLoFiFieldTrialDisabled) {
  base::FieldTrialList::CreateFieldTrial("PreviewsClientLoFi", "Disabled");
  EXPECT_FALSE(io_data()->IsPreviewEnabled(previews::PreviewsType::LOFI));
}

TEST_F(PreviewsServiceTest, TestClientLoFiFieldTrialNotSet) {
  EXPECT_FALSE(io_data()->IsPreviewEnabled(previews::PreviewsType::LOFI));
}

TEST_F(PreviewsServiceTest, TestServerLoFiFieldTrialEnabled) {
  base::FieldTrialList::CreateFieldTrial("DataCompressionProxyLoFi", "Enabled");
  EXPECT_TRUE(io_data()->IsPreviewEnabled(previews::PreviewsType::LOFI));
}

TEST_F(PreviewsServiceTest, TestServerLoFiFieldTrialDisabled) {
  base::FieldTrialList::CreateFieldTrial("DataCompressionProxyLoFi",
                                         "Disabled");
  EXPECT_FALSE(io_data()->IsPreviewEnabled(previews::PreviewsType::LOFI));
}

TEST_F(PreviewsServiceTest, TestServerLoFiFieldTrialNotSet) {
  EXPECT_FALSE(io_data()->IsPreviewEnabled(previews::PreviewsType::LOFI));
}

TEST_F(PreviewsServiceTest, TestLitePageFieldTrialEnabledPreview) {
  base::FieldTrialList::CreateFieldTrial("DataCompressionProxyLoFi",
                                         "Enabled_Preview");
  EXPECT_TRUE(io_data()->IsPreviewEnabled(previews::PreviewsType::LITE_PAGE));
}

TEST_F(PreviewsServiceTest, TestLitePageFieldTrialEnabled) {
  base::FieldTrialList::CreateFieldTrial("DataCompressionProxyLoFi", "Enabled");
  EXPECT_FALSE(io_data()->IsPreviewEnabled(previews::PreviewsType::LITE_PAGE));
}

TEST_F(PreviewsServiceTest, TestLitePageFieldTrialDisabled) {
  base::FieldTrialList::CreateFieldTrial("DataCompressionProxyLoFi",
                                         "Disabled");
  EXPECT_FALSE(io_data()->IsPreviewEnabled(previews::PreviewsType::LITE_PAGE));
}

TEST_F(PreviewsServiceTest, TestLitePageFieldTrialNotSet) {
  EXPECT_FALSE(io_data()->IsPreviewEnabled(previews::PreviewsType::LITE_PAGE));
}
