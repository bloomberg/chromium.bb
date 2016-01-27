// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/phishing_classifier_delegate.h"

#include <stdint.h>
#include <utility>

#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "chrome/common/safe_browsing/safebrowsing_messages.h"
#include "chrome/renderer/safe_browsing/features.h"
#include "chrome/renderer/safe_browsing/phishing_classifier.h"
#include "chrome/renderer/safe_browsing/scorer.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Pointee;
using ::testing::StrictMock;

namespace safe_browsing {

namespace {

class MockPhishingClassifier : public PhishingClassifier {
 public:
  explicit MockPhishingClassifier(content::RenderFrame* render_frame)
      : PhishingClassifier(render_frame, NULL /* clock */) {}

  virtual ~MockPhishingClassifier() {}

  MOCK_METHOD2(BeginClassification,
               void(const base::string16*, const DoneCallback&));
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

class InterceptingMessageFilter : public content::BrowserMessageFilter {
 public:
  InterceptingMessageFilter()
      : BrowserMessageFilter(SafeBrowsingMsgStart),
        waiting_message_loop_(NULL) {
  }

  const ClientPhishingRequest* verdict() const { return verdict_.get(); }
  bool OnMessageReceived(const IPC::Message& message) override {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(InterceptingMessageFilter, message)
        IPC_MESSAGE_HANDLER(SafeBrowsingHostMsg_PhishingDetectionDone,
                            OnPhishingDetectionDone)
        IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  void Reset() {
    run_loop_.reset(new base::RunLoop());
    waiting_message_loop_ = base::MessageLoop::current();
    quit_closure_ = run_loop_->QuitClosure();
  }

  void RunUntilVerdictReceived() {
    content::RunThisRunLoop(run_loop_.get());

    // Clear out the synchronization state just in case.
    waiting_message_loop_ = NULL;
    quit_closure_.Reset();
    run_loop_.reset();
  }

  void OnPhishingDetectionDone(const std::string& verdict_str) {
    scoped_ptr<ClientPhishingRequest> verdict(new ClientPhishingRequest);
    if (verdict->ParseFromString(verdict_str) &&
        verdict->IsInitialized()) {
      verdict_.swap(verdict);
    }
    waiting_message_loop_->task_runner()->PostTask(FROM_HERE, quit_closure_);
  }

 private:
  ~InterceptingMessageFilter() override {}

  scoped_ptr<ClientPhishingRequest> verdict_;
  base::MessageLoop* waiting_message_loop_;
  base::Closure quit_closure_;
  scoped_ptr<base::RunLoop> run_loop_;
};
}  // namespace

class PhishingClassifierDelegateTest : public InProcessBrowserTest {
 public:
  void CancelCalled() {
    if (runner_.get()) {
      content::BrowserThread::PostTask(
          content::BrowserThread::UI, FROM_HERE, runner_->QuitClosure());
    }
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kSingleProcess);
#if defined(OS_WIN)
    // Don't want to try to create a GPU process.
    command_line->AppendSwitch(switches::kDisableGpu);
#endif
  }

  void SetUpOnMainThread() override {
    intercepting_filter_ = new InterceptingMessageFilter();

    GetWebContents()->GetRenderProcessHost()->AddFilter(
        intercepting_filter_.get());
    content::RenderFrame* render_frame = GetRenderFrame();
    classifier_ = new StrictMock<MockPhishingClassifier>(render_frame);
    PostTaskToInProcessRendererAndWait(
        base::Bind(&PhishingClassifierDelegateTest::SetUpOnRendererThread,
                   base::Unretained(this)));

    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&PhishingClassifierDelegateTest::HandleRequest,
                   base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  // Runs the ClassificationDone callback, then waits for the
  // PhishingDetectionDone IPC to arrive.
  void RunClassificationDone(const ClientPhishingRequest& verdict) {
    // Clear out any previous state.
    intercepting_filter_->Reset();
    PostTaskToInProcessRendererAndWait(
        base::Bind(&PhishingClassifierDelegate::ClassificationDone,
        base::Unretained(delegate_),
        verdict));
    intercepting_filter_->RunUntilVerdictReceived();
  }

  void OnStartPhishingDetection(const GURL& url) {
    PostTaskToInProcessRendererAndWait(
        base::Bind(&PhishingClassifierDelegate::OnStartPhishingDetection,
                   base::Unretained(delegate_), url));
  }

  void PageCaptured(base::string16* page_text, bool preliminary_capture) {
    PostTaskToInProcessRendererAndWait(
        base::Bind(&PhishingClassifierDelegate::PageCaptured,
                   base::Unretained(delegate_), page_text,
                   preliminary_capture));
  }

  scoped_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    auto host_it = request.headers.find("Host");
    if (host_it == request.headers.end())
      return scoped_ptr<net::test_server::HttpResponse>();

    std::string url =
        std::string("http://") + host_it->second + request.relative_url;
    if (response_url_.spec() != url)
      return scoped_ptr<net::test_server::HttpResponse>();

    scoped_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse());
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("text/html");
    http_response->set_content(response_content_);
    return std::move(http_response);
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrame* GetRenderFrame() {
    int render_frame_routing_id =
        GetWebContents()->GetMainFrame()->GetRoutingID();
    content::RenderFrame* render_frame =
        content::RenderFrame::FromRoutingID(render_frame_routing_id);
    return render_frame;
  }

