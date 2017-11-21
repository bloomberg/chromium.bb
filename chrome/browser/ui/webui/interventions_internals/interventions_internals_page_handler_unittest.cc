// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/interventions_internals/interventions_internals_page_handler.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
#include "chrome/browser/flag_descriptions.h"
#include "chrome/browser/net/nqe/ui_network_quality_estimator_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/interventions_internals/interventions_internals.mojom.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/previews/content/previews_io_data.h"
#include "components/previews/content/previews_ui_service.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_logger.h"
#include "components/previews/core/previews_logger_observer.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/nqe/effective_connection_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// The keys for the mapping for the enabled/disabled status map.
constexpr char kAMPRedirectionPreviews[] = "ampPreviews";
constexpr char kClientLoFiPreviews[] = "clientLoFiPreviews";
constexpr char kNoScriptPreviews[] = "noScriptPreviews";
constexpr char kOfflinePreviews[] = "offlinePreviews";

// Descriptions for previews.
constexpr char kAmpRedirectionDescription[] = "AMP Previews";
constexpr char kClientLoFiDescription[] = "Client LoFi Previews";
constexpr char kNoScriptDescription[] = "NoScript Previews";
constexpr char kOfflineDesciption[] = "Offline Previews";

// Keys for flags mapping.
constexpr char kEctFlag[] = "ectFlag";
constexpr char kNoScriptFlag[] = "noScriptFlag";
constexpr char kOfflinePageFlag[] = "offlinePageFlag";

// Links to flags in chrome://flags.
constexpr char kNoScriptFlagLink[] = "chrome://flags/#enable-noscript-previews";
constexpr char kEctFlagLink[] =
    "chrome://flags/#force-effective-connection-type";
constexpr char kOfflinePageFlagLink[] =
    "chrome://flags/#enable-offline-previews";

// Flag features names.
constexpr char kNoScriptFeatureName[] = "NoScriptPreviews";
constexpr char kOfflinePageFeatureName[] = "OfflinePreviews";

constexpr char kDefaultFlagValue[] = "Not Forced";

// The map that would be passed to the callback in GetPreviewsEnabledCallback.
std::unordered_map<std::string, mojom::PreviewsStatusPtr> passed_in_modes;

// The map that would be passed to the callback in
// GetPreviewsFlagsDetailsCallback.
std::unordered_map<std::string, mojom::PreviewsFlagPtr> passed_in_flags;

// Mocked call back method to test GetPreviewsEnabledCallback.
void MockGetPreviewsEnabledCallback(
    std::unordered_map<std::string, mojom::PreviewsStatusPtr> params) {
  passed_in_modes = std::move(params);
}

// Mocked call back method to test GetPreviewsFlagsDetailsCallback.
void MockGetPreviewsFlagsCallback(
    std::unordered_map<std::string, mojom::PreviewsFlagPtr> params) {
  passed_in_flags = std::move(params);
}

// Dummy method for creating TestPreviewsUIService.
bool MockedPreviewsIsEnabled(previews::PreviewsType type) {
  return true;
}

