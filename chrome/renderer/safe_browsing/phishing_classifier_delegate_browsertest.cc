// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Note: this test uses RenderViewFakeResourcesTest in order to set up a
// real RenderThread to hold the phishing Scorer object.

#include "chrome/renderer/safe_browsing/phishing_classifier_delegate.h"

#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "chrome/common/safe_browsing/safebrowsing_messages.h"
#include "chrome/renderer/safe_browsing/features.h"
#include "chrome/renderer/safe_browsing/phishing_classifier.h"
#include "chrome/renderer/safe_browsing/scorer.h"
#include "content/public/renderer/render_view.h"
#include "content/test/render_view_fake_resources_test.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebHistoryItem.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLRequest.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Pointee;
using ::testing::StrictMock;

namespace safe_browsing {

namespace {
class MockPhishingClassifier : public PhishingClassifier {
 public:
  explicit MockPhishingClassifier(content::RenderView* render_view)
      : PhishingClassifier(render_view, NULL /* clock */) {}

  virtual ~MockPhishingClassifier() {}

  MOCK_METHOD2(BeginClassification, void(const string16*, const DoneCallback&));
  MOCK_METHOD0(CancelPendingClassification, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPhishingClassifier);
};

class MockScorer : public Scorer {
 public:
  MockScorer() : Scorer() {}
  virtual ~MockScorer() {}

  MOCK_CONST_METHOD1(ComputeScore, double(const FeatureMap&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockScorer);
};
}  // namespace

class PhishingClassifierDelegateTest : public RenderViewFakeResourcesTest {
 protected:
  bool OnMessageReceived(const IPC::Message& message) {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(PhishingClassifierDelegateTest, message)
        IPC_MESSAGE_HANDLER(SafeBrowsingHostMsg_PhishingDetectionDone,
                            OnPhishingDetectionDone)
      IPC_MESSAGE_UNHANDLED(
          handled = RenderViewFakeResourcesTest::OnMessageReceived(message))
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  void OnPhishingDetectionDone(const std::string& verdict_str) {
    scoped_ptr<ClientPhishingRequest> verdict(new ClientPhishingRequest);
    if (verdict->ParseFromString(verdict_str) &&
        verdict->IsInitialized()) {
      verdict_.swap(verdict);
    }
    message_loop_.Quit();
  }

  // Runs the ClassificationDone callback, then waits for the
  // PhishingDetectionDone IPC to arrive.
  void RunClassificationDone(PhishingClassifierDelegate* delegate,
                             const ClientPhishingRequest& verdict) {
    // Clear out any previous state.
    verdict_.reset();
    delegate->ClassificationDone(verdict);
    message_loop_.Run();
  }

  void OnStartPhishingDetection(PhishingClassifierDelegate* delegate,
                                const GURL& url) {
    delegate->OnStartPhishingDetection(url);
  }

  scoped_ptr<ClientPhishingRequest> verdict_;
};

TEST_F(PhishingClassifierDelegateTest, DISABLED_Navigation) {
  MockPhishingClassifier* classifier =
      new StrictMock<MockPhishingClassifier>(view());
  PhishingClassifierDelegate* delegate =
      PhishingClassifierDelegate::Create(view(), classifier);
  MockScorer scorer;
  delegate->SetPhishingScorer(&scorer);
  ASSERT_TRUE(classifier->is_ready());

  // Test an initial load.  We expect classification to happen normally.
  EXPECT_CALL(*classifier, CancelPendingClassification()).Times(2);
  responses_["http://host.com/"] =
      "<html><body><iframe src=\"http://sub1.com/\"></iframe></body></html>";
  LoadURL("http://host.com/");
  Mock::VerifyAndClearExpectations(classifier);
  OnStartPhishingDetection(delegate, GURL("http://host.com/"));
  string16 page_text = ASCIIToUTF16("dummy");
  {
    InSequence s;
    EXPECT_CALL(*classifier, CancelPendingClassification());
    EXPECT_CALL(*classifier, BeginClassification(Pointee(page_text), _));
    delegate->PageCaptured(&page_text, false);
    Mock::VerifyAndClearExpectations(classifier);
  }

  // Reloading the same page should not trigger a reclassification.
  // However, it will cancel any pending classification since the
  // content is being replaced.
  EXPECT_CALL(*classifier, CancelPendingClassification()).Times(2);
  GetMainFrame()->reload();
  message_loop_.Run();
  Mock::VerifyAndClearExpectations(classifier);
  OnStartPhishingDetection(delegate, GURL("http://host.com/"));
  page_text = ASCIIToUTF16("dummy");
  EXPECT_CALL(*classifier, CancelPendingClassification());
  delegate->PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier);

