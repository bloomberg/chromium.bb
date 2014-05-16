// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/phishing_classifier.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/safe_browsing/client_model.pb.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "chrome/renderer/safe_browsing/features.h"
#include "chrome/renderer/safe_browsing/mock_feature_extractor_clock.h"
#include "chrome/renderer/safe_browsing/murmurhash3_util.h"
#include "chrome/renderer/safe_browsing/scorer.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/renderer/render_view.h"
#include "crypto/sha2.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Not;
using ::testing::Pair;

namespace {

// The first RenderFrame is routing ID 1, and the first RenderView is 2.
const int kRenderViewRoutingId = 2;

}

namespace safe_browsing {

class PhishingClassifierTest : public InProcessBrowserTest {
 protected:
  PhishingClassifierTest()
      : url_tld_token_net_(features::kUrlTldToken + std::string("net")),
        page_link_domain_phishing_(features::kPageLinkDomain +
                                   std::string("phishing.com")),
        page_term_login_(features::kPageTerm + std::string("login")) {
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kSingleProcess);
#if defined(OS_WIN)
    // Don't want to try to create a GPU process.
    command_line->AppendSwitch(switches::kDisableGpu);
#endif
  }

  virtual void SetUpOnMainThread() OVERRIDE {
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
    model.set_max_shingles_per_page(100);
    model.set_shingle_size(3);

    clock_ = new MockFeatureExtractorClock;
    scorer_.reset(Scorer::Create(model.SerializeAsString()));
    ASSERT_TRUE(scorer_.get());

    classifier_.reset(new PhishingClassifier(
        content::RenderView::FromRoutingID(kRenderViewRoutingId),
        clock_));
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    content::RunAllPendingInMessageLoop();
  }

  // Helper method to start phishing classification and wait for it to
  // complete.  Returns the true if the page is classified as phishy and
  // false otherwise.
  bool RunPhishingClassifier(const base::string16* page_text,
                             float* phishy_score,
                             FeatureMap* features) {
    ClientPhishingRequest verdict;
    // The classifier accesses the RenderView and must run in the RenderThread.
    PostTaskToInProcessRendererAndWait(
        base::Bind(&PhishingClassifierTest::DoRunPhishingClassifier,
                   base::Unretained(this),
                   page_text, phishy_score, features, &verdict));
    return verdict.is_phishing();
  }

  void DoRunPhishingClassifier(const base::string16* page_text,
                               float* phishy_score,
                               FeatureMap* features,
                               ClientPhishingRequest* verdict) {
    *phishy_score = PhishingClassifier::kInvalidScore;
    features->Clear();

    // Force synchronous behavior for ease of unittesting.
    base::RunLoop run_loop;
    classifier_->BeginClassification(
        page_text,
        base::Bind(&PhishingClassifierTest::ClassificationFinished,
                   base::Unretained(this), &run_loop, verdict));
    content::RunThisRunLoop(&run_loop);

    *phishy_score = verdict->client_score();
    for (int i = 0; i < verdict->feature_map_size(); ++i) {
      features->AddRealFeature(verdict->feature_map(i).name(),
                               verdict->feature_map(i).value());
    }
  }

  // Completion callback for classification.
  void ClassificationFinished(base::RunLoop* run_loop,
                              ClientPhishingRequest* verdict_out,
                              const ClientPhishingRequest& verdict) {
    *verdict_out = verdict;  // Copy the verdict.
    run_loop->Quit();
  }

  scoped_ptr<net::test_server::EmbeddedTestServer> embedded_test_server_;
  net::test_server::EmbeddedTestServer* embedded_test_server() {
    // TODO(ajwong): Merge this into BrowserTestBase.
    if (!embedded_test_server_) {
      embedded_test_server_.reset(new net::test_server::EmbeddedTestServer());
      embedded_test_server_->RegisterRequestHandler(
          base::Bind(&PhishingClassifierTest::HandleRequest,
                     base::Unretained(this)));
      CHECK(embedded_test_server_->InitializeAndWaitUntilReady());
    }
    return embedded_test_server_.get();
  }

