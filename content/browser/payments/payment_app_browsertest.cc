// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "components/payments/content/payment_app.mojom.h"
#include "content/public/browser/payment_app_provider.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

void GetAllManifestsCallback(const base::Closure& done_callback,
                             PaymentAppProvider::Manifests* out_manifests,
                             PaymentAppProvider::Manifests manifests) {
  *out_manifests = std::move(manifests);
  done_callback.Run();
}

}  // namespace

class PaymentAppBrowserTest : public ContentBrowserTest {
 public:
  PaymentAppBrowserTest() {}
  ~PaymentAppBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(https_server_->Start());
    ASSERT_TRUE(NavigateToURL(
        shell(),
        https_server_->GetURL("/payments/payment_app_invocation.html")));
    ContentBrowserTest::SetUpOnMainThread();
  }

  bool RunScript(const std::string& script, std::string* result) {
    return content::ExecuteScriptAndExtractString(shell()->web_contents(),
                                                  script, result);
  }

  std::string PopConsoleString() {
    std::string script_result;
    EXPECT_TRUE(RunScript("resultQueue.pop()", &script_result));
    return script_result;
  }

  void RegisterPaymentApp() {
    std::string script_result;
    ASSERT_TRUE(RunScript("registerPaymentApp()", &script_result));
    ASSERT_EQ("registered", script_result);
  }

  std::vector<int64_t> GetAllPaymentAppIDs() {
    base::RunLoop run_loop;
    PaymentAppProvider::Manifests manifests;
    PaymentAppProvider::GetInstance()->GetAllManifests(
        shell()->web_contents()->GetBrowserContext(),
        base::Bind(&GetAllManifestsCallback, run_loop.QuitClosure(),
                   &manifests));
    run_loop.Run();

    std::vector<int64_t> ids;
    for (const auto& manifest : manifests) {
      ids.push_back(manifest.first);
    }

    return ids;
  }

  void InvokePaymentApp(int64_t registration_id) {
    payments::mojom::PaymentAppRequestPtr app_request =
        payments::mojom::PaymentAppRequest::New();
    app_request->methodData.push_back(
        payments::mojom::PaymentMethodData::New());
    app_request->total = payments::mojom::PaymentItem::New();
    app_request->total->amount = payments::mojom::PaymentCurrencyAmount::New();
    PaymentAppProvider::GetInstance()->InvokePaymentApp(
        shell()->web_contents()->GetBrowserContext(), registration_id,
        std::move(app_request));
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;

  DISALLOW_COPY_AND_ASSIGN(PaymentAppBrowserTest);
};

IN_PROC_BROWSER_TEST_F(PaymentAppBrowserTest, PaymentAppInvocation) {
  RegisterPaymentApp();

  std::vector<int64_t> ids = GetAllPaymentAppIDs();
  ASSERT_EQ(1U, ids.size());

  InvokePaymentApp(ids[0]);

  ASSERT_EQ("payment_app_window_ready", PopConsoleString());
  ASSERT_EQ("payment_app_response", PopConsoleString());
}

}  // namespace content