  // Navigating in a subframe will not change the toplevel URL.  However, this
  // should cancel pending classification since the page content is changing.
  // Currently, we do not start a new classification after subframe loads.
  EXPECT_CALL(*classifier, CancelPendingClassification());
  GetMainFrame()->firstChild()->loadRequest(
      WebKit::WebURLRequest(GURL("http://sub2.com/")));
  message_loop_.Run();
  Mock::VerifyAndClearExpectations(classifier);
  OnStartPhishingDetection(delegate, GURL("http://host.com/"));
  page_text = ASCIIToUTF16("dummy");
  EXPECT_CALL(*classifier, CancelPendingClassification());
  delegate->PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier);

  // Scrolling to an anchor works similarly to a subframe navigation, but
  // see the TODO in PhishingClassifierDelegate::DidCommitProvisionalLoad.
  EXPECT_CALL(*classifier, CancelPendingClassification());
  LoadURL("http://host.com/#foo");
  Mock::VerifyAndClearExpectations(classifier);
  OnStartPhishingDetection(delegate, GURL("http://host.com/#foo"));
  page_text = ASCIIToUTF16("dummy");
  EXPECT_CALL(*classifier, CancelPendingClassification());
  delegate->PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier);

  // Now load a new toplevel page, which should trigger another classification.
  EXPECT_CALL(*classifier, CancelPendingClassification());
  LoadURL("http://host2.com/");
  Mock::VerifyAndClearExpectations(classifier);
  page_text = ASCIIToUTF16("dummy2");
  OnStartPhishingDetection(delegate, GURL("http://host2.com/"));
  {
    InSequence s;
    EXPECT_CALL(*classifier, CancelPendingClassification());
    EXPECT_CALL(*classifier, BeginClassification(Pointee(page_text), _));
    delegate->PageCaptured(&page_text, false);
    Mock::VerifyAndClearExpectations(classifier);
  }

  // No classification should happen on back/forward navigation.
  // Note: in practice, the browser will not send a StartPhishingDetection IPC
  // in this case.  However, we want to make sure that the delegate behaves
  // correctly regardless.
  WebKit::WebHistoryItem forward_item = GetMainFrame()->currentHistoryItem();
  EXPECT_CALL(*classifier, CancelPendingClassification());
  GoBack();
  Mock::VerifyAndClearExpectations(classifier);
  page_text = ASCIIToUTF16("dummy");
  OnStartPhishingDetection(delegate, GURL("http://host.com/#foo"));
  EXPECT_CALL(*classifier, CancelPendingClassification());
  delegate->PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier);

  EXPECT_CALL(*classifier, CancelPendingClassification());
  GoForward(forward_item);
  Mock::VerifyAndClearExpectations(classifier);
  page_text = ASCIIToUTF16("dummy2");
  OnStartPhishingDetection(delegate, GURL("http://host2.com/"));
  EXPECT_CALL(*classifier, CancelPendingClassification());
  delegate->PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier);

  // Now go back again and scroll to a different anchor.
  // No classification should happen.
  EXPECT_CALL(*classifier, CancelPendingClassification());
  GoBack();
  Mock::VerifyAndClearExpectations(classifier);
  page_text = ASCIIToUTF16("dummy");
  OnStartPhishingDetection(delegate, GURL("http://host.com/#foo"));
  EXPECT_CALL(*classifier, CancelPendingClassification());
  delegate->PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier);

  EXPECT_CALL(*classifier, CancelPendingClassification());
  LoadURL("http://host.com/#foo2");
  Mock::VerifyAndClearExpectations(classifier);
  OnStartPhishingDetection(delegate, GURL("http://host.com/#foo2"));
  page_text = ASCIIToUTF16("dummy");
  EXPECT_CALL(*classifier, CancelPendingClassification());
  delegate->PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier);

  // The delegate will cancel pending classification on destruction.
  EXPECT_CALL(*classifier, CancelPendingClassification());
}