// Mock class that would be pass into the InterventionsInternalsPageHandler by
// calling its SetClientPage method. Used to test that the PageHandler
// actually invokes the page's LogNewMessage method with the correct message.
class TestInterventionsInternalsPage
    : public mojom::InterventionsInternalsPage {
 public:
  TestInterventionsInternalsPage(
      mojom::InterventionsInternalsPageRequest request)
      : binding_(this, std::move(request)), blacklist_ignored_(false) {}

  ~TestInterventionsInternalsPage() override {}

  // mojom::InterventionsInternalsPage:
  void LogNewMessage(mojom::MessageLogPtr message) override {
    message_ = base::MakeUnique<mojom::MessageLogPtr>(std::move(message));
  }
  void OnBlacklistedHost(const std::string& host, int64_t time) override {
    host_blacklisted_ = host;
    host_blacklisted_time_ = time;
  }
  void OnUserBlacklistedStatusChange(bool blacklisted) override {
    user_blacklisted_ = blacklisted;
  }
  void OnBlacklistCleared(int64_t time) override {
    blacklist_cleared_time_ = time;
  }
  void OnEffectiveConnectionTypeChanged(const std::string& type) override {
    // Ignore.
    // TODO(thanhdle): Add integration test to test behavior of the pipeline end
    // to end. crbug.com/777936
  }
  void OnIgnoreBlacklistDecisionStatusChanged(bool ignored) override {
    blacklist_ignored_ = ignored;
  }

  // Expose passed in message in LogNewMessage for testing.
  mojom::MessageLogPtr* message() const { return message_.get(); }

  // Expose passed in blacklist events info for testing.
  std::string host_blacklisted() const { return host_blacklisted_; }
  int64_t host_blacklisted_time() const { return host_blacklisted_time_; }
  bool user_blacklisted() const { return user_blacklisted_; }
  int64_t blacklist_cleared_time() const { return blacklist_cleared_time_; }

  // Expose the passed in blacklist ignore status for testing.
  bool blacklist_ignored() const { return blacklist_ignored_; }

 private:
  mojo::Binding<mojom::InterventionsInternalsPage> binding_;

  // The MessageLogPtr passed in LogNewMessage method.
  std::unique_ptr<mojom::MessageLogPtr> message_;

  // Received blacklist events info.
  std::string host_blacklisted_;
  int64_t host_blacklisted_time_;
  int64_t user_blacklisted_;
  int64_t blacklist_cleared_time_;

  // Whether to ignore previews blacklist decisions.
  bool blacklist_ignored_;
};

// Mock class to test interaction between the PageHandler and the
// PreviewsLogger.
class TestPreviewsLogger : public previews::PreviewsLogger {
 public:
  TestPreviewsLogger() : PreviewsLogger(), remove_is_called_(false) {}

  // PreviewsLogger:
  void RemoveObserver(previews::PreviewsLoggerObserver* obs) override {
    remove_is_called_ = true;
  }

  bool RemovedObserverIsCalled() const { return remove_is_called_; }

 private:
  bool remove_is_called_;
};

// Mock class to test interaction between PageHandler and the
// UINetworkQualityEstimatorService.
class TestUINetworkQualityEstimatorService
    : public UINetworkQualityEstimatorService {
 public:
  explicit TestUINetworkQualityEstimatorService(Profile* profile)
      : UINetworkQualityEstimatorService(profile), remove_is_called_(false) {}

  // UINetworkQualityEstimatorService:
  void RemoveEffectiveConnectionTypeObserver(
      net::EffectiveConnectionTypeObserver* observer) override {
    remove_is_called_ = true;
  }

  bool RemovedObserverIsCalled() const { return remove_is_called_; }

 private:
  // Check if the observer removed itself from the observer list.
  bool remove_is_called_;
};

// A dummy class to setup PreviewsUIService.
class TestPreviewsIOData : public previews::PreviewsIOData {
 public:
  TestPreviewsIOData() : PreviewsIOData(nullptr, nullptr) {}

  // previews::PreviewsIOData:
  void Initialize(
      base::WeakPtr<previews::PreviewsUIService> previews_ui_service,
      std::unique_ptr<previews::PreviewsOptOutStore> opt_out_store,
      std::unique_ptr<previews::PreviewsOptimizationGuide> previews_opt_guide,
      const previews::PreviewsIsEnabledCallback& is_enabled_callback) override {
    // Do nothing.
  }
};

// Mocked TestPreviewsService for testing InterventionsInternalsPageHandler.
class TestPreviewsUIService : public previews::PreviewsUIService {
 public:
  TestPreviewsUIService(TestPreviewsIOData* io_data,
                        std::unique_ptr<previews::PreviewsLogger> logger)
      : PreviewsUIService(io_data,
                          nullptr, /* io_task_runner */
                          nullptr, /* previews_opt_out_store */
                          nullptr, /* previews_opt_guide */
                          base::Bind(&MockedPreviewsIsEnabled),
                          std::move(logger)),
        blacklist_ignored_(false) {}
  ~TestPreviewsUIService() override {}

