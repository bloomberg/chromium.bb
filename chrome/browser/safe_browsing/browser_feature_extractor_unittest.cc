// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/browser_feature_extractor.h"

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
class BrowserFeatureExtractorTest : public RenderViewHostTestHarness {
 protected:
  BrowserFeatureExtractorTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {
  }

  virtual void SetUp() {
    RenderViewHostTestHarness::SetUp();
    profile_->CreateHistoryService(true /* delete_file */, false /* no_db */);
    extractor_.reset(new BrowserFeatureExtractor(contents()));
    num_pending_ = 0;
  }

  virtual void TearDown() {
    extractor_.reset();
    profile_->DestroyHistoryService();
    RenderViewHostTestHarness::TearDown();
    ASSERT_EQ(0, num_pending_);
  }

  HistoryService* history_service() {
    return profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  }

  bool ExtractFeatures(ClientPhishingRequest* request) {
    StartExtractFeatures(request);
    MessageLoop::current()->Run();
    EXPECT_EQ(1U, success_.count(request));
    return success_.count(request) ? success_[request] : false;
  }

  void StartExtractFeatures(ClientPhishingRequest* request) {
    success_.erase(request);
    ++num_pending_;
    extractor_->ExtractFeatures(
        request,
        NewCallback(this,
                    &BrowserFeatureExtractorTest::ExtractFeaturesDone));
  }

  BrowserThread ui_thread_;
  int num_pending_;
  scoped_ptr<BrowserFeatureExtractor> extractor_;
  std::map<ClientPhishingRequest*, bool> success_;

 private:
  void ExtractFeaturesDone(bool success, ClientPhishingRequest* request) {
    ASSERT_EQ(0U, success_.count(request));
    success_[request] = success;
    if (--num_pending_ == 0) {
      MessageLoop::current()->Quit();
    }
  }
};

TEST_F(BrowserFeatureExtractorTest, UrlNotInHistory) {
  ClientPhishingRequest request;
  request.set_url("http://www.google.com/");
  request.set_client_score(0.5);
  EXPECT_FALSE(ExtractFeatures(&request));
}

TEST_F(BrowserFeatureExtractorTest, RequestNotInitialized) {
  ClientPhishingRequest request;
  request.set_url("http://www.google.com/");
  // Request is missing the score value.
  EXPECT_FALSE(ExtractFeatures(&request));
}

TEST_F(BrowserFeatureExtractorTest, UrlInHistory) {
  history_service()->AddPage(GURL("http://www.foo.com/bar.html"),
                             history::SOURCE_BROWSED);
  history_service()->AddPage(GURL("https://www.foo.com/gaa.html"),
                             history::SOURCE_BROWSED);  // same host HTTPS.
  history_service()->AddPage(GURL("http://www.foo.com/gaa.html"),
                             history::SOURCE_BROWSED);  // same host HTTP.
  history_service()->AddPage(GURL("http://bar.foo.com/gaa.html"),
                             history::SOURCE_BROWSED);  // different host.
  history_service()->AddPage(GURL("http://www.foo.com/bar.html?a=b"),
                             base::Time::Now() - base::TimeDelta::FromHours(23),
                             NULL, 0, GURL(), PageTransition::LINK,
                             history::RedirectList(), history::SOURCE_BROWSED,
                             false);
  history_service()->AddPage(GURL("http://www.foo.com/bar.html"),
                             base::Time::Now() - base::TimeDelta::FromHours(25),
                             NULL, 0, GURL(), PageTransition::TYPED,
                             history::RedirectList(), history::SOURCE_BROWSED,
                             false);
  history_service()->AddPage(GURL("https://www.foo.com/goo.html"),
                             base::Time::Now() - base::TimeDelta::FromDays(5),
                             NULL, 0, GURL(), PageTransition::TYPED,
                             history::RedirectList(), history::SOURCE_BROWSED,
                             false);

  ClientPhishingRequest request;
  request.set_url("http://www.foo.com/bar.html");
  request.set_client_score(0.5);
  EXPECT_TRUE(ExtractFeatures(&request));
  std::map<std::string, double> features;
  for (int i = 0; i < request.non_model_feature_map_size(); ++i) {
    const ClientPhishingRequest::Feature& feature =
        request.non_model_feature_map(i);
    EXPECT_EQ(0U, features.count(feature.name()));
    features[feature.name()] = feature.value();
  }
  EXPECT_EQ(8U, features.size());
  EXPECT_DOUBLE_EQ(2.0, features[features::kUrlHistoryVisitCount]);
  EXPECT_DOUBLE_EQ(1.0,
                   features[features::kUrlHistoryVisitCountMoreThan24hAgo]);
  EXPECT_DOUBLE_EQ(1.0, features[features::kUrlHistoryTypedCount]);
  EXPECT_DOUBLE_EQ(1.0, features[features::kUrlHistoryLinkCount]);
  EXPECT_DOUBLE_EQ(4.0, features[features::kHttpHostVisitCount]);
  EXPECT_DOUBLE_EQ(2.0, features[features::kHttpsHostVisitCount]);
  EXPECT_DOUBLE_EQ(1.0, features[features::kFirstHttpHostVisitMoreThan24hAgo]);
  EXPECT_DOUBLE_EQ(1.0, features[features::kFirstHttpsHostVisitMoreThan24hAgo]);
}

TEST_F(BrowserFeatureExtractorTest, MultipleRequestsAtOnce) {
  history_service()->AddPage(GURL("http://www.foo.com/bar.html"),
                             history::SOURCE_BROWSED);
  ClientPhishingRequest request;
  request.set_url("http://www.foo.com/bar.html");
  request.set_client_score(0.5);
  StartExtractFeatures(&request);

  ClientPhishingRequest request2;
  request2.set_url("http://www.foo.com/goo.html");
  request2.set_client_score(1.0);
  StartExtractFeatures(&request2);

  MessageLoop::current()->Run();
  EXPECT_TRUE(success_[&request]);
  // Success is false because the second URL is not in the history and we are
  // not able to distinguish between a missing URL in the history and an error.
  EXPECT_FALSE(success_[&request2]);
}
}  // namespace safe_browsing
