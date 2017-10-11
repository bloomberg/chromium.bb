// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/interventions_internals/interventions_internals_page_handler.h"

#include <unordered_map>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/ui/webui/interventions_internals/interventions_internals.mojom.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_logger.h"
#include "components/previews/core/previews_logger_observer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// The key for the mapping for the enabled/disabled status map.
constexpr char kAMPRedirectionPreviews[] = "ampPreviews";
constexpr char kClientLoFiPreviews[] = "clientLoFiPreviews";
constexpr char kOfflinePreviews[] = "offlinePreviews";

// Description for statuses.
constexpr char kAmpRedirectionDescription[] = "AMP Previews";
constexpr char kClientLoFiDescription[] = "Client LoFi Previews";
constexpr char kOfflineDesciption[] = "Offline Previews";

// The map that would be passed to the callback in GetPreviewsEnabledCallback.
std::unordered_map<std::string, mojom::PreviewsStatusPtr> passed_in_map;

void MockGetPreviewsEnabledCallback(
    std::unordered_map<std::string, mojom::PreviewsStatusPtr> params) {
  passed_in_map = std::move(params);
}

// Mock class that would be pass into the InterventionsInternalsPageHandler by
// calling its SetClientPage method. Used to test that the PageHandler
// actually invokes the page's LogNewMessage method with the correct message.
class TestInterventionsInternalsPage
    : public mojom::InterventionsInternalsPage {
 public:
  TestInterventionsInternalsPage(
      mojom::InterventionsInternalsPageRequest request)
      : binding_(this, std::move(request)) {}

  ~TestInterventionsInternalsPage() override {}

  // mojom::InterventionsInternalsPage:
  void LogNewMessage(mojom::MessageLogPtr message) override {
    message_ = base::MakeUnique<mojom::MessageLogPtr>(std::move(message));
  }

  // Expose passed in message in LogNewMessage for testing.
  mojom::MessageLogPtr* message() const { return message_.get(); }

 private:
  mojo::Binding<mojom::InterventionsInternalsPage> binding_;

  // The MessageLogPtr passed in LogNewMessage method.
  std::unique_ptr<mojom::MessageLogPtr> message_;
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

class InterventionsInternalsPageHandlerTest : public testing::Test {
 public:
  InterventionsInternalsPageHandlerTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO) {}

  ~InterventionsInternalsPageHandlerTest() override {}

  void SetUp() override {
    scoped_task_environment_.RunUntilIdle();
    logger_ = base::MakeUnique<TestPreviewsLogger>();

    mojom::InterventionsInternalsPageHandlerPtr page_handler_ptr;
    handler_request_ = mojo::MakeRequest(&page_handler_ptr);
    page_handler_ = base::MakeUnique<InterventionsInternalsPageHandler>(
        std::move(handler_request_), logger_.get());

    mojom::InterventionsInternalsPagePtr page_ptr;
    page_request_ = mojo::MakeRequest(&page_ptr);
    page_ = base::MakeUnique<TestInterventionsInternalsPage>(
        std::move(page_request_));

    page_handler_->SetClientPage(std::move(page_ptr));

    scoped_feature_list_ = base::MakeUnique<base::test::ScopedFeatureList>();
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<TestPreviewsLogger> logger_;

  // InterventionsInternalPageHandler's variables.
  mojom::InterventionsInternalsPageHandlerRequest handler_request_;
  std::unique_ptr<InterventionsInternalsPageHandler> page_handler_;

  // InterventionsInternalPage's variables.
  mojom::InterventionsInternalsPageRequest page_request_;
  std::unique_ptr<TestInterventionsInternalsPage> page_;

  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

TEST_F(InterventionsInternalsPageHandlerTest, CorrectSizeOfPassingParameters) {
  page_handler_->GetPreviewsEnabled(
      base::BindOnce(&MockGetPreviewsEnabledCallback));

  constexpr size_t expected = 3;
  EXPECT_EQ(expected, passed_in_map.size());
}

TEST_F(InterventionsInternalsPageHandlerTest, AMPRedirectionDisabled) {
  // Init with kAMPRedirection disabled.
  scoped_feature_list_->InitWithFeatures({},
                                         {previews::features::kAMPRedirection});

  page_handler_->GetPreviewsEnabled(
      base::BindOnce(&MockGetPreviewsEnabledCallback));
  auto amp_redirection = passed_in_map.find(kAMPRedirectionPreviews);
  ASSERT_NE(passed_in_map.end(), amp_redirection);
  EXPECT_EQ(kAmpRedirectionDescription, amp_redirection->second->description);
  EXPECT_FALSE(amp_redirection->second->enabled);
}

TEST_F(InterventionsInternalsPageHandlerTest, AMPRedirectionEnabled) {
  // Init with kAMPRedirection enabled.
  scoped_feature_list_->InitWithFeatures({previews::features::kAMPRedirection},
                                         {});

  page_handler_->GetPreviewsEnabled(
      base::BindOnce(&MockGetPreviewsEnabledCallback));
  auto amp_redirection = passed_in_map.find(kAMPRedirectionPreviews);
  ASSERT_NE(passed_in_map.end(), amp_redirection);
  EXPECT_EQ(kAmpRedirectionDescription, amp_redirection->second->description);
  EXPECT_TRUE(amp_redirection->second->enabled);
}

TEST_F(InterventionsInternalsPageHandlerTest, ClientLoFiDisabled) {
  // Init with kClientLoFi disabled.
  scoped_feature_list_->InitWithFeatures({}, {previews::features::kClientLoFi});

  page_handler_->GetPreviewsEnabled(
      base::BindOnce(&MockGetPreviewsEnabledCallback));
  auto client_lofi = passed_in_map.find(kClientLoFiPreviews);
  ASSERT_NE(passed_in_map.end(), client_lofi);
  EXPECT_EQ(kClientLoFiDescription, client_lofi->second->description);
  EXPECT_FALSE(client_lofi->second->enabled);
}

TEST_F(InterventionsInternalsPageHandlerTest, ClientLoFiEnabled) {
  // Init with kClientLoFi enabled.
  scoped_feature_list_->InitWithFeatures({previews::features::kClientLoFi}, {});

  page_handler_->GetPreviewsEnabled(
      base::BindOnce(&MockGetPreviewsEnabledCallback));
  auto client_lofi = passed_in_map.find(kClientLoFiPreviews);
  ASSERT_NE(passed_in_map.end(), client_lofi);
  EXPECT_EQ(kClientLoFiDescription, client_lofi->second->description);
  EXPECT_TRUE(client_lofi->second->enabled);
}

TEST_F(InterventionsInternalsPageHandlerTest, OfflinePreviewsDisabled) {
  // Init with kOfflinePreviews disabled.
  scoped_feature_list_->InitWithFeatures(
      {}, {previews::features::kOfflinePreviews});

  page_handler_->GetPreviewsEnabled(
      base::BindOnce(&MockGetPreviewsEnabledCallback));
  auto offline_previews = passed_in_map.find(kOfflinePreviews);
  ASSERT_NE(passed_in_map.end(), offline_previews);
  EXPECT_EQ(kOfflineDesciption, offline_previews->second->description);
  EXPECT_FALSE(offline_previews->second->enabled);
}

TEST_F(InterventionsInternalsPageHandlerTest, OfflinePreviewsEnabled) {
  // Init with kOfflinePreviews enabled.
  scoped_feature_list_->InitWithFeatures({previews::features::kOfflinePreviews},
                                         {});

  page_handler_->GetPreviewsEnabled(
      base::BindOnce(&MockGetPreviewsEnabledCallback));
  auto offline_previews = passed_in_map.find(kOfflinePreviews);
  ASSERT_NE(passed_in_map.end(), offline_previews);
  EXPECT_TRUE(offline_previews->second);
  EXPECT_EQ(kOfflineDesciption, offline_previews->second->description);
  EXPECT_TRUE(offline_previews->second->enabled);
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
  page_handler_.reset();
  EXPECT_TRUE(logger_->RemovedObserverIsCalled());
}

}  // namespace
