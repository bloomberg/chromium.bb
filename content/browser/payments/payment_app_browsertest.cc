// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "components/payments/mojom/payment_app.mojom.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
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

using ::payments::mojom::PaymentAppRequest;
using ::payments::mojom::PaymentAppRequestPtr;
using ::payments::mojom::PaymentAppResponsePtr;
using ::payments::mojom::PaymentCurrencyAmount;
using ::payments::mojom::PaymentDetailsModifier;
using ::payments::mojom::PaymentDetailsModifierPtr;
using ::payments::mojom::PaymentItem;
using ::payments::mojom::PaymentMethodData;

void GetAllPaymentAppsCallback(const base::Closure& done_callback,
                               PaymentAppProvider::PaymentApps* out_apps,
                               PaymentAppProvider::PaymentApps apps) {
  *out_apps = std::move(apps);
  done_callback.Run();
}

void InvokePaymentAppCallback(const base::Closure& done_callback,
                              PaymentAppResponsePtr* out_response,
                              PaymentAppResponsePtr response) {
  *out_response = std::move(response);
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
    PaymentAppProvider::PaymentApps apps;
    PaymentAppProvider::GetInstance()->GetAllPaymentApps(
        shell()->web_contents()->GetBrowserContext(),
        base::BindOnce(&GetAllPaymentAppsCallback, run_loop.QuitClosure(),
                       &apps));
    run_loop.Run();

    std::vector<int64_t> ids;
    for (const auto& app_info : apps) {
      for (const auto& instrument : app_info.second) {
        ids.push_back(instrument->registration_id);
      }
    }

    return ids;
  }

  PaymentAppResponsePtr InvokePaymentAppWithTestData(int64_t registration_id) {
    PaymentAppRequestPtr app_request = PaymentAppRequest::New();

    app_request->top_level_origin = GURL("https://example.com");

    app_request->payment_request_origin = GURL("https://example.com");

    app_request->payment_request_id = "payment-request-id";

    app_request->method_data.push_back(PaymentMethodData::New());
    app_request->method_data[0]->supported_methods = {"basic-card"};

    app_request->total = PaymentItem::New();
    app_request->total->amount = PaymentCurrencyAmount::New();
    app_request->total->amount->currency = "USD";

    PaymentDetailsModifierPtr modifier = PaymentDetailsModifier::New();
    modifier->total = PaymentItem::New();
    modifier->total->amount = PaymentCurrencyAmount::New();
    modifier->total->amount->currency = "USD";
    modifier->method_data = PaymentMethodData::New();
    modifier->method_data->supported_methods = {"basic-card"};
    app_request->modifiers.push_back(std::move(modifier));

    app_request->instrument_key = "instrument-key";

    base::RunLoop run_loop;
    PaymentAppResponsePtr response;
    PaymentAppProvider::GetInstance()->InvokePaymentApp(
        shell()->web_contents()->GetBrowserContext(), registration_id,
        std::move(app_request),
        base::Bind(&InvokePaymentAppCallback, run_loop.QuitClosure(),
                   &response));
    run_loop.Run();

    return response;
  }

  void ClearStoragePartitionData() {
    // Clear data from the storage partition. Parameters are set to clear data
    // for service workers, for all origins, for an unbounded time range.
    base::RunLoop run_loop;

    static_cast<StoragePartitionImpl*>(
        content::BrowserContext::GetDefaultStoragePartition(
            shell()->web_contents()->GetBrowserContext()))
        ->ClearData(StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS,
                    StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL, GURL(),
                    StoragePartition::OriginMatcherFunction(), base::Time(),
                    base::Time::Max(), run_loop.QuitClosure());

    run_loop.Run();
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;

  DISALLOW_COPY_AND_ASSIGN(PaymentAppBrowserTest);
};

IN_PROC_BROWSER_TEST_F(PaymentAppBrowserTest, PaymentAppInvocation) {
  RegisterPaymentApp();

  std::vector<int64_t> ids = GetAllPaymentAppIDs();
  ASSERT_EQ(1U, ids.size());

  PaymentAppResponsePtr response(InvokePaymentAppWithTestData(ids[0]));
  ASSERT_EQ("test", response->method_name);

  ClearStoragePartitionData();

  ids = GetAllPaymentAppIDs();
  ASSERT_EQ(0U, ids.size());

  EXPECT_EQ("https://example.com/", PopConsoleString() /* topLevelOrigin */);
  EXPECT_EQ("https://example.com/",
            PopConsoleString() /* paymentRequestOrigin */);
  EXPECT_EQ("payment-request-id", PopConsoleString() /* paymentRequestId */);
  EXPECT_EQ("[{\"supportedMethods\":[\"basic-card\"]}]",
            PopConsoleString() /* methodData */);
  EXPECT_EQ(
      "{\"amount\":{\"currency\":\"USD\",\"currencySystem\":\"urn:iso:std:iso:"
      "4217\",\"value\":\"\"},\"label\":\"\",\"pending\":false}",
      PopConsoleString() /* total */);
  EXPECT_EQ(
      "[{\"additionalDisplayItems\":[],\"supportedMethods\":[\"basic-card\"],"
      "\"total\":{\"amount\":{\"currency\":\"USD\",\"currencySystem\":\"urn:"
      "iso:std:iso:4217\",\"value\":\"\"},\"label\":\"\",\"pending\":false}}]",
      PopConsoleString() /* modifiers */);
  EXPECT_EQ("instrument-key", PopConsoleString() /* instrumentKey */);
}

}  // namespace content