  // previews::PreviewsUIService:
  void SetIgnorePreviewsBlacklistDecision(bool ignored) override {
    blacklist_ignored_ = ignored;
  }

  // Exposed blacklist ignored state.
  bool blacklist_ignored() const { return blacklist_ignored_; }

 private:
  // Whether the blacklist decisions are ignored or not.
  bool blacklist_ignored_;
};

class InterventionsInternalsPageHandlerTest : public testing::Test {
 public:
  InterventionsInternalsPageHandlerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  ~InterventionsInternalsPageHandlerTest() override {}

  void SetUp() override {
    TestPreviewsIOData io_data;
    std::unique_ptr<TestPreviewsLogger> logger =
        base::MakeUnique<TestPreviewsLogger>();
    logger_ = logger.get();
    previews_ui_service_ =
        base::MakeUnique<TestPreviewsUIService>(&io_data, std::move(logger));

    ASSERT_TRUE(profile_manager_.SetUp());
    TestingProfile* test_profile =
        profile_manager_.CreateTestingProfile(chrome::kInitialProfile);
    ui_nqe_service_ =
        base::MakeUnique<TestUINetworkQualityEstimatorService>(test_profile);

    mojom::InterventionsInternalsPageHandlerPtr page_handler_ptr;
    handler_request_ = mojo::MakeRequest(&page_handler_ptr);
    page_handler_ = base::MakeUnique<InterventionsInternalsPageHandler>(
        std::move(handler_request_), previews_ui_service_.get(),
        ui_nqe_service_.get());

    mojom::InterventionsInternalsPagePtr page_ptr;
    page_request_ = mojo::MakeRequest(&page_ptr);
    page_ = base::MakeUnique<TestInterventionsInternalsPage>(
        std::move(page_request_));

    page_handler_->SetClientPage(std::move(page_ptr));

    scoped_feature_list_ = base::MakeUnique<base::test::ScopedFeatureList>();
  }

  void TearDown() override { profile_manager_.DeleteAllTestingProfiles(); }

  content::TestBrowserThreadBundle thread_bundle_;

 protected:
  TestingProfileManager profile_manager_;

  TestPreviewsLogger* logger_;
  std::unique_ptr<TestPreviewsUIService> previews_ui_service_;
  std::unique_ptr<TestUINetworkQualityEstimatorService> ui_nqe_service_;

  // InterventionsInternalPageHandler's variables.
  mojom::InterventionsInternalsPageHandlerRequest handler_request_;
  std::unique_ptr<InterventionsInternalsPageHandler> page_handler_;

