// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Note that although this is not a "browser" test, it runs as part of
// browser_tests.  This is because WebKit does not work properly if it is
// shutdown and re-initialized.  Since browser_tests runs each test in a
// new process, this avoids the problem.

#include "chrome/renderer/safe_browsing/phishing_dom_feature_extractor.h"

#include "base/callback.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "chrome/renderer/safe_browsing/features.h"
#include "chrome/renderer/safe_browsing/mock_feature_extractor_clock.h"
#include "chrome/renderer/safe_browsing/render_view_fake_resources_test.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::ContainerEq;
using ::testing::Return;

namespace safe_browsing {

class PhishingDOMFeatureExtractorTest : public RenderViewFakeResourcesTest {
 protected:
  virtual void SetUp() {
    // Set up WebKit and the RenderView.
    RenderViewFakeResourcesTest::SetUp();
    extractor_.reset(new PhishingDOMFeatureExtractor(view_, &clock_));
  }

  virtual void TearDown() {
    RenderViewFakeResourcesTest::TearDown();
  }

  // Runs the DOMFeatureExtractor on the RenderView, waiting for the
  // completion callback.  Returns the success boolean from the callback.
  bool ExtractFeatures(FeatureMap* features) {
    success_ = false;
    extractor_->ExtractFeatures(
        features,
        NewCallback(this, &PhishingDOMFeatureExtractorTest::ExtractionDone));
    message_loop_.Run();
    return success_;
  }

  // Completion callback for feature extraction.
  void ExtractionDone(bool success) {
    success_ = success;
    message_loop_.Quit();
  }