  void LoadHtml(const std::string& host, const std::string& content) {
    GURL::Replacements replace_host;
    replace_host.SetHostStr(host);
    response_content_ = content;
    ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->base_url().ReplaceComponents(replace_host));
  }

  void LoadHtmlPost(const std::string& host, const std::string& content) {
    GURL::Replacements replace_host;
    replace_host.SetHostStr(host);
    response_content_ = content;
    ui_test_utils::NavigateToURLWithPost(
        browser(),
        embedded_test_server()->base_url().ReplaceComponents(replace_host));
  }

  scoped_ptr<net::test_server::HttpResponse>
      HandleRequest(const net::test_server::HttpRequest& request) {
    scoped_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse());
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("text/html");
    http_response->set_content(response_content_);
    return http_response.PassAs<net::test_server::HttpResponse>();
  }

  std::string response_content_;
  scoped_ptr<Scorer> scorer_;
  scoped_ptr<PhishingClassifier> classifier_;
  MockFeatureExtractorClock* clock_;  // Owned by classifier_.

  // Features that are in the model.
  const std::string url_tld_token_net_;
  const std::string page_link_domain_phishing_;
  const std::string page_term_login_;
};

// This test flakes on Mac with force compositing mode.
// http://crbug.com/316709
#if defined(OS_MACOSX)
#define MAYBE_TestClassification DISABLED_TestClassification
#else
#define MAYBE_TestClassification TestClassification
#endif
IN_PROC_BROWSER_TEST_F(PhishingClassifierTest, MAYBE_TestClassification) {
  host_resolver()->AddRule("*", "127.0.0.1");

  // No scorer yet, so the classifier is not ready.
  ASSERT_FALSE(classifier_->is_ready());

  // Now set the scorer.
  classifier_->set_phishing_scorer(scorer_.get());
  ASSERT_TRUE(classifier_->is_ready());

  // This test doesn't exercise the extraction timing.
  EXPECT_CALL(*clock_, Now())
      .WillRepeatedly(::testing::Return(base::TimeTicks::Now()));

  base::string16 page_text = base::ASCIIToUTF16("login");
  float phishy_score;
  FeatureMap features;

  LoadHtml("host.net",
      "<html><body><a href=\"http://phishing.com/\">login</a></body></html>");
  EXPECT_TRUE(RunPhishingClassifier(&page_text, &phishy_score, &features));
  // Note: features.features() might contain other features that simply aren't
  // in the model.
  EXPECT_THAT(features.features(),
              AllOf(Contains(Pair(url_tld_token_net_, 1.0)),
                    Contains(Pair(page_link_domain_phishing_, 1.0)),
                    Contains(Pair(page_term_login_, 1.0))));
  EXPECT_FLOAT_EQ(0.5, phishy_score);

  // Change the link domain to something non-phishy.
  LoadHtml("host.net",
           "<html><body><a href=\"http://safe.com/\">login</a></body></html>");
  EXPECT_FALSE(RunPhishingClassifier(&page_text, &phishy_score, &features));
  EXPECT_THAT(features.features(),
              AllOf(Contains(Pair(url_tld_token_net_, 1.0)),
                    Contains(Pair(page_term_login_, 1.0))));
  EXPECT_THAT(features.features(),
              Not(Contains(Pair(page_link_domain_phishing_, 1.0))));
  EXPECT_GE(phishy_score, 0.0);
  EXPECT_LT(phishy_score, 0.5);

  // Extraction should fail for this case since there is no TLD.
  LoadHtml("localhost", "<html><body>content</body></html>");
  EXPECT_FALSE(RunPhishingClassifier(&page_text, &phishy_score, &features));
  EXPECT_EQ(0U, features.features().size());
  EXPECT_EQ(PhishingClassifier::kInvalidScore, phishy_score);

  // Extraction should also fail for this case because the URL is not http.
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  std::string host_str("host.net");  // Must outlive replace_host.
  GURL::Replacements replace_host;
  replace_host.SetHostStr(host_str);
  GURL test_url = https_server.GetURL("/files/title1.html");
  ui_test_utils::NavigateToURL(browser(),
                               test_url.ReplaceComponents(replace_host));
  EXPECT_FALSE(RunPhishingClassifier(&page_text, &phishy_score, &features));
  EXPECT_EQ(0U, features.features().size());
  EXPECT_EQ(PhishingClassifier::kInvalidScore, phishy_score);

  // Extraction should fail for this case because the URL is a POST request.
  LoadHtmlPost("host.net", "<html><body>content</body></html>");
  EXPECT_FALSE(RunPhishingClassifier(&page_text, &phishy_score, &features));
  EXPECT_EQ(0U, features.features().size());
  EXPECT_EQ(PhishingClassifier::kInvalidScore, phishy_score);
}

// Test flakes with LSAN enabled. See http://crbug.com/373155.
#if defined(LEAK_SANITIZER)
#define MAYBE_DisableDetection DISABLED_DisableDetection
#else
#define MAYBE_DisableDetection DisableDetection
#endif
IN_PROC_BROWSER_TEST_F(PhishingClassifierTest, MAYBE_DisableDetection) {
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