  // InterventionsInternalPage's variables.
  mojom::InterventionsInternalsPageRequest page_request_;
  std::unique_ptr<TestInterventionsInternalsPage> page_;

  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

TEST_F(InterventionsInternalsPageHandlerTest, GetPreviewsEnabledCount) {
  page_handler_->GetPreviewsEnabled(
      base::BindOnce(&MockGetPreviewsEnabledCallback));

  constexpr size_t expected = 4;
  EXPECT_EQ(expected, passed_in_modes.size());
}

TEST_F(InterventionsInternalsPageHandlerTest, AMPRedirectionDisabled) {
  // Init with kAMPRedirection disabled.
  scoped_feature_list_->InitWithFeatures({},
                                         {previews::features::kAMPRedirection});

  page_handler_->GetPreviewsEnabled(
      base::BindOnce(&MockGetPreviewsEnabledCallback));
  auto amp_redirection = passed_in_modes.find(kAMPRedirectionPreviews);
  ASSERT_NE(passed_in_modes.end(), amp_redirection);
  EXPECT_EQ(kAmpRedirectionDescription, amp_redirection->second->description);
  EXPECT_FALSE(amp_redirection->second->enabled);
}

TEST_F(InterventionsInternalsPageHandlerTest, AMPRedirectionEnabled) {
  // Init with kAMPRedirection enabled.
  scoped_feature_list_->InitWithFeatures({previews::features::kAMPRedirection},
                                         {});

  page_handler_->GetPreviewsEnabled(
      base::BindOnce(&MockGetPreviewsEnabledCallback));
  auto amp_redirection = passed_in_modes.find(kAMPRedirectionPreviews);
  ASSERT_NE(passed_in_modes.end(), amp_redirection);
  EXPECT_EQ(kAmpRedirectionDescription, amp_redirection->second->description);
  EXPECT_TRUE(amp_redirection->second->enabled);
}

TEST_F(InterventionsInternalsPageHandlerTest, ClientLoFiDisabled) {
  // Init with kClientLoFi disabled.
  scoped_feature_list_->InitWithFeatures({}, {previews::features::kClientLoFi});

  page_handler_->GetPreviewsEnabled(
      base::BindOnce(&MockGetPreviewsEnabledCallback));
  auto client_lofi = passed_in_modes.find(kClientLoFiPreviews);
  ASSERT_NE(passed_in_modes.end(), client_lofi);
  EXPECT_EQ(kClientLoFiDescription, client_lofi->second->description);
  EXPECT_FALSE(client_lofi->second->enabled);
}

TEST_F(InterventionsInternalsPageHandlerTest, ClientLoFiEnabled) {
  // Init with kClientLoFi enabled.
  scoped_feature_list_->InitWithFeatures({previews::features::kClientLoFi}, {});

  page_handler_->GetPreviewsEnabled(
      base::BindOnce(&MockGetPreviewsEnabledCallback));
  auto client_lofi = passed_in_modes.find(kClientLoFiPreviews);
  ASSERT_NE(passed_in_modes.end(), client_lofi);
  EXPECT_EQ(kClientLoFiDescription, client_lofi->second->description);
  EXPECT_TRUE(client_lofi->second->enabled);
}

TEST_F(InterventionsInternalsPageHandlerTest, NoScriptDisabled) {
  // Init with kNoScript disabled.
  scoped_feature_list_->InitWithFeatures(
      {}, {previews::features::kNoScriptPreviews});

  page_handler_->GetPreviewsEnabled(
      base::BindOnce(&MockGetPreviewsEnabledCallback));
  auto noscript = passed_in_modes.find(kNoScriptPreviews);
  ASSERT_NE(passed_in_modes.end(), noscript);
  EXPECT_EQ(kNoScriptDescription, noscript->second->description);
  EXPECT_FALSE(noscript->second->enabled);
}

TEST_F(InterventionsInternalsPageHandlerTest, NoScriptEnabled) {
  // Init with kNoScript enabled.
  scoped_feature_list_->InitWithFeatures(
      {previews::features::kNoScriptPreviews}, {});

  page_handler_->GetPreviewsEnabled(
      base::BindOnce(&MockGetPreviewsEnabledCallback));
  auto noscript = passed_in_modes.find(kNoScriptPreviews);
  ASSERT_NE(passed_in_modes.end(), noscript);
  EXPECT_EQ(kNoScriptDescription, noscript->second->description);
  EXPECT_TRUE(noscript->second->enabled);
}

TEST_F(InterventionsInternalsPageHandlerTest, OfflinePreviewsDisabled) {
  // Init with kOfflinePreviews disabled.
  scoped_feature_list_->InitWithFeatures(
      {}, {previews::features::kOfflinePreviews});

  page_handler_->GetPreviewsEnabled(
      base::BindOnce(&MockGetPreviewsEnabledCallback));
  auto offline_previews = passed_in_modes.find(kOfflinePreviews);
  ASSERT_NE(passed_in_modes.end(), offline_previews);
  EXPECT_EQ(kOfflineDesciption, offline_previews->second->description);
  EXPECT_FALSE(offline_previews->second->enabled);
}

TEST_F(InterventionsInternalsPageHandlerTest, OfflinePreviewsEnabled) {
  // Init with kOfflinePreviews enabled.
  scoped_feature_list_->InitWithFeatures({previews::features::kOfflinePreviews},
                                         {});

  page_handler_->GetPreviewsEnabled(
      base::BindOnce(&MockGetPreviewsEnabledCallback));
  auto offline_previews = passed_in_modes.find(kOfflinePreviews);
  ASSERT_NE(passed_in_modes.end(), offline_previews);
  EXPECT_TRUE(offline_previews->second);
  EXPECT_EQ(kOfflineDesciption, offline_previews->second->description);
  EXPECT_TRUE(offline_previews->second->enabled);
}

TEST_F(InterventionsInternalsPageHandlerTest, GetFlagsCount) {
  page_handler_->GetPreviewsFlagsDetails(
      base::BindOnce(&MockGetPreviewsFlagsCallback));

  constexpr size_t expected = 3;
  EXPECT_EQ(expected, passed_in_flags.size());
}

TEST_F(InterventionsInternalsPageHandlerTest, GetFlagsEctDefaultValue) {
  page_handler_->GetPreviewsFlagsDetails(
      base::BindOnce(&MockGetPreviewsFlagsCallback));
  auto ect_flag = passed_in_flags.find(kEctFlag);

  ASSERT_NE(passed_in_flags.end(), ect_flag);
  EXPECT_EQ(flag_descriptions::kForceEffectiveConnectionTypeName,
            ect_flag->second->description);
  EXPECT_EQ(kDefaultFlagValue, ect_flag->second->value);
  EXPECT_EQ(kEctFlagLink, ect_flag->second->link);
}

TEST_F(InterventionsInternalsPageHandlerTest, GetFlagsForceEctValue) {
  std::string expected_ects[] = {
      net::kEffectiveConnectionTypeUnknown,
      net::kEffectiveConnectionTypeOffline,
      net::kEffectiveConnectionTypeSlow2G,
      net::kEffectiveConnectionType3G,
      net::kEffectiveConnectionType4G,
  };

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  for (auto expected_ect : expected_ects) {
    command_line->AppendSwitchASCII(switches::kForceEffectiveConnectionType,
                                    expected_ect);
    page_handler_->GetPreviewsFlagsDetails(
        base::BindOnce(&MockGetPreviewsFlagsCallback));
    auto ect_flag = passed_in_flags.find(kEctFlag);

    ASSERT_NE(passed_in_flags.end(), ect_flag);
    EXPECT_EQ(flag_descriptions::kForceEffectiveConnectionTypeName,
              ect_flag->second->description);
    EXPECT_EQ(expected_ect, ect_flag->second->value);
    EXPECT_EQ(kEctFlagLink, ect_flag->second->link);
  }
}

TEST_F(InterventionsInternalsPageHandlerTest, GetFlagsNoScriptDefaultValue) {
  page_handler_->GetPreviewsFlagsDetails(
      base::BindOnce(&MockGetPreviewsFlagsCallback));
  auto noscript_flag = passed_in_flags.find(kNoScriptFlag);

  ASSERT_NE(passed_in_flags.end(), noscript_flag);
  EXPECT_EQ(flag_descriptions::kEnableNoScriptPreviewsName,
            noscript_flag->second->description);
  EXPECT_EQ(kDefaultFlagValue, noscript_flag->second->value);
  EXPECT_EQ(kNoScriptFlagLink, noscript_flag->second->link);
}

TEST_F(InterventionsInternalsPageHandlerTest, GetFlagsNoScriptEnabled) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kEnableFeatures,
                                  kNoScriptFeatureName);

  page_handler_->GetPreviewsFlagsDetails(
      base::BindOnce(&MockGetPreviewsFlagsCallback));
  auto noscript_flag = passed_in_flags.find(kNoScriptFlag);

  ASSERT_NE(passed_in_flags.end(), noscript_flag);
  EXPECT_EQ(flag_descriptions::kEnableNoScriptPreviewsName,
            noscript_flag->second->description);
  EXPECT_EQ("Forced Enabled", noscript_flag->second->value);
  EXPECT_EQ(kNoScriptFlagLink, noscript_flag->second->link);
}

