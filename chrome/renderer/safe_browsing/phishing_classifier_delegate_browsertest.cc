// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/phishing_classifier_delegate.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
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
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/renderer/render_view.h"
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
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Pointee;
using ::testing::StrictMock;

namespace safe_browsing {

namespace {

// The RenderFrame is routing ID 1, and the RenderView is 2.
const int kRenderViewRoutingId = 2;

class MockPhishingClassifier : public PhishingClassifier {
 public:
  explicit MockPhishingClassifier(content::RenderView* render_view)
      : PhishingClassifier(render_view, NULL /* clock */) {}

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
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(InterceptingMessageFilter, message)
        IPC_MESSAGE_HANDLER(SafeBrowsingHostMsg_PhishingDetectionDone,
                            OnPhishingDetectionDone)
        IPC_MESSAGE_UNHANDLED(handled = false);
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
    waiting_message_loop_->PostTask(FROM_HERE, quit_closure_);
  }

 private:
  virtual ~InterceptingMessageFilter() {}

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
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kSingleProcess);
#if defined(OS_WIN)
    // Don't want to try to create a GPU process.
    command_line->AppendSwitch(switches::kDisableGpu);
#endif
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    intercepting_filter_ = new InterceptingMessageFilter();
    content::RenderView* render_view =
        content::RenderView::FromRoutingID(kRenderViewRoutingId);

    GetWebContents()->GetRenderProcessHost()->AddFilter(
        intercepting_filter_.get());
    classifier_ = new StrictMock<MockPhishingClassifier>(render_view);
    delegate_ = PhishingClassifierDelegate::Create(render_view, classifier_);

    ASSERT_TRUE(StartTestServer());
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

  bool StartTestServer() {
    CHECK(!embedded_test_server_);
    embedded_test_server_.reset(new net::test_server::EmbeddedTestServer());
    embedded_test_server_->RegisterRequestHandler(
        base::Bind(&PhishingClassifierDelegateTest::HandleRequest,
                   base::Unretained(this)));
    return embedded_test_server_->InitializeAndWaitUntilReady();
  }

  scoped_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    std::map<std::string, std::string>::const_iterator host_it =
        request.headers.find("Host");
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
    return http_response.PassAs<net::test_server::HttpResponse>();
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Returns the URL that was loaded.
  GURL LoadHtml(const std::string& host, const std::string& content) {
    GURL::Replacements replace_host;
    replace_host.SetHostStr(host);
    response_content_ = content;
    response_url_ =
        embedded_test_server_->base_url().ReplaceComponents(replace_host);
    ui_test_utils::NavigateToURL(browser(), response_url_);
    return response_url_;
  }

  void NavigateMainFrame(const GURL& url) {
    PostTaskToInProcessRendererAndWait(
        base::Bind(&PhishingClassifierDelegateTest::NavigateMainFrameInternal,
                   base::Unretained(this), url));
  }

  void NavigateMainFrameInternal(const GURL& url) {
    content::RenderView* render_view =
        content::RenderView::FromRoutingID(kRenderViewRoutingId);
    render_view->GetWebView()->mainFrame()->firstChild()->loadRequest(
        blink::WebURLRequest(url));
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
  scoped_ptr<net::test_server::EmbeddedTestServer> embedded_test_server_;
  scoped_ptr<ClientPhishingRequest> verdict_;
  StrictMock<MockPhishingClassifier>* classifier_;  // Owned by |delegate_|.
  PhishingClassifierDelegate* delegate_;  // Owned by the RenderView.
  scoped_refptr<content::MessageLoopRunner> runner_;
};

IN_PROC_BROWSER_TEST_F(PhishingClassifierDelegateTest, Navigation) {
  MockScorer scorer;
  delegate_->SetPhishingScorer(&scorer);
  ASSERT_TRUE(classifier_->is_ready());

  // Test an initial load.  We expect classification to happen normally.
  EXPECT_CALL(*classifier_, CancelPendingClassification()).Times(2);
  std::string port = base::IntToString(embedded_test_server_->port());
  std::string html = "<html><body><iframe src=\"http://sub1.com:";
  html += port;
  html += "/\"></iframe></body></html>";
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
  EXPECT_CALL(*classifier_, CancelPendingClassification()).Times(2);

  content::TestNavigationObserver observer(GetWebContents());
  chrome::Reload(browser(), CURRENT_TAB);
  observer.Wait();

  Mock::VerifyAndClearExpectations(classifier_);
  OnStartPhishingDetection(url);
  page_text = ASCIIToUTF16("dummy");
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier_);

  // Navigating in a subframe will not change the toplevel URL.  However, this
  // should cancel pending classification since the page content is changing.
  // Currently, we do not start a new classification after subframe loads.
  EXPECT_CALL(*classifier_, CancelPendingClassification())
      .WillOnce(Invoke(this, &PhishingClassifierDelegateTest::CancelCalled));

  runner_ = new content::MessageLoopRunner;
  NavigateMainFrame(GURL(std::string("http://sub2.com:") + port + "/"));

  runner_->Run();
  runner_ = NULL;

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
  EXPECT_CALL(*classifier_, CancelPendingClassification()).Times(2);
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
  EXPECT_CALL(*classifier_, CancelPendingClassification()).Times(2);
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

IN_PROC_BROWSER_TEST_F(PhishingClassifierDelegateTest, NoScorer) {
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

IN_PROC_BROWSER_TEST_F(PhishingClassifierDelegateTest, NoScorer_Ref) {
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

  std::string url_str = "http://host4.com:";
  url_str += base::IntToString(embedded_test_server_->port());
  url_str += "/redir";
  OnStartPhishingDetection(GURL(url_str));
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

IN_PROC_BROWSER_TEST_F(PhishingClassifierDelegateTest,
                       IgnorePreliminaryCapture) {
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

IN_PROC_BROWSER_TEST_F(PhishingClassifierDelegateTest, PhishingDetectionDone) {
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
