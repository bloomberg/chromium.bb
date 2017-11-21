// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/payments/content/service_worker_payment_app_factory.h"
#include "components/payments/core/test_payment_manifest_downloader.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

class PaymentRequestPaymentAppTest : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestPaymentAppTest()
      : alicepay_(net::EmbeddedTestServer::TYPE_HTTPS),
        bobpay_(net::EmbeddedTestServer::TYPE_HTTPS),
        frankpay_(net::EmbeddedTestServer::TYPE_HTTPS) {
    scoped_feature_list_.InitAndEnableFeature(
        features::kServiceWorkerPaymentApps);
  }

  // Starts the test severs and opens a test page on alicepay.com.
  void SetUpOnMainThread() override {
    PaymentRequestBrowserTestBase::SetUpOnMainThread();

    ASSERT_TRUE(StartTestServer("alicepay.com", &alicepay_));
    ASSERT_TRUE(StartTestServer("bobpay.com", &bobpay_));
    ASSERT_TRUE(StartTestServer("frankpay.com", &frankpay_));
  }

  // Invokes the JavaScript function install(|method_name|) in
  // components/test/data/payments/alicepay.com/app1/index.js, which responds
  // back via domAutomationController.
  void InstallAlicePayForMethod(const std::string& method_name) {
    ui_test_utils::NavigateToURL(browser(),
                                 alicepay_.GetURL("alicepay.com", "/app1/"));

    std::string contents;
    std::string script = "install('" + method_name + "');";
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetActiveWebContents(), script,
        &contents))
        << "Script execution failed: " << script;
    ASSERT_NE(std::string::npos,
              contents.find("Payment app for \"" + method_name +
                            "\" method installed."))
        << method_name << " method install message not found in:\n"
        << contents;
  }

  // Sets a TestDownloader for alicepay.com, bobpay.com and frankpay.com to
  // ServiceWorkerPaymentAppFactory, and ignores port in app scope.
  void SetDownloaderAndIgnorePortInAppScopeForTesting() {
    content::BrowserContext* context = browser()
                                           ->tab_strip_model()
                                           ->GetActiveWebContents()
                                           ->GetBrowserContext();
    auto downloader = std::make_unique<TestDownloader>(
        content::BrowserContext::GetDefaultStoragePartition(context)
            ->GetURLRequestContext());
    downloader->AddTestServerURL("https://alicepay.com/",
                                 alicepay_.GetURL("alicepay.com", "/"));
    downloader->AddTestServerURL("https://bobpay.com/",
                                 bobpay_.GetURL("bobpay.com", "/"));
    downloader->AddTestServerURL("https://frankpay.com/",
                                 frankpay_.GetURL("frankpay.com", "/"));
    ServiceWorkerPaymentAppFactory::GetInstance()
        ->SetDownloaderAndIgnorePortInAppScopeForTesting(std::move(downloader));
  }

 private:
  // Starts the |test_server| for |hostname|. Returns true on success.
  bool StartTestServer(const std::string& hostname,
                       net::EmbeddedTestServer* test_server) {
    host_resolver()->AddRule(hostname, "127.0.0.1");
    if (!test_server->InitializeAndListen())
      return false;
    test_server->ServeFilesFromSourceDirectory(
        "components/test/data/payments/" + hostname);
    test_server->StartAcceptingConnections();
    return true;
  }

  // https://alicepay.com hosts the payment app.
  net::EmbeddedTestServer alicepay_;

  // https://bobpay.com/webpay does not permit any other origin to use this
  // payment method.
  net::EmbeddedTestServer bobpay_;

  // https://frankpay.com/webpay supports payment apps from any origin.
  net::EmbeddedTestServer frankpay_;

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestPaymentAppTest);
};

// Test payment request methods are not supported by the payment app.
IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentAppTest, NotSupportedError) {
  InstallAlicePayForMethod("https://frankpay.com");

  {
    SetDownloaderAndIgnorePortInAppScopeForTesting();

    NavigateTo("/payment_request_bobpay_test.html");

    ResetEventObserverForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                                   DialogEvent::CAN_MAKE_PAYMENT_RETURNED});
    ASSERT_TRUE(
        content::ExecuteScript(GetActiveWebContents(), "canMakePayment();"));
    WaitForObservedEvent();
    ExpectBodyContains({"false"});

    // A new payment request will be created below, so call
    // SetDownloaderAndIgnorePortInAppScopeForTesting again.
    SetDownloaderAndIgnorePortInAppScopeForTesting();

    ResetEventObserver(DialogEvent::NOT_SUPPORTED_ERROR);
    ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "buy();"));
    WaitForObservedEvent();
    ExpectBodyContains({"NotSupportedError"});
  }

  // Repeat should have identical results.
  {
    SetDownloaderAndIgnorePortInAppScopeForTesting();

    NavigateTo("/payment_request_bobpay_test.html");

    ResetEventObserverForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                                   DialogEvent::CAN_MAKE_PAYMENT_RETURNED});
    ASSERT_TRUE(
        content::ExecuteScript(GetActiveWebContents(), "canMakePayment();"));
    WaitForObservedEvent();
    ExpectBodyContains({"false"});

    // A new payment request will be created below, so call
    // SetDownloaderAndIgnorePortInAppScopeForTesting again.
    SetDownloaderAndIgnorePortInAppScopeForTesting();

    ResetEventObserver(DialogEvent::NOT_SUPPORTED_ERROR);
    ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "buy();"));
    WaitForObservedEvent();
    ExpectBodyContains({"NotSupportedError"});
  }
}

