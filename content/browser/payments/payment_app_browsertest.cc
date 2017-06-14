// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
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
#include "third_party/WebKit/public/platform/modules/payments/payment_app.mojom.h"

namespace content {
namespace {

using ::payments::mojom::PaymentRequestEventData;
using ::payments::mojom::PaymentRequestEventDataPtr;
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

  std::map<std::string, int64_t> GetAllPaymentInstrumentRegistrationIDs() {
    base::RunLoop run_loop;
    PaymentAppProvider::PaymentApps apps;
    PaymentAppProvider::GetInstance()->GetAllPaymentApps(
        shell()->web_contents()->GetBrowserContext(),
        base::BindOnce(&GetAllPaymentAppsCallback, run_loop.QuitClosure(),
                       &apps));
    run_loop.Run();

    std::map<std::string, int64_t> registrationIds;
    for (const auto& app_info : apps) {
      for (const auto& instrument : app_info.second) {
        registrationIds.insert(std::pair<std::string, int64_t>(
            instrument->instrument_key, instrument->registration_id));
      }
    }

    return registrationIds;
  }

  PaymentAppResponsePtr InvokePaymentAppWithTestData(
      int64_t registration_id,
      const std::string& supported_method,
      const std::string& instrument_key) {
    PaymentRequestEventDataPtr event_data = PaymentRequestEventData::New();

    event_data->top_level_origin = GURL("https://example.com");

    event_data->payment_request_origin = GURL("https://example.com");

    event_data->payment_request_id = "payment-request-id";

    event_data->method_data.push_back(PaymentMethodData::New());
    event_data->method_data[0]->supported_methods = {supported_method};

    event_data->total = PaymentItem::New();
    event_data->total->amount = PaymentCurrencyAmount::New();
    event_data->total->amount->currency = "USD";

    PaymentDetailsModifierPtr modifier = PaymentDetailsModifier::New();
    modifier->total = PaymentItem::New();
    modifier->total->amount = PaymentCurrencyAmount::New();
    modifier->total->amount->currency = "USD";
    modifier->method_data = PaymentMethodData::New();
    modifier->method_data->supported_methods = {supported_method};
    event_data->modifiers.push_back(std::move(modifier));

    event_data->instrument_key = instrument_key;

    base::RunLoop run_loop;
    PaymentAppResponsePtr response;
    PaymentAppProvider::GetInstance()->InvokePaymentApp(
        shell()->web_contents()->GetBrowserContext(), registration_id,
        std::move(event_data),
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

  std::map<std::string, int64_t> registrationIds =
      GetAllPaymentInstrumentRegistrationIDs();
  ASSERT_EQ(2U, registrationIds.size());

  PaymentAppResponsePtr response(InvokePaymentAppWithTestData(
      registrationIds.at("basic-card-payment-app-id"), "basic-card",
      "basic-card-payment-app-id"));
  ASSERT_EQ("test", response->method_name);

  ClearStoragePartitionData();

  registrationIds = GetAllPaymentInstrumentRegistrationIDs();
  ASSERT_EQ(0U, registrationIds.size());

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
  EXPECT_EQ("basic-card-payment-app-id",
            PopConsoleString() /* instrumentKey */);
}

IN_PROC_BROWSER_TEST_F(PaymentAppBrowserTest, PaymentAppOpenWindowFailed) {
  RegisterPaymentApp();

  std::map<std::string, int64_t> registrationIds =
      GetAllPaymentInstrumentRegistrationIDs();
  ASSERT_EQ(2U, registrationIds.size());

  PaymentAppResponsePtr response(InvokePaymentAppWithTestData(
      registrationIds.at("bobpay-payment-app-id"), "https://bobpay.com",
      "bobpay-payment-app-id"));
  // InvokePaymentAppCallback returns empty method_name in case of failure, like
  // in PaymentRequestRespondWithObserver::OnResponseRejected.
  ASSERT_EQ("", response->method_name);

  ClearStoragePartitionData();

  registrationIds = GetAllPaymentInstrumentRegistrationIDs();
  ASSERT_EQ(0U, registrationIds.size());

  EXPECT_EQ("https://example.com/", PopConsoleString() /* topLevelOrigin */);
  EXPECT_EQ("https://example.com/",
            PopConsoleString() /* paymentRequestOrigin */);
  EXPECT_EQ("payment-request-id", PopConsoleString() /* paymentRequestId */);
  EXPECT_EQ("[{\"supportedMethods\":[\"https://bobpay.com\"]}]",
            PopConsoleString() /* methodData */);
  EXPECT_EQ(
      "{\"amount\":{\"currency\":\"USD\",\"currencySystem\":\"urn:iso:std:iso:"
      "4217\",\"value\":\"\"},\"label\":\"\",\"pending\":false}",
      PopConsoleString() /* total */);
  EXPECT_EQ(
      "[{\"additionalDisplayItems\":[],\"supportedMethods\":[\"https://"
      "bobpay.com\"],"
      "\"total\":{\"amount\":{\"currency\":\"USD\",\"currencySystem\":\"urn:"
      "iso:std:iso:4217\",\"value\":\"\"},\"label\":\"\",\"pending\":false}}]",
      PopConsoleString() /* modifiers */);
  EXPECT_EQ("bobpay-payment-app-id", PopConsoleString() /* instrumentKey */);
}
}  // namespace content
