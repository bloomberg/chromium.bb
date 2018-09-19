// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_hints.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/optimization_guide/optimization_guide_service_observer.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/previews/core/previews_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace previews {

class TestHostFilter : public previews::HostFilter {
 public:
  explicit TestHostFilter(std::string single_host_match)
      : HostFilter(nullptr), single_host_match_(single_host_match) {}

  bool ContainsHostSuffix(const GURL& url) const override {
    return single_host_match_ == url.host();
  }

 private:
  std::string single_host_match_;
};

class PreviewsHintsTest : public testing::Test {
 public:
  explicit PreviewsHintsTest() : previews_hints_(nullptr) {}

  ~PreviewsHintsTest() override {}

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void ParseConfig(const optimization_guide::proto::Configuration& config) {
    optimization_guide::ComponentInfo info(
        base::Version("1.0"),
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL("somefile.pb")));
    previews_hints_ = PreviewsHints::CreateFromConfig(config, info);
  }

  PreviewsHints* previews_hints() { return previews_hints_.get(); }

  bool HasLitePageRedirectBlacklist() {
    return previews_hints_->lite_page_redirect_blacklist_.get() != nullptr;
  }

 private:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<PreviewsHints> previews_hints_;
};

// NOTE: most of the PreviewsHints tests are still included in the tests for
// PreviewsOptimizationGuide.

TEST_F(PreviewsHintsTest, FindPageHintForSubstringPagePattern) {
  optimization_guide::proto::Hint hint1;

  // Page hint for "/one/"
  optimization_guide::proto::PageHint* page_hint1 = hint1.add_page_hints();
  page_hint1->set_page_pattern("foo.org/*/one/");

  // Page hint for "two"
  optimization_guide::proto::PageHint* page_hint2 = hint1.add_page_hints();
  page_hint2->set_page_pattern("two");

  // Page hint for "three.jpg"
  optimization_guide::proto::PageHint* page_hint3 = hint1.add_page_hints();
  page_hint3->set_page_pattern("three.jpg");

  EXPECT_EQ(nullptr, PreviewsHints::FindPageHint(GURL(""), hint1));
  EXPECT_EQ(nullptr,
            PreviewsHints::FindPageHint(GURL("https://www.foo.org/"), hint1));
  EXPECT_EQ(nullptr, PreviewsHints::FindPageHint(
                         GURL("https://www.foo.org/one"), hint1));

  EXPECT_EQ(nullptr, PreviewsHints::FindPageHint(
                         GURL("https://www.foo.org/one/"), hint1));
  EXPECT_EQ(page_hint1, PreviewsHints::FindPageHint(
                            GURL("https://www.foo.org/pages/one/"), hint1));
  EXPECT_EQ(page_hint1,
            PreviewsHints::FindPageHint(
                GURL("https://www.foo.org/pages/subpages/one/"), hint1));
  EXPECT_EQ(page_hint1, PreviewsHints::FindPageHint(
                            GURL("https://www.foo.org/pages/one/two"), hint1));
  EXPECT_EQ(page_hint1,
            PreviewsHints::FindPageHint(
                GURL("https://www.foo.org/pages/one/two/three.jpg"), hint1));

  EXPECT_EQ(page_hint2,
            PreviewsHints::FindPageHint(
                GURL("https://www.foo.org/pages/onetwo/three.jpg"), hint1));
  EXPECT_EQ(page_hint2,
            PreviewsHints::FindPageHint(
                GURL("https://www.foo.org/one/two/three.jpg"), hint1));
  EXPECT_EQ(page_hint2,
            PreviewsHints::FindPageHint(GURL("https://one.two.org"), hint1));

  EXPECT_EQ(page_hint3, PreviewsHints::FindPageHint(
                            GURL("https://www.foo.org/bar/three.jpg"), hint1));
}

TEST_F(PreviewsHintsTest, IsBlacklisted) {
  std::unique_ptr<PreviewsHints> previews_hints =
      PreviewsHints::CreateForTesting(
          std::make_unique<TestHostFilter>("black.com"));

  EXPECT_FALSE(previews_hints->IsBlacklisted(GURL("https://black.com/path"),
                                             PreviewsType::LOFI));
  EXPECT_TRUE(previews_hints->IsBlacklisted(GURL("https://black.com/path"),
                                            PreviewsType::LITE_PAGE_REDIRECT));
  EXPECT_FALSE(previews_hints->IsBlacklisted(GURL("https://nonblack.com"),
                                             PreviewsType::LITE_PAGE_REDIRECT));
}

