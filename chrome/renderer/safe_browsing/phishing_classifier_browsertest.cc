// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Note that although this is not a "browser" test, it runs as part of
// browser_tests.  This is because WebKit does not work properly if it is
// shutdown and re-initialized.  Since browser_tests runs each test in a
// new process, this avoids the problem.

#include "chrome/renderer/safe_browsing/phishing_classifier.h"

#include <string>

#include "base/scoped_ptr.h"
#include "base/sha2.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/renderer/safe_browsing/client_model.pb.h"
#include "chrome/renderer/safe_browsing/features.h"
#include "chrome/renderer/safe_browsing/mock_feature_extractor_clock.h"
#include "chrome/renderer/safe_browsing/render_view_fake_resources_test.h"
#include "chrome/renderer/safe_browsing/scorer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace safe_browsing {

class PhishingClassifierTest : public RenderViewFakeResourcesTest {
 protected:
  virtual void SetUp() {
    // Set up WebKit and the RenderView.
    RenderViewFakeResourcesTest::SetUp();

    // Construct a model to test with.  We include one feature from each of
    // the feature extractors, which allows us to verify that they all ran.
    ClientSideModel model;
    model.add_hashes(base::SHA256HashString(features::kUrlTldToken +
                                            std::string("net")));
    model.add_hashes(base::SHA256HashString(features::kPageLinkDomain +
                                            std::string("phishing.com")));
    model.add_hashes(base::SHA256HashString(features::kPageTerm +
                                            std::string("login")));
    model.add_hashes(base::SHA256HashString("login"));

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
    model.add_page_word(3);
    model.set_max_words_per_term(1);

    clock_ = new MockFeatureExtractorClock;
    scorer_.reset(Scorer::Create(model.SerializeAsString()));
    ASSERT_TRUE(scorer_.get());
    classifier_.reset(new PhishingClassifier(view_, scorer_.get(), clock_));
  }

  virtual void TearDown() {
    RenderViewFakeResourcesTest::TearDown();
  }

  // Helper method to start phishing classification and wait for it to
  // complete.  Returns the success value from the PhishingClassifier's
  // DoneCallback, and fills in phishy_score with the score.
  bool RunPhishingClassifier(const string16* page_text, double* phishy_score) {
    success_ = false;
    *phishy_score = PhishingClassifier::kInvalidScore;

    classifier_->BeginClassification(
        page_text,
        NewCallback(this, &PhishingClassifierTest::ClassificationFinished));
    message_loop_.Run();

    *phishy_score = phishy_score_;
    return success_;
  }

  // Completion callback for classification.
  void ClassificationFinished(bool success, double phishy_score) {
    success_ = success;
    phishy_score_ = phishy_score;
    message_loop_.Quit();
  }

  scoped_ptr<Scorer> scorer_;
  scoped_ptr<PhishingClassifier> classifier_;
  MockFeatureExtractorClock* clock_;  // owned by classifier_

  // These members hold the status from the most recent call to the
  // ClassificationFinished callback.
  bool success_;
  double phishy_score_;
};

TEST_F(PhishingClassifierTest, TestClassification) {
  // This test doesn't exercise the extraction timing.
  EXPECT_CALL(*clock_, Now())
      .WillRepeatedly(::testing::Return(base::TimeTicks::Now()));

  responses_["http://host.net/"] =
      "<html><body><a href=\"http://phishing.com/\">login</a></body></html>";
  LoadURL("http://host.net/");

  string16 page_text = ASCIIToUTF16("login");
  double phishy_score;
  EXPECT_TRUE(RunPhishingClassifier(&page_text, &phishy_score));
  EXPECT_DOUBLE_EQ(0.5, phishy_score);

  // Change the link domain to something non-phishy.
  responses_["http://host.net/"] =
      "<html><body><a href=\"http://safe.com/\">login</a></body></html>";
  LoadURL("http://host.net/");

  EXPECT_FALSE(RunPhishingClassifier(&page_text, &phishy_score));
  EXPECT_GE(phishy_score, 0.0);
  EXPECT_LT(phishy_score, 0.5);

  // Extraction should fail for this case, since there is no TLD.
  responses_["http://localhost/"] = "<html><body>content</body></html>";
  LoadURL("http://localhost/");
  EXPECT_FALSE(RunPhishingClassifier(&page_text, &phishy_score));
  EXPECT_EQ(phishy_score, PhishingClassifier::kInvalidScore);

  // Extraction should also fail for this case, because the URL is not http.
  responses_["https://host.net/"] = "<html><body>secure</body></html>";
  LoadURL("https://host.net/");
  EXPECT_FALSE(RunPhishingClassifier(&page_text, &phishy_score));
  EXPECT_EQ(phishy_score, PhishingClassifier::kInvalidScore);
}

}  // namespace safe_browsing