TEST_F(InterventionsInternalsPageHandlerTest, GetFlagsNoScriptDisabled) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kDisableFeatures,
                                  kNoScriptFeatureName);

  page_handler_->GetPreviewsFlagsDetails(
      base::BindOnce(&MockGetPreviewsFlagsCallback));
  auto noscript_flag = passed_in_flags.find(kNoScriptFlag);

  ASSERT_NE(passed_in_flags.end(), noscript_flag);
  EXPECT_EQ(flag_descriptions::kEnableNoScriptPreviewsName,
            noscript_flag->second->description);
  EXPECT_EQ("Forced Disabled", noscript_flag->second->value);
  EXPECT_EQ(kNoScriptFlagLink, noscript_flag->second->link);
}

#if defined(OS_ANDROID)
#define TestAndroid(x) x
#else
#define TestAndroid(x) DISABLED_##x
#endif  // OS_ANDROID
TEST_F(InterventionsInternalsPageHandlerTest,
       TestAndroid(GetFlagsOfflinePageDefaultValue)) {
  page_handler_->GetPreviewsFlagsDetails(
      base::BindOnce(&MockGetPreviewsFlagsCallback));
  auto offline_page_flag = passed_in_flags.find(kOfflinePageFlag);

  ASSERT_NE(passed_in_flags.end(), offline_page_flag);
#if defined(OS_ANDROID)
  EXPECT_EQ(flag_descriptions::kEnableOfflinePreviewsName,
            offline_page_flag->second->description);
#endif  // OS_ANDROID
  EXPECT_EQ(kDefaultFlagValue, offline_page_flag->second->value);
  EXPECT_EQ(kOfflinePageFlagLink, offline_page_flag->second->link);
}

