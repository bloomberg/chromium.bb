// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Note: this test uses RenderViewFakeResourcesTest in order to set up a
// real RenderThread to hold the phishing Scorer object.

#include "chrome/renderer/safe_browsing/phishing_classifier_delegate.h"

#include "base/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/safe_browsing/features.h"
#include "chrome/renderer/safe_browsing/phishing_classifier.h"
#include "chrome/renderer/safe_browsing/render_view_fake_resources_test.h"
#include "chrome/renderer/safe_browsing/scorer.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLRequest.h"

using ::testing::_;
using ::testing::DeleteArg;
using ::testing::Mock;
using ::testing::Pointee;
using ::testing::StrictMock;

namespace safe_browsing {

namespace {
class MockPhishingClassifier : public PhishingClassifier {
 public:
  explicit MockPhishingClassifier(RenderView* render_view)
      : PhishingClassifier(render_view, NULL /* clock */) {}

  virtual ~MockPhishingClassifier() {}

  MOCK_METHOD2(BeginClassification, void(const string16*, DoneCallback*));
  MOCK_METHOD0(CancelPendingClassification, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPhishingClassifier);
};

class MockScorer : public Scorer {
 public:
  MockScorer() : Scorer() {}
  virtual ~MockScorer() {}

  MOCK_METHOD1(ComputeScore, double(const FeatureMap&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockScorer);
};
}  // namespace

class PhishingClassifierDelegateTest : public RenderViewFakeResourcesTest {
};

TEST_F(PhishingClassifierDelegateTest, Navigation) {
  MockPhishingClassifier* classifier =
      new StrictMock<MockPhishingClassifier>(view_);
  PhishingClassifierDelegate delegate(view_, classifier);
  MockScorer scorer;
  delegate.SetPhishingScorer(&scorer);
  ASSERT_TRUE(classifier->is_ready());

  // Test an initial load.  We expect classification to happen normally.
  responses_["http://host.com/"] =
      "<html><body><iframe src=\"http://sub1.com/\"></iframe></body></html>";
  LoadURL("http://host.com/");
  WebKit::WebFrame* child_frame = GetMainFrame()->firstChild();
  string16 page_text = ASCIIToUTF16("dummy");
  EXPECT_CALL(*classifier, CancelPendingClassification()).Times(2);
  delegate.CommittedLoadInFrame(GetMainFrame());
  delegate.CommittedLoadInFrame(child_frame);
  Mock::VerifyAndClearExpectations(classifier);
  EXPECT_CALL(*classifier, BeginClassification(Pointee(page_text), _)).
      WillOnce(DeleteArg<1>());
  delegate.FinishedLoad(&page_text);
  Mock::VerifyAndClearExpectations(classifier);

  // Reloading the same page should not trigger a reclassification.
  // However, it will cancel any pending classification since the
  // content is being replaced.
  EXPECT_CALL(*classifier, CancelPendingClassification());
  delegate.CommittedLoadInFrame(GetMainFrame());
  Mock::VerifyAndClearExpectations(classifier);
  delegate.FinishedLoad(&page_text);

  // Navigating in a subframe will increment the page id, but not change
  // the toplevel URL.  This should cancel pending classification since the
  // page content is changing, and not begin a new classification.
  child_frame->loadRequest(WebKit::WebURLRequest(GURL("http://sub2.com/")));
  message_loop_.Run();
  EXPECT_CALL(*classifier, CancelPendingClassification());
  delegate.CommittedLoadInFrame(child_frame);
  Mock::VerifyAndClearExpectations(classifier);
  delegate.FinishedLoad(&page_text);

  // Scrolling to an anchor will increment the page id, but should not
  // not trigger a reclassification.  A pending classification should not
  // be cancelled, since the content is not changing.
  LoadURL("http://host.com/#foo");
  delegate.CommittedLoadInFrame(GetMainFrame());
  delegate.FinishedLoad(&page_text);

  // Now load a new toplevel page, which should trigger another classification.
  LoadURL("http://host2.com/");
  page_text = ASCIIToUTF16("dummy2");
  EXPECT_CALL(*classifier, CancelPendingClassification());
  delegate.CommittedLoadInFrame(GetMainFrame());
  Mock::VerifyAndClearExpectations(classifier);
  EXPECT_CALL(*classifier, BeginClassification(Pointee(page_text), _)).
      WillOnce(DeleteArg<1>());
  delegate.FinishedLoad(&page_text);
  Mock::VerifyAndClearExpectations(classifier);

  // The delegate will cancel pending classification on destruction.
  EXPECT_CALL(*classifier, CancelPendingClassification());
}

TEST_F(PhishingClassifierDelegateTest, PendingClassification) {
  // For this test, we'll create the delegate with no scorer available yet.
  MockPhishingClassifier* classifier =
      new StrictMock<MockPhishingClassifier>(view_);
  PhishingClassifierDelegate delegate(view_, classifier);
  ASSERT_FALSE(classifier->is_ready());

  // Queue up a pending classification, cancel it, then queue up another one.
  LoadURL("http://host.com/");
  string16 page_text = ASCIIToUTF16("dummy");
  delegate.CommittedLoadInFrame(GetMainFrame());
  delegate.FinishedLoad(&page_text);

  LoadURL("http://host2.com/");
  delegate.CommittedLoadInFrame(GetMainFrame());
  page_text = ASCIIToUTF16("dummy2");
  delegate.FinishedLoad(&page_text);

  // Now set a scorer, which should cause a classifier to be created and
  // the classification to proceed.  Note that we need to reset |page_text|
  // since it is modified by the call to FinishedLoad().
  page_text = ASCIIToUTF16("dummy2");
  EXPECT_CALL(*classifier, BeginClassification(Pointee(page_text), _)).
      WillOnce(DeleteArg<1>());
  MockScorer scorer;
  delegate.SetPhishingScorer(&scorer);
  Mock::VerifyAndClearExpectations(classifier);

  // The delegate will cancel pending classification on destruction.
  EXPECT_CALL(*classifier, CancelPendingClassification());
}

TEST_F(PhishingClassifierDelegateTest, PendingClassification_Ref) {
  // Similar to the last test, but navigates within the page before
  // setting the scorer.
  MockPhishingClassifier* classifier =
      new StrictMock<MockPhishingClassifier>(view_);
  PhishingClassifierDelegate delegate(view_, classifier);
  ASSERT_FALSE(classifier->is_ready());

  // Queue up a pending classification, cancel it, then queue up another one.
  LoadURL("http://host.com/");
  delegate.CommittedLoadInFrame(GetMainFrame());
  string16 orig_page_text = ASCIIToUTF16("dummy");
  string16 page_text = orig_page_text;
  delegate.FinishedLoad(&page_text);

  LoadURL("http://host.com/#foo");
  page_text = orig_page_text;
  delegate.CommittedLoadInFrame(GetMainFrame());
  delegate.FinishedLoad(&page_text);

  // Now set a scorer, which should cause a classifier to be created and
  // the classification to proceed.
  EXPECT_CALL(*classifier, BeginClassification(Pointee(orig_page_text), _)).
      WillOnce(DeleteArg<1>());
  MockScorer scorer;
  delegate.SetPhishingScorer(&scorer);
  Mock::VerifyAndClearExpectations(classifier);

  // The delegate will cancel pending classification on destruction.
  EXPECT_CALL(*classifier, CancelPendingClassification());
}

}  // namespace safe_browsing
