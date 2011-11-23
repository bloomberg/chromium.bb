// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Note that although this is not a "browser" test, it runs as part of
// browser_tests.  This is because WebKit does not work properly if it is
// shutdown and re-initialized.  Since browser_tests runs each test in a
// new process, this avoids the problem.

#include "chrome/renderer/safe_browsing/phishing_classifier.h"

#include <string>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/safe_browsing/client_model.pb.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "chrome/renderer/safe_browsing/features.h"
#include "chrome/renderer/safe_browsing/mock_feature_extractor_clock.h"
#include "chrome/renderer/safe_browsing/murmurhash3_util.h"
#include "chrome/renderer/safe_browsing/scorer.h"
#include "content/test/render_view_fake_resources_test.h"
#include "crypto/sha2.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Not;
using ::testing::Pair;

namespace safe_browsing {

class PhishingClassifierTest : public RenderViewFakeResourcesTest {
 protected:
  PhishingClassifierTest()
      : url_tld_token_net_(features::kUrlTldToken + std::string("net")),
        page_link_domain_phishing_(features::kPageLinkDomain +
                                   std::string("phishing.com")),
        page_term_login_(features::kPageTerm + std::string("login")) {}

  virtual void SetUp() {
    // Set up WebKit and the RenderView.
    RenderViewFakeResourcesTest::SetUp();

    // Construct a model to test with.  We include one feature from each of
    // the feature extractors, which allows us to verify that they all ran.
    ClientSideModel model;

    model.add_hashes(crypto::SHA256HashString(url_tld_token_net_));
    model.add_hashes(crypto::SHA256HashString(page_link_domain_phishing_));
    model.add_hashes(crypto::SHA256HashString(page_term_login_));
    model.add_hashes(crypto::SHA256HashString("login"));
    model.add_hashes(crypto::SHA256HashString(features::kUrlTldToken +
                                              std::string("net")));
    model.add_hashes(crypto::SHA256HashString(features::kPageLinkDomain +
                                              std::string("phishing.com")));
    model.add_hashes(crypto::SHA256HashString(features::kPageTerm +
                                              std::string("login")));
    model.add_hashes(crypto::SHA256HashString("login"));

    // Add a default rule with a non-phishy weight.
    ClientSideModel::Rule* rule = model.add_rule();
    rule->set_weight(-1.0);

    // To give a phishy score, the total weight needs to be >= 0
    // (0.5 when converted to a probability).  This will only happen
    // if all of the listed features are present.
    rule = model.add_rule();
    rule->add_feature(0);
    rule->add_feature(1);
    rule->add_feature(2);
    rule->set_weight(1.0);

    model.add_page_term(3);
    model.set_murmur_hash_seed(2777808611U);
    model.add_page_word(MurmurHash3String("login", model.murmur_hash_seed()));
    model.set_max_words_per_term(1);

    clock_ = new MockFeatureExtractorClock;
    scorer_.reset(Scorer::Create(model.SerializeAsString()));
    ASSERT_TRUE(scorer_.get());
    classifier_.reset(new PhishingClassifier(view(), clock_));
  }

  virtual void TearDown() {
    RenderViewFakeResourcesTest::TearDown();
  }

  // Helper method to start phishing classification and wait for it to
  // complete.  Returns the true if the page is classified as phishy and
  // false otherwise.
  bool RunPhishingClassifier(const string16* page_text,
                             float* phishy_score,
                             FeatureMap* features) {
    verdict_.Clear();
    *phishy_score = PhishingClassifier::kInvalidScore;
    features->Clear();

    classifier_->BeginClassification(
        page_text,
        base::Bind(&PhishingClassifierTest::ClassificationFinished,
                   base::Unretained(this)));
    message_loop_.Run();

    *phishy_score = verdict_.client_score();
    for (int i = 0; i < verdict_.feature_map_size(); ++i) {
      features->AddRealFeature(verdict_.feature_map(i).name(),
                               verdict_.feature_map(i).value());
    }
    return verdict_.is_phishing();
  }

  // Completion callback for classification.
  void ClassificationFinished(const ClientPhishingRequest& verdict) {
    verdict_ = verdict;  // copy the verdict.
    message_loop_.Quit();
  }