  // Returns the URL that was loaded.
  GURL LoadHtml(const std::string& host, const std::string& content) {
    GURL::Replacements replace_host;
    replace_host.SetHostStr(host);
    response_content_ = content;
    response_url_ =
        embedded_test_server()->base_url().ReplaceComponents(replace_host);
    ui_test_utils::NavigateToURL(browser(), response_url_);
    return response_url_;
  }

  void GoBack() {
    GetWebContents()->GetController().GoBack();
    content::WaitForLoadStop(GetWebContents());
  }

  void GoForward() {
    GetWebContents()->GetController().GoForward();
    content::WaitForLoadStop(GetWebContents());
  }

  scoped_refptr<InterceptingMessageFilter> intercepting_filter_;
  GURL response_url_;
  std::string response_content_;
  scoped_ptr<ClientPhishingRequest> verdict_;
  StrictMock<MockPhishingClassifier>* classifier_;  // Owned by |delegate_|.
  PhishingClassifierDelegate* delegate_;  // Owned by the RenderView.
  scoped_refptr<content::MessageLoopRunner> runner_;

 private:
  void SetUpOnRendererThread() {
    content::RenderFrame* render_frame = GetRenderFrame();
    // PhishingClassifierDelegate is a RenderFrameObserver and therefore has to
    // be created on the renderer thread, which is not the main thread in an
    // in-process-browser-test.
    delegate_ = PhishingClassifierDelegate::Create(render_frame, classifier_);
  }

};

IN_PROC_BROWSER_TEST_F(PhishingClassifierDelegateTest, Navigation) {
  MockScorer scorer;
  delegate_->SetPhishingScorer(&scorer);
  ASSERT_TRUE(classifier_->is_ready());

  // Test an initial load.  We expect classification to happen normally.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  GURL iframe_url = embedded_test_server()->GetURL("/");
  GURL::Replacements replace_host;
  replace_host.SetHostStr("sub1.com");
  std::string html = "<html><body><iframe src=\"";
  html += iframe_url.ReplaceComponents(replace_host).spec();
  html += "\"></iframe></body></html>";
  GURL url = LoadHtml("host.com", html);
  Mock::VerifyAndClearExpectations(classifier_);
  OnStartPhishingDetection(url);
  base::string16 page_text = ASCIIToUTF16("dummy");
  {
    InSequence s;
    EXPECT_CALL(*classifier_, CancelPendingClassification());
    EXPECT_CALL(*classifier_, BeginClassification(Pointee(page_text), _));
    PageCaptured(&page_text, false);
    Mock::VerifyAndClearExpectations(classifier_);
  }

  // Reloading the same page should not trigger a reclassification.
  // However, it will cancel any pending classification since the
  // content is being replaced.
  EXPECT_CALL(*classifier_, CancelPendingClassification());

  content::TestNavigationObserver observer(GetWebContents());
  chrome::Reload(browser(), CURRENT_TAB);
  observer.Wait();

  Mock::VerifyAndClearExpectations(classifier_);
  OnStartPhishingDetection(url);
  page_text = ASCIIToUTF16("dummy");
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier_);

  OnStartPhishingDetection(url);
  page_text = ASCIIToUTF16("dummy");
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier_);

