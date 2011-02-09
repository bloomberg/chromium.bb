// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Note: this test uses RenderViewFakeResourcesTest in order to set up a
// real RenderThread to hold the phishing Scorer object.

#include "chrome/renderer/safe_browsing/phishing_classifier_delegate.h"

#include "base/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/safe_browsing/features.h"
#include "chrome/renderer/safe_browsing/phishing_classifier.h"
#include "chrome/renderer/safe_browsing/render_view_fake_resources_test.h"
#include "chrome/renderer/safe_browsing/scorer.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"

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
 protected:
  bool OnMessageReceived(const IPC::Message& message) {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(PhishingClassifierDelegateTest, message)
      IPC_MESSAGE_HANDLER(ViewHostMsg_DetectedPhishingSite,
                          OnDetectedPhishingSite)
      IPC_MESSAGE_UNHANDLED(
          handled = RenderViewFakeResourcesTest::OnMessageReceived(message))
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  void OnDetectedPhishingSite(GURL phishing_url, double phishing_score) {
    detected_phishing_site_ = true;
    detected_url_ = phishing_url;
    detected_score_ = phishing_score;
    message_loop_.Quit();
  }

  // Runs the ClassificationDone callback, then waits for the
  // DetectedPhishingSite IPC to arrive.
  void RunClassificationDone(PhishingClassifierDelegate* delegate,
                             bool is_phishy, double phishy_score) {
    // Clear out any previous state.
    detected_phishing_site_ = false;
    detected_url_ = GURL();
    detected_score_ = -1.0;

    delegate->ClassificationDone(is_phishy, phishy_score);
    message_loop_.Run();
  }

  bool detected_phishing_site_;
  GURL detected_url_;
  double detected_score_;
};

TEST_F(PhishingClassifierDelegateTest, Navigation) {
  MockPhishingClassifier* classifier =
      new StrictMock<MockPhishingClassifier>(view_);
  PhishingClassifierDelegate* delegate =
      new PhishingClassifierDelegate(view_, classifier);
  MockScorer scorer;
  delegate->SetPhishingScorer(&scorer);
  ASSERT_TRUE(classifier->is_ready());

  // Test an initial load.  We expect classification to happen normally.
  EXPECT_CALL(*classifier, CancelPendingClassification()).Times(2);
  responses_["http://host.com/"] =
      "<html><body><iframe src=\"http://sub1.com/\"></iframe></body></html>";
  LoadURL("http://host.com/");
  Mock::VerifyAndClearExpectations(classifier);
  string16 page_text = ASCIIToUTF16("dummy");
  EXPECT_CALL(*classifier, BeginClassification(Pointee(page_text), _)).
      WillOnce(DeleteArg<1>());
  delegate->PageCaptured(page_text);
  Mock::VerifyAndClearExpectations(classifier);

  // Reloading the same page should not trigger a reclassification.
  // However, it will cancel any pending classification since the
  // content is being replaced.
  EXPECT_CALL(*classifier, CancelPendingClassification()).Times(2);
  GetMainFrame()->reload();
  message_loop_.Run();
  Mock::VerifyAndClearExpectations(classifier);
  delegate->PageCaptured(page_text);

  // Navigating in a subframe will increment the page id, but not change
  // the toplevel URL.  This should cancel pending classification since the
  // page content is changing, and not begin a new classification.
  EXPECT_CALL(*classifier, CancelPendingClassification());
  GetMainFrame()->firstChild()->loadRequest(
      WebKit::WebURLRequest(GURL("http://sub2.com/")));
  message_loop_.Run();
  Mock::VerifyAndClearExpectations(classifier);
  delegate->PageCaptured(page_text);

  // Scrolling to an anchor will increment the page id, but should not
  // not trigger a reclassification.  A pending classification should not
  // be cancelled, since the content is not changing.
  LoadURL("http://host.com/#foo");
  delegate->PageCaptured(page_text);

  // Now load a new toplevel page, which should trigger another classification.
  EXPECT_CALL(*classifier, CancelPendingClassification());
  LoadURL("http://host2.com/");
  Mock::VerifyAndClearExpectations(classifier);
  page_text = ASCIIToUTF16("dummy2");
  EXPECT_CALL(*classifier, BeginClassification(Pointee(page_text), _)).
      WillOnce(DeleteArg<1>());
  delegate->PageCaptured(page_text);
  Mock::VerifyAndClearExpectations(classifier);

  // The delegate will cancel pending classification on destruction.
  EXPECT_CALL(*classifier, CancelPendingClassification());
}