// Test CanMakePayment and payment request can be fullfiled.
IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentAppTest, PayWithAlicePay) {
  InstallAlicePayForMethod("https://alicepay.com");

  {
    SetDownloaderAndIgnorePortInAppScopeForTesting();

    NavigateTo("/payment_request_bobpay_test.html");

    ResetEventObserverForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                                   DialogEvent::CAN_MAKE_PAYMENT_RETURNED});
    ASSERT_TRUE(
        content::ExecuteScript(GetActiveWebContents(), "canMakePayment();"));
    WaitForObservedEvent();
    ExpectBodyContains({"true"});

    // A new payment request will be created below, so call
    // SetDownloaderAndIgnorePortInAppScopeForTesting again.
    SetDownloaderAndIgnorePortInAppScopeForTesting();

    InvokePaymentRequestUI();

    ResetEventObserver(DialogEvent::DIALOG_CLOSED);
    ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());
    ExpectBodyContains({"https://alicepay.com"});
  }

  // Repeat should have identical results.
  {
    SetDownloaderAndIgnorePortInAppScopeForTesting();

    NavigateTo("/payment_request_bobpay_test.html");

    ResetEventObserverForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                                   DialogEvent::CAN_MAKE_PAYMENT_RETURNED});
    ASSERT_TRUE(
        content::ExecuteScript(GetActiveWebContents(), "canMakePayment();"));
    WaitForObservedEvent();
    ExpectBodyContains({"true"});

    // A new payment request will be created below, so call
    // SetDownloaderAndIgnorePortInAppScopeForTesting again.
    SetDownloaderAndIgnorePortInAppScopeForTesting();

    InvokePaymentRequestUI();

    ResetEventObserver(DialogEvent::DIALOG_CLOSED);
    ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());
    ExpectBodyContains({"https://alicepay.com"});
  }
}

// Test https://bobpay.com can not be used by https://alicepay.com
IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentAppTest, CanNotPayWithBobPay) {
  InstallAlicePayForMethod("https://bobpay.com");

  {
    SetDownloaderAndIgnorePortInAppScopeForTesting();

    NavigateTo("/payment_request_bobpay_test.html");

    ResetEventObserverForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                                   DialogEvent::CAN_MAKE_PAYMENT_RETURNED});
    ASSERT_TRUE(
        content::ExecuteScript(GetActiveWebContents(), "canMakePayment();"));
    WaitForObservedEvent();
    ExpectBodyContains({"false"});

    // A new payment request will be created below, so call
    // SetDownloaderAndIgnorePortInAppScopeForTesting again.
    SetDownloaderAndIgnorePortInAppScopeForTesting();

    ResetEventObserver(DialogEvent::NOT_SUPPORTED_ERROR);
    ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "buy();"));
    WaitForObservedEvent();
    ExpectBodyContains({"NotSupportedError"});
  }

  // Repeat should have identical results.
  {
    SetDownloaderAndIgnorePortInAppScopeForTesting();

    NavigateTo("/payment_request_bobpay_test.html");

    ResetEventObserverForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                                   DialogEvent::CAN_MAKE_PAYMENT_RETURNED});
    ASSERT_TRUE(
        content::ExecuteScript(GetActiveWebContents(), "canMakePayment();"));
    WaitForObservedEvent();
    ExpectBodyContains({"false"});

    // A new payment request will be created below, so call
    // SetDownloaderAndIgnorePortInAppScopeForTesting again.
    SetDownloaderAndIgnorePortInAppScopeForTesting();

    ResetEventObserver(DialogEvent::NOT_SUPPORTED_ERROR);
    ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "buy();"));
    WaitForObservedEvent();
    ExpectBodyContains({"NotSupportedError"});
  }
}

// Test can pay with 'basic-card' payment method from alicepay.
IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentAppTest, PayWithBasicCard) {
  InstallAlicePayForMethod("basic-card");

  {
    SetDownloaderAndIgnorePortInAppScopeForTesting();

    NavigateTo(
        "/payment_request_bobpay_and_basic_card_with_modifiers_test.html");
    InvokePaymentRequestUI();

    ResetEventObserver(DialogEvent::DIALOG_CLOSED);
    ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());
    ExpectBodyContains({"basic-card"});
  }

  // Repeat should have identical results.
  {
    SetDownloaderAndIgnorePortInAppScopeForTesting();

    NavigateTo(
        "/payment_request_bobpay_and_basic_card_with_modifiers_test.html");
    InvokePaymentRequestUI();

    ResetEventObserver(DialogEvent::DIALOG_CLOSED);
    ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());
    ExpectBodyContains({"basic-card"});
  }
}

}  // namespace payments