  // Scrolling to an anchor works similarly to a subframe navigation, but
  // see the TODO in PhishingClassifierDelegate::DidCommitProvisionalLoad.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  GURL foo_url = GURL(url.spec() + "#foo");
  ui_test_utils::NavigateToURL(browser(), foo_url);
  Mock::VerifyAndClearExpectations(classifier_);
  OnStartPhishingDetection(url);
  page_text = ASCIIToUTF16("dummy");
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier_);

  // Now load a new toplevel page, which should trigger another classification.
  EXPECT_CALL(*classifier_, CancelPendingClassification())
      .WillOnce(Invoke(this, &PhishingClassifierDelegateTest::CancelCalled));

  runner_ = new content::MessageLoopRunner;
  url = LoadHtml("host2.com", "dummy2");
  runner_->Run();
  runner_ = NULL;

  Mock::VerifyAndClearExpectations(classifier_);
  page_text = ASCIIToUTF16("dummy2");
  OnStartPhishingDetection(url);
  {
    InSequence s;
    EXPECT_CALL(*classifier_, CancelPendingClassification());
    EXPECT_CALL(*classifier_, BeginClassification(Pointee(page_text), _));
    PageCaptured(&page_text, false);
    Mock::VerifyAndClearExpectations(classifier_);
  }

  // No classification should happen on back/forward navigation.
  // Note: in practice, the browser will not send a StartPhishingDetection IPC
  // in this case.  However, we want to make sure that the delegate behaves
  // correctly regardless.
  EXPECT_CALL(*classifier_, CancelPendingClassification()).Times(1);
  GoBack();
  Mock::VerifyAndClearExpectations(classifier_);

  page_text = ASCIIToUTF16("dummy");
  OnStartPhishingDetection(url);
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier_);

  EXPECT_CALL(*classifier_, CancelPendingClassification());
  GoForward();
  Mock::VerifyAndClearExpectations(classifier_);

  page_text = ASCIIToUTF16("dummy2");
  OnStartPhishingDetection(url);
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier_);

  // Now go back again and scroll to a different anchor.
  // No classification should happen.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  GoBack();
  Mock::VerifyAndClearExpectations(classifier_);
  page_text = ASCIIToUTF16("dummy");

  OnStartPhishingDetection(url);
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier_);

  EXPECT_CALL(*classifier_, CancelPendingClassification());
  GURL foo2_url = GURL(foo_url.spec() + "2");
  ui_test_utils::NavigateToURL(browser(), foo2_url);
  Mock::VerifyAndClearExpectations(classifier_);

  OnStartPhishingDetection(url);
  page_text = ASCIIToUTF16("dummy");
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier_);

  // The delegate will cancel pending classification on destruction.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
}

// Flaky: crbug.com/479757
#if defined(LEAK_SANITIZER)
#define MAYBE_NoScorer DISABLED_NoScorer
#else
#define MAYBE_NoScorer NoScorer
#endif

IN_PROC_BROWSER_TEST_F(PhishingClassifierDelegateTest, MAYBE_NoScorer) {
  // For this test, we'll create the delegate with no scorer available yet.
  ASSERT_FALSE(classifier_->is_ready());

  // Queue up a pending classification, cancel it, then queue up another one.
  GURL url = LoadHtml("host.com", "dummy");
  base::string16 page_text = ASCIIToUTF16("dummy");
  OnStartPhishingDetection(url);
  PageCaptured(&page_text, false);

  url = LoadHtml("host2.com", "dummy2");
  page_text = ASCIIToUTF16("dummy2");
  OnStartPhishingDetection(url);
  PageCaptured(&page_text, false);

  // Now set a scorer, which should cause a classifier to be created and
  // the classification to proceed.
  page_text = ASCIIToUTF16("dummy2");
  EXPECT_CALL(*classifier_, BeginClassification(Pointee(page_text), _));
  MockScorer scorer;
  delegate_->SetPhishingScorer(&scorer);
  Mock::VerifyAndClearExpectations(classifier_);

  // If we set a new scorer while a classification is going on the
  // classification should be cancelled.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  delegate_->SetPhishingScorer(&scorer);
  Mock::VerifyAndClearExpectations(classifier_);

  // The delegate will cancel pending classification on destruction.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
}

// Flaky: crbug.com/435719
#if defined(LEAK_SANITIZER)
#define MAYBE_NoScorer_Ref DISABLED_NoScorer_Ref
#else
#define MAYBE_NoScorer_Ref NoScorer_Ref
#endif

