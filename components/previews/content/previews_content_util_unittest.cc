// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_content_util.h"

#include <memory>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/common/previews_state.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace previews {

namespace {

class TestPreviewsDecider : public PreviewsDecider {
 public:
  TestPreviewsDecider() {}
  ~TestPreviewsDecider() override {}

  bool ShouldAllowPreviewAtECT(
      const net::URLRequest& request,
      PreviewsType type,
      net::EffectiveConnectionType effective_connection_type_threshold,
      const std::vector<std::string>& host_blacklist_from_server)
      const override {
    // For these tests, simply return whether client preview feature is enabled
    // or not (ignores ECT and blacklist considerations).
    switch (type) {
      case previews::PreviewsType::OFFLINE:
        return previews::params::IsOfflinePreviewsEnabled();
      case previews::PreviewsType::LOFI:
        return previews::params::IsClientLoFiEnabled();
      case previews::PreviewsType::AMP_REDIRECTION:
        return previews::params::IsAMPRedirectionPreviewEnabled();
      case previews::PreviewsType::NOSCRIPT:
        return previews::params::IsNoScriptPreviewsEnabled();
      case previews::PreviewsType::LITE_PAGE:
      case previews::PreviewsType::NONE:
      case previews::PreviewsType::LAST:
        break;
    }
    NOTREACHED();
    return false;
  }

  bool ShouldAllowPreview(const net::URLRequest& request,
                          PreviewsType type) const override {
    // Not used for these tests.
    NOTREACHED();
    return false;
  }
};

class PreviewsContentUtilTest : public testing::Test {
 public:
  PreviewsContentUtilTest() : previews_decider_(), context_() {}
  ~PreviewsContentUtilTest() override {}

  TestPreviewsDecider* previews_decider() { return &previews_decider_; }

  std::unique_ptr<net::URLRequest> CreateRequest() const {
    return CreateRequestWithURL(GURL("http://example.com"));
  }

  std::unique_ptr<net::URLRequest> CreateHttpsRequest() const {
    return CreateRequestWithURL(GURL("https://secure.example.com"));
  }

  std::unique_ptr<net::URLRequest> CreateRequestWithURL(const GURL& url) const {
    return context_.CreateRequest(url, net::DEFAULT_PRIORITY, nullptr,
                                  TRAFFIC_ANNOTATION_FOR_TESTS);
  }

 protected:
  // Needed for TestURLRequestContext.
  base::MessageLoopForIO loop_;

 private:
  TestPreviewsDecider previews_decider_;
  net::TestURLRequestContext context_;
};

TEST_F(PreviewsContentUtilTest, DetermineEnabledClientPreviewsStateClientLoFi) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("ClientLoFi", std::string());
  EXPECT_EQ(content::CLIENT_LOFI_ON,
            previews::DetermineEnabledClientPreviewsState(*CreateHttpsRequest(),
                                                          previews_decider()));
  EXPECT_EQ(content::CLIENT_LOFI_ON,
            previews::DetermineEnabledClientPreviewsState(*CreateRequest(),
                                                          previews_decider()));
}

TEST_F(PreviewsContentUtilTest,
       DetermineEnabledClientPreviewsStateNoScriptAndClientLoFi) {
  // First, verify no client previews enabled.
  EXPECT_EQ(content::PREVIEWS_UNSPECIFIED,
            previews::DetermineEnabledClientPreviewsState(*CreateHttpsRequest(),
                                                          previews_decider()));

  // Now, enable both Client LoFi and NoScript.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("ClientLoFi,NoScriptPreviews",
                                          std::string());

  // Verify NoScript takes precendence over LoFi (for https).
  EXPECT_EQ(content::NOSCRIPT_ON,
            previews::DetermineEnabledClientPreviewsState(*CreateHttpsRequest(),
                                                          previews_decider()));
  EXPECT_EQ(content::NOSCRIPT_ON, previews::DetermineEnabledClientPreviewsState(
                                      *CreateRequest(), previews_decider()));

  // Verify non-HTTP[S] URL has no previews enabled.
  std::unique_ptr<net::URLRequest> data_url_request(
      CreateRequestWithURL(GURL("data://someblob")));
  EXPECT_EQ(content::PREVIEWS_UNSPECIFIED,
            previews::DetermineEnabledClientPreviewsState(*data_url_request,
                                                          previews_decider()));
}

TEST_F(PreviewsContentUtilTest,
       DetermineCommittedClientPreviewsStateClientLoFi) {
  // Server bits take precendence over NoScript:
  EXPECT_EQ(content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON |
                content::CLIENT_LOFI_ON,
            previews::DetermineCommittedClientPreviewsState(
                *CreateHttpsRequest(),
                content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON |
                    content::CLIENT_LOFI_ON | content::NOSCRIPT_ON));

  // For HTTPS, NoScript has precendence over Client LoFi:
  EXPECT_EQ(content::NOSCRIPT_ON,
            previews::DetermineCommittedClientPreviewsState(
                *CreateHttpsRequest(),
                content::CLIENT_LOFI_ON | content::NOSCRIPT_ON));

  // HTTP allows Client LoFi:
  EXPECT_EQ(
      content::CLIENT_LOFI_ON,
      previews::DetermineCommittedClientPreviewsState(
          *CreateRequest(), content::CLIENT_LOFI_ON | content::NOSCRIPT_ON));
}

TEST_F(PreviewsContentUtilTest, GetMainFramePreviewsType) {
  // Simple cases:
  EXPECT_EQ(previews::PreviewsType::LITE_PAGE,
            previews::GetMainFramePreviewsType(content::SERVER_LITE_PAGE_ON));
  EXPECT_EQ(previews::PreviewsType::LOFI,
            previews::GetMainFramePreviewsType(content::SERVER_LOFI_ON));
  EXPECT_EQ(previews::PreviewsType::NOSCRIPT,
            previews::GetMainFramePreviewsType(content::NOSCRIPT_ON));
  EXPECT_EQ(previews::PreviewsType::LOFI,
            previews::GetMainFramePreviewsType(content::CLIENT_LOFI_ON));

  // NONE cases:
  EXPECT_EQ(previews::PreviewsType::NONE,
            previews::GetMainFramePreviewsType(content::PREVIEWS_UNSPECIFIED));
  EXPECT_EQ(previews::PreviewsType::NONE,
            previews::GetMainFramePreviewsType(content::PREVIEWS_NO_TRANSFORM));

  // Precedence cases:
  EXPECT_EQ(previews::PreviewsType::LITE_PAGE,
            previews::GetMainFramePreviewsType(
                content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON |
                content::NOSCRIPT_ON | content::CLIENT_LOFI_ON));
  EXPECT_EQ(previews::PreviewsType::LOFI,
            previews::GetMainFramePreviewsType(content::SERVER_LOFI_ON |
                                               content::NOSCRIPT_ON |
                                               content::CLIENT_LOFI_ON));
  EXPECT_EQ(previews::PreviewsType::NOSCRIPT,
            previews::GetMainFramePreviewsType(content::NOSCRIPT_ON |
                                               content::CLIENT_LOFI_ON));
}

}  // namespace

}  // namespace previews