TEST_F(PhishingClassifierDelegateTest, DISABLED_NoScorer) {
  // For this test, we'll create the delegate with no scorer available yet.
  MockPhishingClassifier* classifier =
      new StrictMock<MockPhishingClassifier>(view());
  PhishingClassifierDelegate* delegate =
      PhishingClassifierDelegate::Create(view(), classifier);
  ASSERT_FALSE(classifier->is_ready());

  // Queue up a pending classification, cancel it, then queue up another one.
  LoadURL("http://host.com/");
  string16 page_text = ASCIIToUTF16("dummy");
  OnStartPhishingDetection(delegate, GURL("http://host.com/"));
  delegate->PageCaptured(&page_text, false);

  LoadURL("http://host2.com/");
  page_text = ASCIIToUTF16("dummy2");
  OnStartPhishingDetection(delegate, GURL("http://host2.com/"));
  delegate->PageCaptured(&page_text, false);

  // Now set a scorer, which should cause a classifier to be created and
  // the classification to proceed.
  page_text = ASCIIToUTF16("dummy2");
  EXPECT_CALL(*classifier, BeginClassification(Pointee(page_text), _));
  MockScorer scorer;
  delegate->SetPhishingScorer(&scorer);
  Mock::VerifyAndClearExpectations(classifier);

  // If we set a new scorer while a classification is going on the
  // classification should be cancelled.
  EXPECT_CALL(*classifier, CancelPendingClassification());
  delegate->SetPhishingScorer(&scorer);
  Mock::VerifyAndClearExpectations(classifier);

  // The delegate will cancel pending classification on destruction.
  EXPECT_CALL(*classifier, CancelPendingClassification());
}

TEST_F(PhishingClassifierDelegateTest, DISABLED_NoScorer_Ref) {
  // Similar to the last test, but navigates within the page before
  // setting the scorer.
  MockPhishingClassifier* classifier =
      new StrictMock<MockPhishingClassifier>(view());
  PhishingClassifierDelegate* delegate =
      PhishingClassifierDelegate::Create(view(), classifier);
  ASSERT_FALSE(classifier->is_ready());

  // Queue up a pending classification, cancel it, then queue up another one.
  LoadURL("http://host.com/");
  string16 page_text = ASCIIToUTF16("dummy");
  OnStartPhishingDetection(delegate, GURL("http://host.com/"));
  delegate->PageCaptured(&page_text, false);

  LoadURL("http://host.com/#foo");
  OnStartPhishingDetection(delegate, GURL("http://host.com/#foo"));
  page_text = ASCIIToUTF16("dummy");
  delegate->PageCaptured(&page_text, false);

  // Now set a scorer, which should cause a classifier to be created and
  // the classification to proceed.
  page_text = ASCIIToUTF16("dummy");
  EXPECT_CALL(*classifier, BeginClassification(Pointee(page_text), _));
  MockScorer scorer;
  delegate->SetPhishingScorer(&scorer);
  Mock::VerifyAndClearExpectations(classifier);

  // The delegate will cancel pending classification on destruction.
  EXPECT_CALL(*classifier, CancelPendingClassification());
}