IN_PROC_BROWSER_TEST_F(PhishingClassifierDelegateTest, MAYBE_NoScorer_Ref) {
  // Similar to the last test, but navigates within the page before
  // setting the scorer.
  ASSERT_FALSE(classifier_->is_ready());

  // Queue up a pending classification, cancel it, then queue up another one.
  GURL url = LoadHtml("host.com", "dummy");
  base::string16 page_text = ASCIIToUTF16("dummy");
  OnStartPhishingDetection(url);
  PageCaptured(&page_text, false);

  OnStartPhishingDetection(url);
  page_text = ASCIIToUTF16("dummy");
  PageCaptured(&page_text, false);

  // Now set a scorer, which should cause a classifier to be created and
  // the classification to proceed.
  page_text = ASCIIToUTF16("dummy");
  EXPECT_CALL(*classifier_, BeginClassification(Pointee(page_text), _));
  MockScorer scorer;
  delegate_->SetPhishingScorer(&scorer);
  Mock::VerifyAndClearExpectations(classifier_);

  // The delegate will cancel pending classification on destruction.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
}

IN_PROC_BROWSER_TEST_F(PhishingClassifierDelegateTest,
                       NoStartPhishingDetection) {
  // Tests the behavior when OnStartPhishingDetection has not yet been called
  // when the page load finishes.
  MockScorer scorer;
  delegate_->SetPhishingScorer(&scorer);
  ASSERT_TRUE(classifier_->is_ready());

  EXPECT_CALL(*classifier_, CancelPendingClassification());
  GURL url = LoadHtml("host.com", "<html><body>phish</body></html>");
  Mock::VerifyAndClearExpectations(classifier_);
  base::string16 page_text = ASCIIToUTF16("phish");
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier_);
  // Now simulate the StartPhishingDetection IPC.  We expect classification
  // to begin.
  page_text = ASCIIToUTF16("phish");
  EXPECT_CALL(*classifier_, BeginClassification(Pointee(page_text), _));
  OnStartPhishingDetection(url);
  Mock::VerifyAndClearExpectations(classifier_);

  // Now try again, but this time we will navigate the page away before
  // the IPC is sent.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  LoadHtml("host2.com", "<html><body>phish</body></html>");
  Mock::VerifyAndClearExpectations(classifier_);
  page_text = ASCIIToUTF16("phish");
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier_);

  EXPECT_CALL(*classifier_, CancelPendingClassification());
  LoadHtml("host3.com", "<html><body>phish</body></html>");
  Mock::VerifyAndClearExpectations(classifier_);
  OnStartPhishingDetection(url);

  // In this test, the original page is a redirect, which we do not get a
  // StartPhishingDetection IPC for.  We use location.replace() to load a
  // new page while reusing the original session history entry, and check that
  // classification begins correctly for the landing page.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  LoadHtml("host4.com", "<html><body>abc</body></html>");
  Mock::VerifyAndClearExpectations(classifier_);
  page_text = ASCIIToUTF16("abc");
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier_);
  EXPECT_CALL(*classifier_, CancelPendingClassification());

  ui_test_utils::NavigateToURL(
      browser(), GURL("javascript:location.replace(\'redir\');"));

  Mock::VerifyAndClearExpectations(classifier_);

  GURL redir_url = embedded_test_server()->GetURL("/redir");
  GURL::Replacements replace_host;
  replace_host.SetHostStr("host4.com");
  OnStartPhishingDetection(redir_url.ReplaceComponents(replace_host));
  page_text = ASCIIToUTF16("123");
  {
    InSequence s;
    EXPECT_CALL(*classifier_, CancelPendingClassification());
    EXPECT_CALL(*classifier_, BeginClassification(Pointee(page_text), _));
    PageCaptured(&page_text, false);
    Mock::VerifyAndClearExpectations(classifier_);
  }

  // The delegate will cancel pending classification on destruction.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
}