  MockFeatureExtractorClock clock_;
  scoped_ptr<PhishingDOMFeatureExtractor> extractor_;
  bool success_;  // holds the success value from ExtractFeatures
};

TEST_F(PhishingDOMFeatureExtractorTest, FormFeatures) {
  // This test doesn't exercise the extraction timing.
  EXPECT_CALL(clock_, Now()).WillRepeatedly(Return(base::TimeTicks::Now()));
  responses_["http://host.com/"] =
      "<html><head><body>"
      "<form action=\"query\"><input type=text><input type=checkbox></form>"
      "<form action=\"http://cgi.host.com/submit\"></form>"
      "<form action=\"http://other.com/\"></form>"
      "<form action=\"query\"></form>"
      "<form></form></body></html>";

  FeatureMap expected_features;
  expected_features.AddBooleanFeature(features::kPageHasForms);
  expected_features.AddRealFeature(features::kPageActionOtherDomainFreq, 0.25);
  expected_features.AddBooleanFeature(features::kPageHasTextInputs);
  expected_features.AddBooleanFeature(features::kPageHasCheckInputs);

  FeatureMap features;
  LoadURL("http://host.com/");
  ASSERT_TRUE(ExtractFeatures(&features));
  EXPECT_THAT(features.features(), ContainerEq(expected_features.features()));

  responses_["http://host.com/"] =
      "<html><head><body>"
      "<input type=\"radio\"><input type=password></body></html>";

  expected_features.Clear();
  expected_features.AddBooleanFeature(features::kPageHasRadioInputs);
  expected_features.AddBooleanFeature(features::kPageHasPswdInputs);

  features.Clear();
  LoadURL("http://host.com/");
  ASSERT_TRUE(ExtractFeatures(&features));
  EXPECT_THAT(features.features(), ContainerEq(expected_features.features()));

  responses_["http://host.com/"] =
      "<html><head><body><input></body></html>";

  expected_features.Clear();
  expected_features.AddBooleanFeature(features::kPageHasTextInputs);

  features.Clear();
  LoadURL("http://host.com/");
  ASSERT_TRUE(ExtractFeatures(&features));
  EXPECT_THAT(features.features(), ContainerEq(expected_features.features()));

  responses_["http://host.com/"] =
      "<html><head><body><input type=\"invalid\"></body></html>";

  expected_features.Clear();
  expected_features.AddBooleanFeature(features::kPageHasTextInputs);

  features.Clear();
  LoadURL("http://host.com/");
  ASSERT_TRUE(ExtractFeatures(&features));
  EXPECT_THAT(features.features(), ContainerEq(expected_features.features()));
}

TEST_F(PhishingDOMFeatureExtractorTest, LinkFeatures) {
  // This test doesn't exercise the extraction timing.
  EXPECT_CALL(clock_, Now()).WillRepeatedly(Return(base::TimeTicks::Now()));
  responses_["http://www.host.com/"] =
      "<html><head><body>"
      "<a href=\"http://www2.host.com/abc\">link</a>"
      "<a name=page_anchor></a>"
      "<a href=\"http://www.chromium.org/\">chromium</a>"
      "</body></html";

  FeatureMap expected_features;
  expected_features.AddRealFeature(features::kPageExternalLinksFreq, 0.5);
  expected_features.AddRealFeature(features::kPageSecureLinksFreq, 0.0);
  expected_features.AddBooleanFeature(features::kPageLinkDomain +
                                      std::string("chromium.org"));

  FeatureMap features;
  LoadURL("http://www.host.com/");
  ASSERT_TRUE(ExtractFeatures(&features));
  EXPECT_THAT(features.features(), ContainerEq(expected_features.features()));

  responses_.clear();
  responses_["https://www.host.com/"] =
      "<html><head><body>"
      "<a href=\"login\">this is secure</a>"
      "<a href=\"http://host.com\">not secure</a>"
      "<a href=\"https://www2.host.com/login\">also secure</a>"
      "<a href=\"http://chromium.org/\">also not secure</a>"
      "</body></html>";

  expected_features.Clear();
  expected_features.AddRealFeature(features::kPageExternalLinksFreq, 0.25);
  expected_features.AddRealFeature(features::kPageSecureLinksFreq, 0.5);
  expected_features.AddBooleanFeature(features::kPageLinkDomain +
                                      std::string("chromium.org"));

  features.Clear();
  LoadURL("https://www.host.com/");
  ASSERT_TRUE(ExtractFeatures(&features));
  EXPECT_THAT(features.features(), ContainerEq(expected_features.features()));
}

TEST_F(PhishingDOMFeatureExtractorTest, ScriptAndImageFeatures) {
  // This test doesn't exercise the extraction timing.
  EXPECT_CALL(clock_, Now()).WillRepeatedly(Return(base::TimeTicks::Now()));
  responses_["http://host.com/"] =
      "<html><head><script></script><script></script></head></html>";

  FeatureMap expected_features;
  expected_features.AddBooleanFeature(features::kPageNumScriptTagsGTOne);

  FeatureMap features;
  LoadURL("http://host.com/");
  ASSERT_TRUE(ExtractFeatures(&features));
  EXPECT_THAT(features.features(), ContainerEq(expected_features.features()));

  responses_["http://host.com/"] =
      "<html><head><script></script><script></script><script></script>"
      "<script></script><script></script><script></script><script></script>"
      "</head><body><img src=\"blah.gif\">"
      "<img src=\"http://host2.com/blah.gif\"></body></html>";

  expected_features.Clear();
  expected_features.AddBooleanFeature(features::kPageNumScriptTagsGTOne);
  expected_features.AddBooleanFeature(features::kPageNumScriptTagsGTSix);
  expected_features.AddRealFeature(features::kPageImgOtherDomainFreq, 0.5);

  features.Clear();
  LoadURL("http://host.com/");
  ASSERT_TRUE(ExtractFeatures(&features));
  EXPECT_THAT(features.features(), ContainerEq(expected_features.features()));
}

TEST_F(PhishingDOMFeatureExtractorTest, SubFrames) {
  // This test doesn't exercise the extraction timing.
  EXPECT_CALL(clock_, Now()).WillRepeatedly(Return(base::TimeTicks::Now()));

  // Test that features are aggregated across all frames.
  responses_["http://host.com/"] =
      "<html><body><input type=text><a href=\"info.html\">link</a>"
      "<iframe src=\"http://host2.com/\"></iframe>"
      "<iframe src=\"http://host3.com/\"></iframe>"
      "</body></html>";

  responses_["http://host2.com/"] =
      "<html><head><script></script><body>"
      "<form action=\"http://host4.com/\"><input type=checkbox></form>"
      "<form action=\"http://host2.com/submit\"></form>"
      "<a href=\"http://www.host2.com/home\">link</a>"
      "<iframe src=\"nested.html\"></iframe>"
      "<body></html>";

  responses_["http://host2.com/nested.html"] =
      "<html><body><input type=password>"
      "<a href=\"https://host4.com/\">link</a>"
      "<a href=\"relative\">another</a>"
      "</body></html>";

  responses_["http://host3.com/"] =
      "<html><head><script></script><body>"
      "<img src=\"http://host.com/123.png\">"
      "</body></html>";

  FeatureMap expected_features;
  expected_features.AddBooleanFeature(features::kPageHasForms);
  // Form action domains are compared to the URL of the document they're in,
  // not the URL of the toplevel page.  So http://host2.com/ has two form
  // actions, one of which is external.
  expected_features.AddRealFeature(features::kPageActionOtherDomainFreq, 0.5);
  expected_features.AddBooleanFeature(features::kPageHasTextInputs);
  expected_features.AddBooleanFeature(features::kPageHasPswdInputs);
  expected_features.AddBooleanFeature(features::kPageHasCheckInputs);
  expected_features.AddRealFeature(features::kPageExternalLinksFreq, 0.25);
  expected_features.AddBooleanFeature(features::kPageLinkDomain +
                                      std::string("host4.com"));
  expected_features.AddRealFeature(features::kPageSecureLinksFreq, 0.25);
  expected_features.AddBooleanFeature(features::kPageNumScriptTagsGTOne);
  expected_features.AddRealFeature(features::kPageImgOtherDomainFreq, 1.0);

  FeatureMap features;
  LoadURL("http://host.com/");
  ASSERT_TRUE(ExtractFeatures(&features));
  EXPECT_THAT(features.features(), ContainerEq(expected_features.features()));
}

TEST_F(PhishingDOMFeatureExtractorTest, Continuation) {
  // For this test, we'll cause the feature extraction to run multiple
  // iterations by incrementing the clock.

  // This page has a total of 50 elements.  For the external forms feature to
  // be computed correctly, the extractor has to examine the whole document.
  // Note: the empty HEAD is important -- WebKit will synthesize a HEAD if
  // there isn't one present, which can be confusing for the element counts.
  std::string response = "<html><head></head><body>"
      "<form action=\"ondomain\"></form>";
  for (int i = 0; i < 45; ++i) {
    response.append("<p>");
  }
  response.append("<form action=\"http://host2.com/\"></form></body></html>");
  responses_["http://host.com/"] = response;

  // Advance the clock 8 ms every 10 elements processed, 10 ms between chunks.
  // Note that this assumes kClockCheckGranularity = 10 and
  // kMaxTimePerChunkMs = 20.
  base::TimeTicks now = base::TimeTicks::Now();
  EXPECT_CALL(clock_, Now())
      // Time check at the start of extraction.
      .WillOnce(Return(now))
      // Time check at the start of the first chunk of work.
      .WillOnce(Return(now))
      // Time check after the first 10 elements.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(8)))
      // Time check after the next 10 elements.  This is over the chunk
      // time limit, so a continuation task will be posted.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(16)))
      // Time check at the start of the second chunk of work.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(26)))
      // Time check after resuming iteration for the second chunk.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(28)))
      // Time check after the next 10 elements.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(36)))
      // Time check after the next 10 elements.  This will trigger another
      // continuation task.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(44)))
      // Time check at the start of the third chunk of work.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(54)))
      // Time check after resuming iteration for the third chunk.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(56)))
      // Time check after the last 10 elements.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(64)))
      // A final time check for the histograms.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(66)));

  FeatureMap expected_features;
  expected_features.AddBooleanFeature(features::kPageHasForms);
  expected_features.AddRealFeature(features::kPageActionOtherDomainFreq, 0.5);

  FeatureMap features;
  LoadURL("http://host.com/");
  ASSERT_TRUE(ExtractFeatures(&features));
  EXPECT_THAT(features.features(), ContainerEq(expected_features.features()));
  // Make sure none of the mock expectations carry over to the next test.
  ::testing::Mock::VerifyAndClearExpectations(&clock_);

  // Now repeat the test with the same page, but advance the clock faster so
  // that the extraction time exceeds the maximum total time for the feature
  // extractor.  Extraction should fail.  Note that this assumes
  // kMaxTotalTimeMs = 500.
  EXPECT_CALL(clock_, Now())
      // Time check at the start of extraction.
      .WillOnce(Return(now))
      // Time check at the start of the first chunk of work.
      .WillOnce(Return(now))
      // Time check after the first 10 elements.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(300)))
      // Time check at the start of the second chunk of work.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(350)))
      // Time check after resuming iteration for the second chunk.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(360)))
      // Time check after the next 10 elements.  This is over the limit.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(600)))
      // A final time check for the histograms.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(620)));

  features.Clear();
  EXPECT_FALSE(ExtractFeatures(&features));
}

}  // namespace safe_browsing