TEST_F(PhishingClassifierDelegateTest, DISABLED_NoStartPhishingDetection) {
  // Tests the behavior when OnStartPhishingDetection has not yet been called
  // when the page load finishes.
  MockPhishingClassifier* classifier =
      new StrictMock<MockPhishingClassifier>(view());
  PhishingClassifierDelegate* delegate =
      PhishingClassifierDelegate::Create(view(), classifier);
  MockScorer scorer;
  delegate->SetPhishingScorer(&scorer);
  ASSERT_TRUE(classifier->is_ready());

  EXPECT_CALL(*classifier, CancelPendingClassification());
  responses_["http://host.com/"] = "<html><body>phish</body></html>";
  LoadURL("http://host.com/");
  Mock::VerifyAndClearExpectations(classifier);
  string16 page_text = ASCIIToUTF16("phish");
  EXPECT_CALL(*classifier, CancelPendingClassification());
  delegate->PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier);
  // Now simulate the StartPhishingDetection IPC.  We expect classification
  // to begin.
  page_text = ASCIIToUTF16("phish");
  EXPECT_CALL(*classifier, BeginClassification(Pointee(page_text), _));
  OnStartPhishingDetection(delegate, GURL("http://host.com/"));
  Mock::VerifyAndClearExpectations(classifier);

  // Now try again, but this time we will navigate the page away before
  // the IPC is sent.
  EXPECT_CALL(*classifier, CancelPendingClassification());
  responses_["http://host2.com/"] = "<html><body>phish</body></html>";
  LoadURL("http://host2.com/");
  Mock::VerifyAndClearExpectations(classifier);
  page_text = ASCIIToUTF16("phish");
  EXPECT_CALL(*classifier, CancelPendingClassification());
  delegate->PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier);

  EXPECT_CALL(*classifier, CancelPendingClassification());
  responses_["http://host3.com/"] = "<html><body>phish</body></html>";
  LoadURL("http://host3.com/");
  Mock::VerifyAndClearExpectations(classifier);
  OnStartPhishingDetection(delegate, GURL("http://host2.com/"));

  // In this test, the original page is a redirect, which we do not get a
  // StartPhishingDetection IPC for.  We use location.replace() to load a
  // new page while reusing the original session history entry, and check that
  // classification begins correctly for the landing page.
  EXPECT_CALL(*classifier, CancelPendingClassification());
  responses_["http://host4.com/"] = "<html><body>abc</body></html>";
  LoadURL("http://host4.com/");
  Mock::VerifyAndClearExpectations(classifier);
  page_text = ASCIIToUTF16("abc");
  EXPECT_CALL(*classifier, CancelPendingClassification());
  delegate->PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier);
  responses_["http://host4.com/redir"] = "<html><body>123</body></html>";
  EXPECT_CALL(*classifier, CancelPendingClassification());
  LoadURL("javascript:location.replace(\'redir\');");
  Mock::VerifyAndClearExpectations(classifier);
  OnStartPhishingDetection(delegate, GURL("http://host4.com/redir"));
  page_text = ASCIIToUTF16("123");
  {
    InSequence s;
    EXPECT_CALL(*classifier, CancelPendingClassification());
    EXPECT_CALL(*classifier, BeginClassification(Pointee(page_text), _));
    delegate->PageCaptured(&page_text, false);
    Mock::VerifyAndClearExpectations(classifier);
  }

  // The delegate will cancel pending classification on destruction.
  EXPECT_CALL(*classifier, CancelPendingClassification());
}

TEST_F(PhishingClassifierDelegateTest, DISABLED_IgnorePreliminaryCapture) {
  // Tests that preliminary PageCaptured notifications are ignored.
  MockPhishingClassifier* classifier =
      new StrictMock<MockPhishingClassifier>(view());
  PhishingClassifierDelegate* delegate =
      PhishingClassifierDelegate::Create(view(), classifier);
  MockScorer scorer;
  delegate->SetPhishingScorer(&scorer);
  ASSERT_TRUE(classifier->is_ready());

  EXPECT_CALL(*classifier, CancelPendingClassification());
  responses_["http://host.com/"] = "<html><body>phish</body></html>";
  LoadURL("http://host.com/");
  Mock::VerifyAndClearExpectations(classifier);
  OnStartPhishingDetection(delegate, GURL("http://host.com/"));
  string16 page_text = ASCIIToUTF16("phish");
  delegate->PageCaptured(&page_text, true);

  // Once the non-preliminary capture happens, classification should begin.
  page_text = ASCIIToUTF16("phish");
  {
    InSequence s;
    EXPECT_CALL(*classifier, CancelPendingClassification());
    EXPECT_CALL(*classifier, BeginClassification(Pointee(page_text), _));
    delegate->PageCaptured(&page_text, false);
    Mock::VerifyAndClearExpectations(classifier);
  }

  // The delegate will cancel pending classification on destruction.
  EXPECT_CALL(*classifier, CancelPendingClassification());
}