TEST_F(PreviewsHintsTest, IsBlacklistedFromConfig) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kLitePageServerPreviews);
  BloomFilter blacklist_bloom_filter(7, 511);
  blacklist_bloom_filter.Add("black.com");
  std::string blacklist_data((char*)&blacklist_bloom_filter.bytes()[0],
                             blacklist_bloom_filter.bytes().size());
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::OptimizationFilter* blacklist_proto =
      config.add_optimization_blacklists();
  blacklist_proto->set_optimization_type(
      optimization_guide::proto::LITE_PAGE_REDIRECT);
  std::unique_ptr<optimization_guide::proto::BloomFilter> bloom_filter_proto =
      std::make_unique<optimization_guide::proto::BloomFilter>();
  bloom_filter_proto->set_num_hash_functions(7);
  bloom_filter_proto->set_num_bits(511);
  bloom_filter_proto->set_data(blacklist_data);
  blacklist_proto->set_allocated_bloom_filter(bloom_filter_proto.release());
  ParseConfig(config);
  EXPECT_TRUE(HasLitePageRedirectBlacklist());

  EXPECT_FALSE(previews_hints()->IsBlacklisted(GURL("https://black.com/path"),
                                               PreviewsType::LOFI));
  EXPECT_TRUE(previews_hints()->IsBlacklisted(
      GURL("https://black.com/path"), PreviewsType::LITE_PAGE_REDIRECT));
  EXPECT_TRUE(previews_hints()->IsBlacklisted(
      GURL("https://joe.black.com/path"), PreviewsType::LITE_PAGE_REDIRECT));
  EXPECT_FALSE(previews_hints()->IsBlacklisted(
      GURL("https://nonblack.com"), PreviewsType::LITE_PAGE_REDIRECT));
}

TEST_F(PreviewsHintsTest, ParseConfigWithInsufficientConfigDetails) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kLitePageServerPreviews);
  BloomFilter blacklist_bloom_filter(7, 511);
  blacklist_bloom_filter.Add("black.com");
  std::string blacklist_data((char*)&blacklist_bloom_filter.bytes()[0],
                             blacklist_bloom_filter.bytes().size());
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::OptimizationFilter* blacklist_proto =
      config.add_optimization_blacklists();
  blacklist_proto->set_optimization_type(
      optimization_guide::proto::LITE_PAGE_REDIRECT);
  std::unique_ptr<optimization_guide::proto::BloomFilter> bloom_filter_proto =
      std::make_unique<optimization_guide::proto::BloomFilter>();
  bloom_filter_proto->set_num_hash_functions(7);
  // Set num_bits to one more than the size of the data.
  bloom_filter_proto->set_num_bits(blacklist_data.size() * 8 + 1);
  bloom_filter_proto->set_data(blacklist_data);
  blacklist_proto->set_allocated_bloom_filter(bloom_filter_proto.release());
  ParseConfig(config);
  EXPECT_FALSE(HasLitePageRedirectBlacklist());
  histogram_tester.ExpectBucketCount(
      "Previews.OptimizationFilterStatus.LitePageRedirect",
      0 /* FOUND_SERVER_BLACKLIST_CONFIG */, 1);
  histogram_tester.ExpectBucketCount(
      "Previews.OptimizationFilterStatus.LitePageRedirect",
      2 /* FAILED_SERVER_BLACKLIST_BAD_CONFIG */, 1);

  EXPECT_FALSE(previews_hints()->IsBlacklisted(
      GURL("https://black.com/path"), PreviewsType::LITE_PAGE_REDIRECT));
}

TEST_F(PreviewsHintsTest, ParseConfigWithTooLargeBlacklist) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kLitePageServerPreviews);
  int too_many_bits =
      previews::params::LitePageRedirectPreviewMaxServerBlacklistByteSize() *
          8 +
      1;
  BloomFilter blacklist_bloom_filter(7, too_many_bits);
  blacklist_bloom_filter.Add("black.com");
  std::string blacklist_data((char*)&blacklist_bloom_filter.bytes()[0],
                             blacklist_bloom_filter.bytes().size());
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::OptimizationFilter* blacklist_proto =
      config.add_optimization_blacklists();
  blacklist_proto->set_optimization_type(
      optimization_guide::proto::LITE_PAGE_REDIRECT);
  std::unique_ptr<optimization_guide::proto::BloomFilter> bloom_filter_proto =
      std::make_unique<optimization_guide::proto::BloomFilter>();
  bloom_filter_proto->set_num_hash_functions(7);
  bloom_filter_proto->set_num_bits(too_many_bits);
  bloom_filter_proto->set_data(blacklist_data);
  blacklist_proto->set_allocated_bloom_filter(bloom_filter_proto.release());
  ParseConfig(config);
  EXPECT_FALSE(HasLitePageRedirectBlacklist());
  histogram_tester.ExpectBucketCount(
      "Previews.OptimizationFilterStatus.LitePageRedirect",
      0 /* FOUND_SERVER_BLACKLIST_CONFIG */, 1);
  histogram_tester.ExpectBucketCount(
      "Previews.OptimizationFilterStatus.LitePageRedirect",
      3 /* FAILED_SERVER_BLACKLIST_TOO_BIG */, 1);

  EXPECT_FALSE(previews_hints()->IsBlacklisted(
      GURL("https://black.com/path"), PreviewsType::LITE_PAGE_REDIRECT));
}

}  // namespace previews
