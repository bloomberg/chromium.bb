// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/browser_feature_extractor.h"

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/browser_features.h"
#include "chrome/browser/safe_browsing/client_side_detection_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "content/common/page_transition_types.h"
#include "content/common/view_messages.h"
#include "crypto/sha2.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;
using ::testing::StrictMock;

namespace safe_browsing {
namespace {
class MockClientSideDetectionService : public ClientSideDetectionService {
 public:
  MockClientSideDetectionService() : ClientSideDetectionService(NULL) {}
  virtual ~MockClientSideDetectionService() {};

  MOCK_CONST_METHOD1(IsBadIpAddress, bool(const std::string&));
};
}  // namespace

class BrowserFeatureExtractorTest : public RenderViewHostTestHarness {
 protected:
  BrowserFeatureExtractorTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {
  }

  virtual void SetUp() {
    RenderViewHostTestHarness::SetUp();
    profile_->CreateHistoryService(true /* delete_file */, false /* no_db */);
    service_.reset(new StrictMock<MockClientSideDetectionService>());
    extractor_.reset(new BrowserFeatureExtractor(contents(), service_.get()));
    num_pending_ = 0;
    browse_info_.reset(new BrowseInfo);
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

  // This is similar to NavigateAndCommit that is in test_tab_contents, but
  // allows us to specify the referrer and page_transition_type.
  void NavigateAndCommit(const GURL& url,
                         const GURL& referrer,
                         PageTransition::Type type) {
    contents()->controller().LoadURL(url, referrer, type);

    static int page_id = 0;
    ViewHostMsg_FrameNavigate_Params params;
    InitNavigateParams(&params, ++page_id, url, type);
    params.referrer = referrer;

    RenderViewHost* rvh = contents()->pending_rvh();
    if (!rvh) {
      rvh = contents()->render_view_host();
    }
    contents()->ProceedWithCrossSiteNavigation();
    contents()->TestDidNavigate(rvh, params);
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
        browse_info_.get(),
        request,
        NewCallback(this,
                    &BrowserFeatureExtractorTest::ExtractFeaturesDone));
  }

  void GetFeatureMap(const ClientPhishingRequest& request,
                     std::map<std::string, double>* features) {
    for (int i = 0; i < request.non_model_feature_map_size(); ++i) {
      const ClientPhishingRequest::Feature& feature =
          request.non_model_feature_map(i);
      EXPECT_EQ(0U, features->count(feature.name()));
      (*features)[feature.name()] = feature.value();
    }
  }

