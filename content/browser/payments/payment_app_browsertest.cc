// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/service_worker/service_worker_types.h"
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
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/platform/modules/payments/payment_app.mojom.h"

namespace content {
namespace {

using ::payments::mojom::CanMakePaymentEventData;
using ::payments::mojom::CanMakePaymentEventDataPtr;
using ::payments::mojom::PaymentRequestEventData;
using ::payments::mojom::PaymentRequestEventDataPtr;
using ::payments::mojom::PaymentHandlerResponsePtr;
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

void PaymentEventResultCallback(const base::Closure& done_callback,
                                bool* out_payment_event_result,
                                bool payment_event_result) {
  *out_payment_event_result = payment_event_result;
  done_callback.Run();
}

void InvokePaymentAppCallback(const base::Closure& done_callback,
                              PaymentHandlerResponsePtr* out_response,
                              PaymentHandlerResponsePtr response) {
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

  std::vector<int64_t> GetAllPaymentAppRegistrationIDs() {
    base::RunLoop run_loop;
    PaymentAppProvider::PaymentApps apps;
    PaymentAppProvider::GetInstance()->GetAllPaymentApps(
        shell()->web_contents()->GetBrowserContext(),
        base::BindOnce(&GetAllPaymentAppsCallback, run_loop.QuitClosure(),
                       &apps));
    run_loop.Run();

    std::vector<int64_t> registrationIds;
    for (const auto& app_info : apps) {
      registrationIds.push_back(app_info.second->registration_id);
    }

    return registrationIds;
  }

  bool AbortPayment(int64_t registration_id) {
    base::RunLoop run_loop;
    bool payment_aborted = false;
    PaymentAppProvider::GetInstance()->AbortPayment(
        shell()->web_contents()->GetBrowserContext(), registration_id,
        base::BindOnce(&PaymentEventResultCallback, run_loop.QuitClosure(),
                       &payment_aborted));
    run_loop.Run();

    return payment_aborted;
  }

  bool CanMakePaymentWithTestData(int64_t registration_id,
                                  const std::string& supported_method) {
    CanMakePaymentEventDataPtr event_data =
        CreateCanMakePaymentEventData(supported_method);

    base::RunLoop run_loop;
    bool can_make_payment = false;
    PaymentAppProvider::GetInstance()->CanMakePayment(
        shell()->web_contents()->GetBrowserContext(), registration_id,
        std::move(event_data),
        base::BindOnce(&PaymentEventResultCallback, run_loop.QuitClosure(),
                       &can_make_payment));
    run_loop.Run();

    return can_make_payment;
  }

  PaymentHandlerResponsePtr InvokePaymentAppWithTestData(
      int64_t registration_id,
      const std::string& supported_method,
      const std::string& instrument_key) {
    base::RunLoop run_loop;
    PaymentHandlerResponsePtr response;
    PaymentAppProvider::GetInstance()->InvokePaymentApp(
        shell()->web_contents()->GetBrowserContext(), registration_id,
        CreatePaymentRequestEventData(supported_method, instrument_key),
        base::BindOnce(&InvokePaymentAppCallback, run_loop.QuitClosure(),
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
  CanMakePaymentEventDataPtr CreateCanMakePaymentEventData(
      const std::string& supported_method) {
    CanMakePaymentEventDataPtr event_data = CanMakePaymentEventData::New();

    event_data->top_level_origin = GURL("https://example.com");

    event_data->payment_request_origin = GURL("https://example.com");

    event_data->method_data.push_back(PaymentMethodData::New());
    event_data->method_data[0]->supported_methods = {supported_method};

    PaymentDetailsModifierPtr modifier = PaymentDetailsModifier::New();
    modifier->total = PaymentItem::New();
    modifier->total->amount = PaymentCurrencyAmount::New();
    modifier->total->amount->currency = "USD";
    modifier->total->amount->value = "55";
    modifier->method_data = PaymentMethodData::New();
    modifier->method_data->supported_methods = {supported_method};
    event_data->modifiers.push_back(std::move(modifier));

    return event_data;
  }

  PaymentRequestEventDataPtr CreatePaymentRequestEventData(
      const std::string& supported_method,
      const std::string& instrument_key) {
    PaymentRequestEventDataPtr event_data = PaymentRequestEventData::New();

    event_data->top_level_origin = GURL("https://example.com");

    event_data->payment_request_origin = GURL("https://example.com");

    event_data->payment_request_id = "payment-request-id";

    event_data->method_data.push_back(PaymentMethodData::New());
    event_data->method_data[0]->supported_methods = {supported_method};

    event_data->total = PaymentCurrencyAmount::New();
    event_data->total->currency = "USD";
    event_data->total->value = "55";

    PaymentDetailsModifierPtr modifier = PaymentDetailsModifier::New();
    modifier->total = PaymentItem::New();
    modifier->total->amount = PaymentCurrencyAmount::New();
    modifier->total->amount->currency = "USD";
    modifier->total->amount->value = "55";
    modifier->method_data = PaymentMethodData::New();
    modifier->method_data->supported_methods = {supported_method};
    event_data->modifiers.push_back(std::move(modifier));

    event_data->instrument_key = instrument_key;

    return event_data;
  }

  std::unique_ptr<net::EmbeddedTestServer> https_server_;

  DISALLOW_COPY_AND_ASSIGN(PaymentAppBrowserTest);
};

IN_PROC_BROWSER_TEST_F(PaymentAppBrowserTest,
                       AbortPaymentWithInvalidRegistrationId) {
  RegisterPaymentApp();

  std::vector<int64_t> registrationIds = GetAllPaymentAppRegistrationIDs();
  ASSERT_EQ(1U, registrationIds.size());

  bool payment_aborted =
      AbortPayment(blink::mojom::kInvalidServiceWorkerRegistrationId);
  ASSERT_FALSE(payment_aborted);

  ClearStoragePartitionData();
}

IN_PROC_BROWSER_TEST_F(PaymentAppBrowserTest, AbortPayment) {
  RegisterPaymentApp();

  std::vector<int64_t> registrationIds = GetAllPaymentAppRegistrationIDs();
  ASSERT_EQ(1U, registrationIds.size());

  bool payment_aborted = AbortPayment(registrationIds[0]);
  ASSERT_TRUE(payment_aborted);

  ClearStoragePartitionData();
}

IN_PROC_BROWSER_TEST_F(PaymentAppBrowserTest, CanMakePayment) {
  RegisterPaymentApp();

  std::vector<int64_t> registrationIds = GetAllPaymentAppRegistrationIDs();
  ASSERT_EQ(1U, registrationIds.size());

  bool can_make_payment =
      CanMakePaymentWithTestData(registrationIds[0], "basic-card");
  ASSERT_TRUE(can_make_payment);

  ClearStoragePartitionData();

  EXPECT_EQ("https://example.com/", PopConsoleString() /* topLevelOrigin */);
  EXPECT_EQ("https://example.com/",
            PopConsoleString() /* paymentRequestOrigin */);
  EXPECT_EQ("[{\"supportedMethods\":[\"basic-card\"]}]",
            PopConsoleString() /* methodData */);
  EXPECT_EQ(
      "[{\"additionalDisplayItems\":[],\"supportedMethods\":[\"basic-card\"],"
      "\"total\":{\"amount\":{\"currency\":\"USD\",\"currencySystem\":\"urn:"
      "iso:std:iso:4217\",\"value\":\"55\"},\"label\":\"\",\"pending\":false}}"
      "]",
      PopConsoleString() /* modifiers */);
}

IN_PROC_BROWSER_TEST_F(PaymentAppBrowserTest, PaymentAppInvocationAndFailed) {
  RegisterPaymentApp();

  std::vector<int64_t> registrationIds = GetAllPaymentAppRegistrationIDs();
  ASSERT_EQ(1U, registrationIds.size());

  // Remove all payment apps and service workers to cause error.
  ClearStoragePartitionData();

  PaymentHandlerResponsePtr response(InvokePaymentAppWithTestData(
      registrationIds[0], "basic-card", "basic-card-payment-app-id"));
  ASSERT_EQ("", response->method_name);

  ClearStoragePartitionData();
}

IN_PROC_BROWSER_TEST_F(PaymentAppBrowserTest, PaymentAppInvocation) {
  RegisterPaymentApp();

  std::vector<int64_t> registrationIds = GetAllPaymentAppRegistrationIDs();
  ASSERT_EQ(1U, registrationIds.size());

  PaymentHandlerResponsePtr response(InvokePaymentAppWithTestData(
      registrationIds[0], "basic-card", "basic-card-payment-app-id"));
  ASSERT_EQ("test", response->method_name);

  ClearStoragePartitionData();

  registrationIds = GetAllPaymentAppRegistrationIDs();
  ASSERT_EQ(0U, registrationIds.size());

  EXPECT_EQ("https://example.com/", PopConsoleString() /* topLevelOrigin */);
  EXPECT_EQ("https://example.com/",
            PopConsoleString() /* paymentRequestOrigin */);
  EXPECT_EQ("payment-request-id", PopConsoleString() /* paymentRequestId */);
  EXPECT_EQ("[{\"supportedMethods\":[\"basic-card\"]}]",
            PopConsoleString() /* methodData */);
  EXPECT_EQ(
      "{\"currency\":\"USD\",\"currencySystem\":\"urn:iso:std:iso:4217\","
      "\"value\":\"55\"}",
      PopConsoleString() /* total */);
  EXPECT_EQ(
      "[{\"additionalDisplayItems\":[],\"supportedMethods\":[\"basic-card\"],"
      "\"total\":{\"amount\":{\"currency\":\"USD\",\"currencySystem\":\"urn:"
      "iso:std:iso:4217\",\"value\":\"55\"},\"label\":\"\",\"pending\":false}}"
      "]",
      PopConsoleString() /* modifiers */);
  EXPECT_EQ("basic-card-payment-app-id",
            PopConsoleString() /* instrumentKey */);
}

IN_PROC_BROWSER_TEST_F(PaymentAppBrowserTest, PaymentAppOpenWindowFailed) {
  RegisterPaymentApp();

  std::vector<int64_t> registrationIds = GetAllPaymentAppRegistrationIDs();
  ASSERT_EQ(1U, registrationIds.size());

  PaymentHandlerResponsePtr response(InvokePaymentAppWithTestData(
      registrationIds[0], "https://bobpay.com", "bobpay-payment-app-id"));
  // InvokePaymentAppCallback returns empty method_name in case of failure, like
  // in PaymentRequestRespondWithObserver::OnResponseRejected.
  ASSERT_EQ("", response->method_name);

  ClearStoragePartitionData();

  registrationIds = GetAllPaymentAppRegistrationIDs();
  ASSERT_EQ(0U, registrationIds.size());

  EXPECT_EQ("https://example.com/", PopConsoleString() /* topLevelOrigin */);
  EXPECT_EQ("https://example.com/",
            PopConsoleString() /* paymentRequestOrigin */);
  EXPECT_EQ("payment-request-id", PopConsoleString() /* paymentRequestId */);
  EXPECT_EQ("[{\"supportedMethods\":[\"https://bobpay.com\"]}]",
            PopConsoleString() /* methodData */);
  EXPECT_EQ(
      "{\"currency\":\"USD\",\"currencySystem\":\"urn:iso:std:iso:4217\","
      "\"value\":\"55\"}",
      PopConsoleString() /* total */);
  EXPECT_EQ(
      "[{\"additionalDisplayItems\":[],\"supportedMethods\":[\"https://"
      "bobpay.com\"],"
      "\"total\":{\"amount\":{\"currency\":\"USD\",\"currencySystem\":\"urn:"
      "iso:std:iso:4217\",\"value\":\"55\"},\"label\":\"\",\"pending\":false}}"
      "]",
      PopConsoleString() /* modifiers */);
  EXPECT_EQ("bobpay-payment-app-id", PopConsoleString() /* instrumentKey */);
}
}  // namespace content