TEST_F(PhishingClassifierDelegateTest, PendingClassification) {
  // For this test, we'll create the delegate with no scorer available yet.
  MockPhishingClassifier* classifier =
      new StrictMock<MockPhishingClassifier>(view_);
  PhishingClassifierDelegate* delegate =
      new PhishingClassifierDelegate(view_, classifier);
  ASSERT_FALSE(classifier->is_ready());

  // Queue up a pending classification, cancel it, then queue up another one.
  LoadURL("http://host.com/");
  string16 page_text = ASCIIToUTF16("dummy");
  delegate->PageCaptured(page_text);

  LoadURL("http://host2.com/");
  page_text = ASCIIToUTF16("dummy2");
  delegate->PageCaptured(page_text);

  // Now set a scorer, which should cause a classifier to be created and
  // the classification to proceed.  Note that we need to reset |page_text|
  // since it is modified by the call to PageCaptured().
  page_text = ASCIIToUTF16("dummy2");
  EXPECT_CALL(*classifier, BeginClassification(Pointee(page_text), _)).
      WillOnce(DeleteArg<1>());
  MockScorer scorer;
  delegate->SetPhishingScorer(&scorer);
  Mock::VerifyAndClearExpectations(classifier);

  // The delegate will cancel pending classification on destruction.
  EXPECT_CALL(*classifier, CancelPendingClassification());
}

TEST_F(PhishingClassifierDelegateTest, PendingClassification_Ref) {
  // Similar to the last test, but navigates within the page before
  // setting the scorer.
  MockPhishingClassifier* classifier =
      new StrictMock<MockPhishingClassifier>(view_);
  PhishingClassifierDelegate* delegate =
      new PhishingClassifierDelegate(view_, classifier);
  ASSERT_FALSE(classifier->is_ready());

  // Queue up a pending classification, cancel it, then queue up another one.
  LoadURL("http://host.com/");
  string16 orig_page_text = ASCIIToUTF16("dummy");
  string16 page_text = orig_page_text;
  delegate->PageCaptured(page_text);

  LoadURL("http://host.com/#foo");
  page_text = orig_page_text;
  delegate->PageCaptured(page_text);

  // Now set a scorer, which should cause a classifier to be created and
  // the classification to proceed.
  EXPECT_CALL(*classifier, BeginClassification(Pointee(orig_page_text), _)).
      WillOnce(DeleteArg<1>());
  MockScorer scorer;
  delegate->SetPhishingScorer(&scorer);
  Mock::VerifyAndClearExpectations(classifier);

  // The delegate will cancel pending classification on destruction.
  EXPECT_CALL(*classifier, CancelPendingClassification());
}

TEST_F(PhishingClassifierDelegateTest, DetectedPhishingSite) {
  // Tests that a DetectedPhishingSite IPC is sent to the browser
  // if a site comes back as phishy.
  MockPhishingClassifier* classifier =
      new StrictMock<MockPhishingClassifier>(view_);
  PhishingClassifierDelegate* delegate =
      new PhishingClassifierDelegate(view_, classifier);
  MockScorer scorer;
  delegate->SetPhishingScorer(&scorer);
  ASSERT_TRUE(classifier->is_ready());

  // Start by loading a page to populate the delegate's state.
  responses_["http://host.com/"] = "<html><body>phish</body></html>";
  EXPECT_CALL(*classifier, CancelPendingClassification());
  LoadURL("http://host.com/");
  Mock::VerifyAndClearExpectations(classifier);
  string16 page_text = ASCIIToUTF16("phish");
  EXPECT_CALL(*classifier, BeginClassification(Pointee(page_text), _)).
      WillOnce(DeleteArg<1>());
  delegate->PageCaptured(page_text);
  Mock::VerifyAndClearExpectations(classifier);

  // Now run the callback to simulate the classifier finishing.
  RunClassificationDone(delegate, true, 0.8);
  EXPECT_TRUE(detected_phishing_site_);
  EXPECT_EQ(GURL("http://host.com/"), detected_url_);
  EXPECT_EQ(0.8, detected_score_);

  // The delegate will cancel pending classification on destruction.
  EXPECT_CALL(*classifier, CancelPendingClassification());
}

}  // namespace safe_browsing