  BrowserThread ui_thread_;
  int num_pending_;
  scoped_ptr<BrowserFeatureExtractor> extractor_;
  std::map<ClientPhishingRequest*, bool> success_;
  scoped_ptr<BrowseInfo> browse_info_;
  scoped_ptr<MockClientSideDetectionService> service_;

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
  contents()->NavigateAndCommit(GURL("http://www.google.com"));
  request.set_url("http://www.google.com/");
  request.set_client_score(0.5);
  EXPECT_FALSE(ExtractFeatures(&request));
}

TEST_F(BrowserFeatureExtractorTest, RequestNotInitialized) {
  ClientPhishingRequest request;
  request.set_url("http://www.google.com/");
  // Request is missing the score value.
  contents()->NavigateAndCommit(GURL("http://www.google.com"));
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

  contents()->NavigateAndCommit(GURL("http://www.foo.com/bar.html"));

  ClientPhishingRequest request;
  request.set_url("http://www.foo.com/bar.html");
  request.set_client_score(0.5);
  EXPECT_TRUE(ExtractFeatures(&request));
  std::map<std::string, double> features;
  GetFeatureMap(request, &features);

  EXPECT_EQ(12U, features.size());
  EXPECT_DOUBLE_EQ(2.0, features[features::kUrlHistoryVisitCount]);
  EXPECT_DOUBLE_EQ(1.0,
                   features[features::kUrlHistoryVisitCountMoreThan24hAgo]);
  EXPECT_DOUBLE_EQ(1.0, features[features::kUrlHistoryTypedCount]);
  EXPECT_DOUBLE_EQ(1.0, features[features::kUrlHistoryLinkCount]);
  EXPECT_DOUBLE_EQ(4.0, features[features::kHttpHostVisitCount]);
  EXPECT_DOUBLE_EQ(2.0, features[features::kHttpsHostVisitCount]);
  EXPECT_DOUBLE_EQ(1.0, features[features::kFirstHttpHostVisitMoreThan24hAgo]);
  EXPECT_DOUBLE_EQ(1.0, features[features::kFirstHttpsHostVisitMoreThan24hAgo]);

  request.Clear();
  request.set_url("http://bar.foo.com/gaa.html");
  request.set_client_score(0.5);
  EXPECT_TRUE(ExtractFeatures(&request));
  features.clear();
  GetFeatureMap(request, &features);
  // We have less features because we didn't Navigate to this page, so we don't
  // have Referrer, IsFirstNavigation, HasSSLReferrer, etc.
  EXPECT_EQ(7U, features.size());
  EXPECT_DOUBLE_EQ(1.0, features[features::kUrlHistoryVisitCount]);
  EXPECT_DOUBLE_EQ(0.0,
                   features[features::kUrlHistoryVisitCountMoreThan24hAgo]);
  EXPECT_DOUBLE_EQ(0.0, features[features::kUrlHistoryTypedCount]);
  EXPECT_DOUBLE_EQ(1.0, features[features::kUrlHistoryLinkCount]);
  EXPECT_DOUBLE_EQ(1.0, features[features::kHttpHostVisitCount]);
  EXPECT_DOUBLE_EQ(0.0, features[features::kHttpsHostVisitCount]);
  EXPECT_DOUBLE_EQ(0.0, features[features::kFirstHttpHostVisitMoreThan24hAgo]);
  EXPECT_FALSE(features.count(features::kFirstHttpsHostVisitMoreThan24hAgo));
}

TEST_F(BrowserFeatureExtractorTest, MultipleRequestsAtOnce) {
  history_service()->AddPage(GURL("http://www.foo.com/bar.html"),
                             history::SOURCE_BROWSED);
  contents()->NavigateAndCommit(GURL("http:/www.foo.com/bar.html"));
  ClientPhishingRequest request;
  request.set_url("http://www.foo.com/bar.html");
  request.set_client_score(0.5);
  StartExtractFeatures(&request);

  contents()->NavigateAndCommit(GURL("http://www.foo.com/goo.html"));
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

TEST_F(BrowserFeatureExtractorTest, BrowseFeatures) {
  history_service()->AddPage(GURL("http://www.foo.com/"),
                             history::SOURCE_BROWSED);
  history_service()->AddPage(GURL("http://www.foo.com/page.html"),
                             history::SOURCE_BROWSED);
  history_service()->AddPage(GURL("http://www.bar.com/"),
                             history::SOURCE_BROWSED);
  history_service()->AddPage(GURL("http://www.bar.com/other_page.html"),
                             history::SOURCE_BROWSED);
  history_service()->AddPage(GURL("http://www.baz.com/"),
                             history::SOURCE_BROWSED);

  ClientPhishingRequest request;
  request.set_url("http://www.foo.com/");
  request.set_client_score(0.5);
  NavigateAndCommit(GURL("http://www.foo.com/"),
                    GURL("http://google.com/"),
                    PageTransition::AUTO_BOOKMARK |
                    PageTransition::FORWARD_BACK);

  EXPECT_TRUE(ExtractFeatures(&request));
  std::map<std::string, double> features;
  GetFeatureMap(request, &features);

  EXPECT_EQ(1.0,
            features[StringPrintf("%s=%s",
                                  features::kReferrer,
                                  "http://google.com/")]);
  EXPECT_EQ(0.0, features[features::kHasSSLReferrer]);
  EXPECT_EQ(2.0, features[features::kPageTransitionType]);
  EXPECT_EQ(1.0, features[features::kIsFirstNavigation]);

  request.Clear();
  request.set_url("http://www.foo.com/page.html");
  request.set_client_score(0.5);
  NavigateAndCommit(GURL("http://www.foo.com/page.html"),
                    GURL("http://www.foo.com"),
                    PageTransition::TYPED |
                    PageTransition::CHAIN_START |
                    PageTransition::CLIENT_REDIRECT);

  EXPECT_TRUE(ExtractFeatures(&request));
  features.clear();
  GetFeatureMap(request, &features);

  EXPECT_EQ(1,
            features[StringPrintf("%s=%s",
                                  features::kReferrer,
                                  "http://www.foo.com/")]);
  EXPECT_EQ(0.0, features[features::kHasSSLReferrer]);
  EXPECT_EQ(1.0, features[features::kPageTransitionType]);
  EXPECT_EQ(0.0, features[features::kIsFirstNavigation]);
  EXPECT_EQ(1.0,
            features[StringPrintf("%s%s=%s",
                                  features::kHostPrefix,
                                  features::kReferrer,
                                  "http://google.com/")]);
  EXPECT_EQ(2.0,
            features[StringPrintf("%s%s",
                                  features::kHostPrefix,
                                  features::kPageTransitionType)]);
  EXPECT_EQ(1.0,
            features[StringPrintf("%s%s",
                                  features::kHostPrefix,
                                  features::kIsFirstNavigation)]);
  // Make sure that we aren't adding redirect features since we are the first
  // redirect in a chain.
  EXPECT_EQ(0U,
            features.count(StringPrintf("%s%s",
                                        features::kRedirectPrefix,
                                        features::kPageTransitionType)));
  EXPECT_EQ(0U,
            features.count(StringPrintf("%s%s",
                                        features::kRedirectPrefix,
                                        features::kIsFirstNavigation)));

  request.Clear();
  request.set_url("http://www.bar.com/");
  request.set_client_score(0.5);
  NavigateAndCommit(GURL("http://www.bar.com/"),
                    GURL("http://www.foo.com/page.html"),
                    PageTransition::LINK |
                    PageTransition::CHAIN_END |
                    PageTransition::CLIENT_REDIRECT);

  EXPECT_TRUE(ExtractFeatures(&request));
  features.clear();
  GetFeatureMap(request, &features);

  EXPECT_EQ(1.0,
            features[StringPrintf("%s=%s",
                                  features::kReferrer,
                                  "http://www.foo.com/page.html")]);
  EXPECT_EQ(0.0, features[features::kHasSSLReferrer]);
  EXPECT_EQ(0.0, features[features::kPageTransitionType]);
  EXPECT_EQ(0.0, features[features::kIsFirstNavigation]);
  EXPECT_EQ(1.0,
            features[StringPrintf("%s%s=%s",
                                  features::kRedirectPrefix,
                                  features::kReferrer,
                                  "http://www.foo.com/")]);
  EXPECT_EQ(1.0,
            features[StringPrintf("%s%s",
                                  features::kRedirectPrefix,
                                  features::kPageTransitionType)]);
  EXPECT_EQ(0.0,
            features[StringPrintf("%s%s",
                                  features::kRedirectPrefix,
                                  features::kIsFirstNavigation)]);
  // Should not have host features.
  EXPECT_EQ(0U,
            features.count(StringPrintf("%s%s",
                                        features::kHostPrefix,
                                        features::kPageTransitionType)));
  EXPECT_EQ(0U,
            features.count(StringPrintf("%s%s",
                                        features::kHostPrefix,
                                        features::kIsFirstNavigation)));

  request.Clear();
  request.set_url("http://www.bar.com/other_page.html");
  request.set_client_score(0.5);
  NavigateAndCommit(GURL("http://www.bar.com/other_page.html"),
                    GURL("http://www.bar.com/"),
                    PageTransition::LINK);

  EXPECT_TRUE(ExtractFeatures(&request));
  features.clear();
  GetFeatureMap(request, &features);

  EXPECT_EQ(1.0,
            features[StringPrintf("%s=%s",
                                  features::kReferrer,
                                  "http://www.bar.com/")]);
  EXPECT_EQ(0.0, features[features::kHasSSLReferrer]);
  EXPECT_EQ(0.0, features[features::kPageTransitionType]);
  EXPECT_EQ(0.0, features[features::kIsFirstNavigation]);
  EXPECT_EQ(1.0,
            features[StringPrintf("%s%s%s=%s",
                                  features::kHostPrefix,
                                  features::kRedirectPrefix,
                                  features::kReferrer,
                                  "http://www.foo.com/")]);
  EXPECT_EQ(1.0,
            features[StringPrintf("%s%s%s",
                                  features::kHostPrefix,
                                  features::kRedirectPrefix,
                                  features::kPageTransitionType)]);
  EXPECT_EQ(0.0,
            features[StringPrintf("%s%s%s",
                                  features::kHostPrefix,
                                  features::kRedirectPrefix,
                                  features::kIsFirstNavigation)]);
  EXPECT_EQ(1.0,
            features[StringPrintf("%s%s=%s",
                                  features::kHostPrefix,
                                  features::kReferrer,
                                  "http://www.foo.com/page.html")]);
  EXPECT_EQ(0.0,
            features[StringPrintf("%s%s",
                                  features::kHostPrefix,
                                  features::kPageTransitionType)]);
  EXPECT_EQ(0.0,
            features[StringPrintf("%s%s",
                                  features::kHostPrefix,
                                  features::kIsFirstNavigation)]);
  // Should not have redirect features.
  EXPECT_EQ(0U,
            features.count(StringPrintf("%s%s",
                                        features::kRedirectPrefix,
                                        features::kPageTransitionType)));
  EXPECT_EQ(0U,
            features.count(StringPrintf("%s%s",
                                        features::kRedirectPrefix,
                                        features::kIsFirstNavigation)));
  request.Clear();
  request.set_url("http://www.baz.com/");
  request.set_client_score(0.5);
  NavigateAndCommit(GURL("http://www.baz.com"),
                    GURL("https://bankofamerica.com"),
                    PageTransition::GENERATED);

  browse_info_->ips.insert("193.5.163.8");
  browse_info_->ips.insert("23.94.78.1");
  EXPECT_CALL(*service_, IsBadIpAddress("193.5.163.8")).WillOnce(Return(true));
  EXPECT_CALL(*service_, IsBadIpAddress("23.94.78.1")).WillOnce(Return(false));

  EXPECT_TRUE(ExtractFeatures(&request));
  features.clear();
  GetFeatureMap(request, &features);

  EXPECT_EQ(1.0, features[features::kHasSSLReferrer]);
  EXPECT_EQ(5.0, features[features::kPageTransitionType]);
  // Should not have redirect or host features.
  EXPECT_EQ(0U,
            features.count(StringPrintf("%s%s",
                                        features::kRedirectPrefix,
                                        features::kPageTransitionType)));
  EXPECT_EQ(0U,
            features.count(StringPrintf("%s%s",
                                        features::kRedirectPrefix,
                                        features::kIsFirstNavigation)));
  EXPECT_EQ(0U,
            features.count(StringPrintf("%s%s",
                                        features::kHostPrefix,
                                        features::kPageTransitionType)));
  EXPECT_EQ(0U,
            features.count(StringPrintf("%s%s",
                                        features::kHostPrefix,
                                        features::kIsFirstNavigation)));
  EXPECT_EQ(5.0, features[features::kPageTransitionType]);
  EXPECT_EQ(1.0, features[std::string(features::kBadIpFetch) + "193.5.163.8"]);
  EXPECT_FALSE(features.count(std::string(features::kBadIpFetch) +
                              "23.94.78.1"));
}

TEST_F(BrowserFeatureExtractorTest, SafeBrowsingFeatures) {
  contents()->NavigateAndCommit(GURL("http://www.foo.com/malware.html"));
  ClientPhishingRequest request;
  request.set_url("http://www.foo.com/malware.html");
  request.set_client_score(0.5);

  browse_info_->unsafe_resource.reset(new SafeBrowsingService::UnsafeResource);
  browse_info_->unsafe_resource->url = GURL("http://www.malware.com/");
  browse_info_->unsafe_resource->original_url = GURL("http://www.good.com/");
  browse_info_->unsafe_resource->is_subresource = true;
  browse_info_->unsafe_resource->threat_type = SafeBrowsingService::URL_MALWARE;

  ExtractFeatures(&request);
  std::map<std::string, double> features;
  GetFeatureMap(request, &features);
  EXPECT_TRUE(features.count(StringPrintf("%s%s",
                                          features::kSafeBrowsingMaliciousUrl,
                                          "http://www.malware.com/")));
  EXPECT_TRUE(features.count(StringPrintf("%s%s",
                                          features::kSafeBrowsingOriginalUrl,
                                          "http://www.good.com/")));
  EXPECT_DOUBLE_EQ(1.0, features[features::kSafeBrowsingIsSubresource]);
  EXPECT_DOUBLE_EQ(2.0, features[features::kSafeBrowsingThreatType]);
}

TEST_F(BrowserFeatureExtractorTest, URLHashes) {
  ClientPhishingRequest request;
  request.set_url("http://host.com/");
  request.set_client_score(0.8f);

  history_service()->AddPage(GURL("http://host.com/"),
                             history::SOURCE_BROWSED);
  contents()->NavigateAndCommit(GURL("http://host.com/"));

  EXPECT_TRUE(ExtractFeatures(&request));
  EXPECT_EQ(crypto::SHA256HashString("host.com/").substr(
      0, BrowserFeatureExtractor::kHashPrefixLength),
            request.hash_prefix());

  request.set_url("http://www.host.com/path/");
  history_service()->AddPage(GURL("http://www.host.com/path/"),
                             history::SOURCE_BROWSED);
  contents()->NavigateAndCommit(GURL("http://www.host.com/path/"));

  EXPECT_TRUE(ExtractFeatures(&request));
  EXPECT_EQ(crypto::SHA256HashString("host.com/path/").substr(
      0, BrowserFeatureExtractor::kHashPrefixLength),
            request.hash_prefix());

  request.set_url("http://user@www.host.com:1111/path/123?args");
  history_service()->AddPage(
      GURL("http://user@www.host.com:1111/path/123?args"),
      history::SOURCE_BROWSED);
  contents()->NavigateAndCommit(
      GURL("http://user@www.host.com:1111/path/123?args"));

  EXPECT_TRUE(ExtractFeatures(&request));
  EXPECT_EQ(crypto::SHA256HashString("host.com/path/").substr(
      0, BrowserFeatureExtractor::kHashPrefixLength),
            request.hash_prefix());

  // Check that escaping matches the SafeBrowsing specification.
  request.set_url("http://www.host.com/A%21//B");
  history_service()->AddPage(GURL("http://www.host.com/A%21//B"),
                             history::SOURCE_BROWSED);
  contents()->NavigateAndCommit(GURL("http://www.host.com/A%21//B"));

  EXPECT_TRUE(ExtractFeatures(&request));
  EXPECT_EQ(crypto::SHA256HashString("host.com/a!/").substr(
      0, BrowserFeatureExtractor::kHashPrefixLength),
            request.hash_prefix());
}
}  // namespace safe_browsing
