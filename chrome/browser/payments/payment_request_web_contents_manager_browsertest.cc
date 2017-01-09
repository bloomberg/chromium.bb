// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/payments/payment_request.h"
#include "components/payments/payment_request_web_contents_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace payments {

class PaymentRequestWebContentsManagerBrowserTest
    : public InProcessBrowserTest {
 public:
  PaymentRequestWebContentsManagerBrowserTest() {}
  ~PaymentRequestWebContentsManagerBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    ASSERT_TRUE(https_server_->InitializeAndListen());
    https_server_->ServeFilesFromSourceDirectory("chrome/test/data");
    https_server_->StartAcceptingConnections();
  }

  // Convenience method to get a list of PaymentRequest associated with
  // |web_contents|.
  const std::vector<PaymentRequest*> GetPaymentRequests(
      content::WebContents* web_contents) {
    PaymentRequestWebContentsManager* manager =
        PaymentRequestWebContentsManager::GetOrCreateForWebContents(
            web_contents);
    if (!manager)
      return std::vector<PaymentRequest*>();

    std::vector<PaymentRequest*> payment_requests_ptrs;
    for (const auto& p : manager->payment_requests_) {
      payment_requests_ptrs.push_back(p.first);
    }
    return payment_requests_ptrs;
  }

  std::unique_ptr<net::EmbeddedTestServer> https_server_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestWebContentsManagerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestWebContentsManagerBrowserTest,
                       MultiplePaymentRequests) {
  GURL url = https_server_->GetURL("/payment_request_multiple_requests.html");
  ui_test_utils::NavigateToURL(browser(), url);

  const std::vector<PaymentRequest*> payment_requests =
      GetPaymentRequests(browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(5U, payment_requests.size());
}

}  // namespace payments