// Test flakes with LSAN enabled. See http://crbug.com/373155.
#if defined(LEAK_SANITIZER)
#define MAYBE_IgnorePreliminaryCapture DISABLED_IgnorePreliminaryCapture
#else
#define MAYBE_IgnorePreliminaryCapture IgnorePreliminaryCapture
#endif
IN_PROC_BROWSER_TEST_F(PhishingClassifierDelegateTest,
                       MAYBE_IgnorePreliminaryCapture) {
  // Tests that preliminary PageCaptured notifications are ignored.
  MockScorer scorer;
  delegate_->SetPhishingScorer(&scorer);
  ASSERT_TRUE(classifier_->is_ready());

  EXPECT_CALL(*classifier_, CancelPendingClassification());
  GURL url = LoadHtml("host.com", "<html><body>phish</body></html>");
  Mock::VerifyAndClearExpectations(classifier_);
  OnStartPhishingDetection(url);
  base::string16 page_text = ASCIIToUTF16("phish");
  PageCaptured(&page_text, true);

  // Once the non-preliminary capture happens, classification should begin.
  page_text = ASCIIToUTF16("phish");
  {
    InSequence s;
    EXPECT_CALL(*classifier_, CancelPendingClassification());
    EXPECT_CALL(*classifier_, BeginClassification(Pointee(page_text), _));
    PageCaptured(&page_text, false);
    Mock::VerifyAndClearExpectations(classifier_);
  }

  // The delegate will cancel pending classification on destruction.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
}

#if defined(ADDRESS_SANITIZER)
#define Maybe_DuplicatePageCapture DISABLED_DuplicatePageCapture
#else
#define Maybe_DuplicatePageCapture DuplicatePageCapture
#endif
IN_PROC_BROWSER_TEST_F(PhishingClassifierDelegateTest,
                       Maybe_DuplicatePageCapture) {
  // Tests that a second PageCaptured notification causes classification to
  // be cancelled.
  MockScorer scorer;
  delegate_->SetPhishingScorer(&scorer);
  ASSERT_TRUE(classifier_->is_ready());

  EXPECT_CALL(*classifier_, CancelPendingClassification());
  GURL url = LoadHtml("host.com", "<html><body>phish</body></html>");
  Mock::VerifyAndClearExpectations(classifier_);
  OnStartPhishingDetection(url);
  base::string16 page_text = ASCIIToUTF16("phish");
  {
    InSequence s;
    EXPECT_CALL(*classifier_, CancelPendingClassification());
    EXPECT_CALL(*classifier_, BeginClassification(Pointee(page_text), _));
    PageCaptured(&page_text, false);
    Mock::VerifyAndClearExpectations(classifier_);
  }

  page_text = ASCIIToUTF16("phish");
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier_);

  // The delegate will cancel pending classification on destruction.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
}

// Test flakes with LSAN enabled. See http://crbug.com/373155.
#if defined(LEAK_SANITIZER)
#define MAYBE_PhishingDetectionDone DISABLED_PhishingDetectionDone
#else
#define MAYBE_PhishingDetectionDone PhishingDetectionDone
#endif
IN_PROC_BROWSER_TEST_F(PhishingClassifierDelegateTest,
                       MAYBE_PhishingDetectionDone) {
  // Tests that a PhishingDetectionDone IPC is sent to the browser
  // whenever we finish classification.
  MockScorer scorer;
  delegate_->SetPhishingScorer(&scorer);
  ASSERT_TRUE(classifier_->is_ready());

  // Start by loading a page to populate the delegate's state.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  GURL url = LoadHtml("host.com", "<html><body>phish</body></html>");
  Mock::VerifyAndClearExpectations(classifier_);
  base::string16 page_text = ASCIIToUTF16("phish");
  OnStartPhishingDetection(url);
  {
    InSequence s;
    EXPECT_CALL(*classifier_, CancelPendingClassification());
    EXPECT_CALL(*classifier_, BeginClassification(Pointee(page_text), _));
    PageCaptured(&page_text, false);
    Mock::VerifyAndClearExpectations(classifier_);
  }

  // Now run the callback to simulate the classifier finishing.
  ClientPhishingRequest verdict;
  verdict.set_url(url.spec());
  verdict.set_client_score(0.8f);
  verdict.set_is_phishing(false);  // Send IPC even if site is not phishing.
  RunClassificationDone(verdict);
  ASSERT_TRUE(intercepting_filter_->verdict());
  EXPECT_EQ(verdict.SerializeAsString(),
            intercepting_filter_->verdict()->SerializeAsString());

  // The delegate will cancel pending classification on destruction.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
}

}  // namespace safe_browsing