TEST_F(PhishingClassifierDelegateTest, DISABLED_DuplicatePageCapture) {
  // Tests that a second PageCaptured notification causes classification to
  // be cancelled.
  MockPhishingClassifier* classifier =
      new StrictMock<MockPhishingClassifier>(view());
  PhishingClassifierDelegate* delegate =
      PhishingClassifierDelegate::Create(view(), classifier);
  MockScorer scorer;
  delegate->SetPhishingScorer(&scorer);
  ASSERT_TRUE(classifier->is_ready());

  EXPECT_CALL(*classifier, CancelPendingClassification());
  responses_["http://host.com/"] = "<html><body>phish</body></html>";
  LoadURL("http://host.com/");
  Mock::VerifyAndClearExpectations(classifier);
  OnStartPhishingDetection(delegate, GURL("http://host.com/"));
  string16 page_text = ASCIIToUTF16("phish");
  {
    InSequence s;
    EXPECT_CALL(*classifier, CancelPendingClassification());
    EXPECT_CALL(*classifier, BeginClassification(Pointee(page_text), _));
    delegate->PageCaptured(&page_text, false);
    Mock::VerifyAndClearExpectations(classifier);
  }

  page_text = ASCIIToUTF16("phish");
  EXPECT_CALL(*classifier, CancelPendingClassification());
  delegate->PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier);

  // The delegate will cancel pending classification on destruction.
  EXPECT_CALL(*classifier, CancelPendingClassification());
}

TEST_F(PhishingClassifierDelegateTest, DISABLED_PhishingDetectionDone) {
  // Tests that a PhishingDetectionDone IPC is sent to the browser
  // whenever we finish classification.
  MockPhishingClassifier* classifier =
      new StrictMock<MockPhishingClassifier>(view());
  PhishingClassifierDelegate* delegate =
      PhishingClassifierDelegate::Create(view(), classifier);
  MockScorer scorer;
  delegate->SetPhishingScorer(&scorer);
  ASSERT_TRUE(classifier->is_ready());

  // Start by loading a page to populate the delegate's state.
  responses_["http://host.com/"] = "<html><body>phish</body></html>";
  EXPECT_CALL(*classifier, CancelPendingClassification());
  LoadURL("http://host.com/#a");
  Mock::VerifyAndClearExpectations(classifier);
  string16 page_text = ASCIIToUTF16("phish");
  OnStartPhishingDetection(delegate, GURL("http://host.com/#a"));
  {
    InSequence s;
    EXPECT_CALL(*classifier, CancelPendingClassification());
    EXPECT_CALL(*classifier, BeginClassification(Pointee(page_text), _));
    delegate->PageCaptured(&page_text, false);
    Mock::VerifyAndClearExpectations(classifier);
  }

  // Now run the callback to simulate the classifier finishing.
  ClientPhishingRequest verdict;
  verdict.set_url("http://host.com/#a");
  verdict.set_client_score(0.8f);
  verdict.set_is_phishing(false);  // Send IPC even if site is not phishing.
  RunClassificationDone(delegate, verdict);
  ASSERT_TRUE(verdict_.get());
  EXPECT_EQ(verdict.SerializeAsString(), verdict_->SerializeAsString());

  // The delegate will cancel pending classification on destruction.
  EXPECT_CALL(*classifier, CancelPendingClassification());
}

}  // namespace safe_browsing