TEST_F(InterventionsInternalsPageHandlerTest,
       TestAndroid(GetFlagsOfflinePageEnabled)) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kEnableFeatures,
                                  kOfflinePageFeatureName);

  page_handler_->GetPreviewsFlagsDetails(
      base::BindOnce(&MockGetPreviewsFlagsCallback));
  auto offline_page_flag = passed_in_flags.find(kOfflinePageFlag);

  ASSERT_NE(passed_in_flags.end(), offline_page_flag);
#if defined(OS_ANDROID)
  EXPECT_EQ(flag_descriptions::kEnableOfflinePreviewsName,
            offline_page_flag->second->description);
#endif  // OS_ANDROID
  EXPECT_EQ("Forced Enabled", offline_page_flag->second->value);
  EXPECT_EQ(kOfflinePageFlagLink, offline_page_flag->second->link);
}

TEST_F(InterventionsInternalsPageHandlerTest,
       TestAndroid(GetFlagsOfflinePageDisabled)) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kDisableFeatures,
                                  kOfflinePageFeatureName);

  page_handler_->GetPreviewsFlagsDetails(
      base::BindOnce(&MockGetPreviewsFlagsCallback));
  auto offline_page_flag = passed_in_flags.find(kOfflinePageFlag);

  ASSERT_NE(passed_in_flags.end(), offline_page_flag);
#if defined(OS_ANDROID)
  EXPECT_EQ(flag_descriptions::kEnableOfflinePreviewsName,
            offline_page_flag->second->description);
#endif  // OS_ANDROID
  EXPECT_EQ("Forced Disabled", offline_page_flag->second->value);
  EXPECT_EQ(kOfflinePageFlagLink, offline_page_flag->second->link);
}