  scoped_ptr<Scorer> scorer_;
  scoped_ptr<PhishingClassifier> classifier_;
  MockFeatureExtractorClock* clock_;  // owned by classifier_

  // Features that are in the model.
  const std::string url_tld_token_net_;
  const std::string page_link_domain_phishing_;
  const std::string page_term_login_;

  // This member holds the status from the most recent call to the
  // ClassificationFinished callback.
  ClientPhishingRequest verdict_;
};

TEST_F(PhishingClassifierTest, TestClassification) {
  // No scorer yet, so the classifier is not ready.
  EXPECT_FALSE(classifier_->is_ready());

  // Now set the scorer.
  classifier_->set_phishing_scorer(scorer_.get());
  EXPECT_TRUE(classifier_->is_ready());

  // This test doesn't exercise the extraction timing.
  EXPECT_CALL(*clock_, Now())
      .WillRepeatedly(::testing::Return(base::TimeTicks::Now()));

  responses_["http://host.net/"] =
      "<html><body><a href=\"http://phishing.com/\">login</a></body></html>";
  LoadURL("http://host.net/");

  string16 page_text = ASCIIToUTF16("login");
  float phishy_score;
  FeatureMap features;
  EXPECT_TRUE(RunPhishingClassifier(&page_text, &phishy_score, &features));
  // Note: features.features() might contain other features that simply aren't
  // in the model.
  EXPECT_THAT(features.features(),
              AllOf(Contains(Pair(url_tld_token_net_, 1.0)),
                    Contains(Pair(page_link_domain_phishing_, 1.0)),
                    Contains(Pair(page_term_login_, 1.0))));
  EXPECT_FLOAT_EQ(0.5, phishy_score);

  // Change the link domain to something non-phishy.
  responses_["http://host.net/"] =
      "<html><body><a href=\"http://safe.com/\">login</a></body></html>";
  LoadURL("http://host.net/");

  EXPECT_FALSE(RunPhishingClassifier(&page_text, &phishy_score, &features));
  EXPECT_THAT(features.features(),
              AllOf(Contains(Pair(url_tld_token_net_, 1.0)),
                    Contains(Pair(page_term_login_, 1.0))));
  EXPECT_THAT(features.features(),
              Not(Contains(Pair(page_link_domain_phishing_, 1.0))));
  EXPECT_GE(phishy_score, 0.0);
  EXPECT_LT(phishy_score, 0.5);

  // Extraction should fail for this case, since there is no TLD.
  responses_["http://localhost/"] = "<html><body>content</body></html>";
  LoadURL("http://localhost/");
  EXPECT_FALSE(RunPhishingClassifier(&page_text, &phishy_score, &features));
  EXPECT_EQ(0U, features.features().size());
  EXPECT_EQ(PhishingClassifier::kInvalidScore, phishy_score);

  // Extraction should also fail for this case, because the URL is not http.
  responses_["https://host.net/"] = "<html><body>secure</body></html>";
  LoadURL("https://host.net/");
  EXPECT_FALSE(RunPhishingClassifier(&page_text, &phishy_score, &features));
  EXPECT_EQ(0U, features.features().size());
  EXPECT_EQ(PhishingClassifier::kInvalidScore, phishy_score);

  // Extraction should fail for this case because the URL is a POST request.
  LoadURLWithPost("http://host.net/");
  EXPECT_FALSE(RunPhishingClassifier(&page_text, &phishy_score, &features));
  EXPECT_EQ(0U, features.features().size());
  EXPECT_EQ(PhishingClassifier::kInvalidScore, phishy_score);
}

TEST_F(PhishingClassifierTest, DisableDetection) {
  // No scorer yet, so the classifier is not ready.
  EXPECT_FALSE(classifier_->is_ready());

  // Now set the scorer.
  classifier_->set_phishing_scorer(scorer_.get());
  EXPECT_TRUE(classifier_->is_ready());

  // Set a NULL scorer, which turns detection back off.
  classifier_->set_phishing_scorer(NULL);
  EXPECT_FALSE(classifier_->is_ready());
}

}  // namespace safe_browsing