TEST_F(InterventionsInternalsPageHandlerTest, OnNewMessageLogAddedPostToPage) {
  const previews::PreviewsLogger::MessageLog expected_messages[] = {
      previews::PreviewsLogger::MessageLog("Event_a", "Some description a",
                                           GURL("http://www.url_a.com/url_a"),
                                           base::Time::Now()),
      previews::PreviewsLogger::MessageLog("Event_b", "Some description b",
                                           GURL("http://www.url_b.com/url_b"),
                                           base::Time::Now()),
      previews::PreviewsLogger::MessageLog("Event_c", "Some description c",
                                           GURL("http://www.url_c.com/url_c"),
                                           base::Time::Now()),
  };

  for (auto message : expected_messages) {
    page_handler_->OnNewMessageLogAdded(message);
    base::RunLoop().RunUntilIdle();

    mojom::MessageLogPtr* actual = page_->message();
    EXPECT_EQ(message.event_type, (*actual)->type);
    EXPECT_EQ(message.event_description, (*actual)->description);
    EXPECT_EQ(message.url, (*actual)->url);
    int64_t expected_time = message.time.ToJavaTime();
    EXPECT_EQ(expected_time, (*actual)->time);
  }
}

TEST_F(InterventionsInternalsPageHandlerTest, ObserverIsRemovedWhenDestroyed) {
  EXPECT_FALSE(logger_->RemovedObserverIsCalled());
  EXPECT_FALSE(ui_nqe_service_->RemovedObserverIsCalled());
  page_handler_.reset();
  EXPECT_TRUE(logger_->RemovedObserverIsCalled());
  EXPECT_TRUE(ui_nqe_service_->RemovedObserverIsCalled());
}

TEST_F(InterventionsInternalsPageHandlerTest, OnNewBlacklistedHostPostToPage) {
  const std::string hosts[] = {
      "example_0.com", "example_1.com", "example_2.com",
  };

  for (auto expected_host : hosts) {
    base::Time expected_time = base::Time::Now();
    page_handler_->OnNewBlacklistedHost(expected_host, expected_time);
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(expected_host, page_->host_blacklisted());
    EXPECT_EQ(expected_time.ToJavaTime(), page_->host_blacklisted_time());
  }
}

TEST_F(InterventionsInternalsPageHandlerTest, OnUserBlacklistedPostToPage) {
  page_handler_->OnUserBlacklistedStatusChange(true /* blacklisted */);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(page_->user_blacklisted());

  page_handler_->OnUserBlacklistedStatusChange(false /* blacklisted */);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(page_->user_blacklisted());
}

TEST_F(InterventionsInternalsPageHandlerTest, OnBlacklistClearedPostToPage) {
  base::Time times[] = {
      base::Time::FromJsTime(-413696806000),  // Nov 21 1956 20:13:14 UTC
      base::Time::FromJsTime(758620800000),   // Jan 15 1994 08:00:00 UTC
      base::Time::FromJsTime(1581696550000),  // Feb 14 2020 16:09:10 UTC
  };
  for (auto expected_time : times) {
    page_handler_->OnBlacklistCleared(expected_time);
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(expected_time.ToJavaTime(), page_->blacklist_cleared_time());
  }
}

TEST_F(InterventionsInternalsPageHandlerTest,
       SetIgnorePreviewsBlacklistDecisionCallsUIServiceCorrectly) {
  page_handler_->SetIgnorePreviewsBlacklistDecision(true /* ignored */);
  EXPECT_TRUE(previews_ui_service_->blacklist_ignored());

  page_handler_->SetIgnorePreviewsBlacklistDecision(false /* ignored */);
  EXPECT_FALSE(previews_ui_service_->blacklist_ignored());
}

TEST_F(InterventionsInternalsPageHandlerTest,
       PageUpdateOnBlacklistIgnoredChange) {
  page_handler_->OnIgnoreBlacklistDecisionStatusChanged(true /* ignored */);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(page_->blacklist_ignored());

  page_handler_->OnIgnoreBlacklistDecisionStatusChanged(false /* ignored */);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(page_->blacklist_ignored());
}

TEST_F(InterventionsInternalsPageHandlerTest,
       IgnoreBlacklistReversedOnLastObserverRemovedCalled) {
  page_handler_->OnLastObserverRemove();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(page_->blacklist_ignored());
}

}  // namespace
