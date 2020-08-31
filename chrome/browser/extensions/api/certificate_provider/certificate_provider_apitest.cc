// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash/sha1.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/chromeos/certificate_provider/test_certificate_provider_extension.h"
#include "chrome/browser/chromeos/ui/request_pin_view.h"
#include "chrome/browser/extensions/api/certificate_provider/certificate_provider_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "crypto/rsa_private_key.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_background_page_first_load_observer.h"
#include "net/cert/x509_certificate.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

using testing::Return;
using testing::_;

namespace {

void IgnoreResult(const base::Closure& callback, base::Value value) {
  callback.Run();
}

void StoreBool(bool* result, const base::Closure& callback, base::Value value) {
  value.GetAsBoolean(result);
  callback.Run();
}

void StoreDigest(std::vector<uint8_t>* digest,
                 const base::Closure& callback,
                 base::Value value) {
  ASSERT_TRUE(value.is_blob()) << "Unexpected value in StoreDigest";
  digest->assign(value.GetBlob().begin(), value.GetBlob().end());
  callback.Run();
}

bool RsaSign(const std::vector<uint8_t>& digest,
             crypto::RSAPrivateKey* key,
             std::vector<uint8_t>* signature) {
  RSA* rsa_key = EVP_PKEY_get0_RSA(key->key());
  if (!rsa_key)
    return false;

  unsigned len = 0;
  signature->resize(RSA_size(rsa_key));
  if (!RSA_sign(NID_sha1, digest.data(), digest.size(), signature->data(), &len,
                rsa_key)) {
    signature->clear();
    return false;
  }
  signature->resize(len);
  return true;
}

// Create a string that if evaluated in JavaScript returns a Uint8Array with
// |bytes| as content.
std::string JsUint8Array(const std::vector<uint8_t>& bytes) {
  std::string res = "new Uint8Array([";
  for (const uint8_t byte : bytes) {
    res += base::NumberToString(byte);
    res += ", ";
  }
  res += "])";
  return res;
}

std::string GetPageTextContent(content::WebContents* web_contents) {
  base::RunLoop run_loop;
  std::string text_content;
  web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("document.body.textContent;"),
      base::BindOnce(
          [](std::string* result, base::OnceClosure callback,
             base::Value value) {
            if (value.is_string())
              *result = value.GetString();
            std::move(callback).Run();
          },
          &text_content, run_loop.QuitClosure()));
  run_loop.Run();
  return text_content;
}

std::string GetCertFingerprint1(const net::X509Certificate& cert) {
  unsigned char hash[base::kSHA1Length];
  base::SHA1HashBytes(CRYPTO_BUFFER_data(cert.cert_buffer()),
                      CRYPTO_BUFFER_len(cert.cert_buffer()), hash);
  return base::ToLowerASCII(base::HexEncode(hash, base::kSHA1Length));
}

class CertificateProviderApiTest : public extensions::ExtensionApiTest {
 public:
  CertificateProviderApiTest() {}

  void SetUpInProcessBrowserTestFixture() override {
    EXPECT_CALL(provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);

    extensions::ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    // Set up the AutoSelectCertificateForUrls policy to avoid the client
    // certificate selection dialog.
    const std::string autoselect_pattern = R"({"pattern": "*", "filter": {}})";

    std::unique_ptr<base::ListValue> autoselect_policy(new base::ListValue);
    autoselect_policy->AppendString(autoselect_pattern);

    policy::PolicyMap policy;
    policy.Set(policy::key::kAutoSelectCertificateForUrls,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, std::move(autoselect_policy),
               nullptr);
    provider_.UpdateChromePolicy(policy);

    content::RunAllPendingInMessageLoop();
  }

 protected:
  policy::MockConfigurationPolicyProvider provider_;
};

class CertificateProviderRequestPinTest : public CertificateProviderApiTest {
 protected:
  static constexpr int kFakeSignRequestId = 123;
  static constexpr int kWrongPinAttemptsLimit = 3;
  static constexpr const char* kCorrectPin = "1234";
  static constexpr const char* kWrongPin = "567";

  void SetUpOnMainThread() override {
    CertificateProviderApiTest::SetUpOnMainThread();
    cert_provider_service_ =
        chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
            profile());
    command_request_listener_ = std::make_unique<ExtensionTestMessageListener>(
        "GetCommand", /*will_reply=*/true);
    LoadRequestPinExtension();
  }

  void TearDownOnMainThread() override {
    if (command_request_listener_->was_satisfied()) {
      // Avoid destroying a non-replied extension function without.
      command_request_listener_->Reply(/*message=*/std::string());
    }
    command_request_listener_.reset();
    CertificateProviderApiTest::TearDownOnMainThread();
  }

  std::string pin_request_extension_id() const { return extension_->id(); }

  void AddFakeSignRequest(int sign_request_id) {
    cert_provider_service_->pin_dialog_manager()->AddSignRequestId(
        extension_->id(), sign_request_id, {});
  }

  void NavigateTo(const std::string& test_page_file_name) {
    ui_test_utils::NavigateToURL(
        browser(), extension_->GetResourceURL(test_page_file_name));
  }

  chromeos::RequestPinView* GetActivePinDialogView() {
    return cert_provider_service_->pin_dialog_manager()
        ->default_dialog_host_for_testing()
        ->active_view_for_testing();
  }

  views::Widget* GetActivePinDialogWindow() {
    return cert_provider_service_->pin_dialog_manager()
        ->default_dialog_host_for_testing()
        ->active_window_for_testing();
  }

  // Enters the code in the ShowPinDialog window and pushes the OK event.
  void EnterCode(const std::string& code) {
    GetActivePinDialogView()->textfield_for_testing()->SetText(
        base::ASCIIToUTF16(code));
    GetActivePinDialogView()->Accept();
    base::RunLoop().RunUntilIdle();
  }

  // Enters the valid code for extensions from local example folders, in the
  // ShowPinDialog window and waits for the window to close. The extension code
  // is expected to send "Success" message after the validation and request to
  // stopPinRequest is done.
  void EnterCorrectPinAndWaitForMessage() {
    ExtensionTestMessageListener listener("Success", false);
    EnterCode(kCorrectPin);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }

  // Enters an invalid code for extensions from local example folders, in the
  // ShowPinDialog window and waits for the window to update with the error. The
  // extension code is expected to send "Invalid PIN" message after the
  // validation and the new requestPin (with the error) is done.
  void EnterWrongPinAndWaitForMessage() {
    ExtensionTestMessageListener listener("Invalid PIN", false);
    EnterCode(kWrongPin);
    ASSERT_TRUE(listener.WaitUntilSatisfied());

    // Check that we have an error message displayed.
    EXPECT_EQ(
        gfx::kGoogleRed600,
        GetActivePinDialogView()->error_label_for_testing()->GetEnabledColor());
  }

  bool SendCommand(const std::string& command) {
    if (!command_request_listener_->WaitUntilSatisfied())
      return false;
    command_request_listener_->Reply(command);
    command_request_listener_->Reset();
    return true;
  }

  bool SendCommandAndWaitForMessage(const std::string& command,
                                    const std::string& expected_message) {
    ExtensionTestMessageListener listener(expected_message,
                                          /*will_reply=*/false);
    if (!SendCommand(command))
      return false;
    return listener.WaitUntilSatisfied();
  }

 private:
  void LoadRequestPinExtension() {
    const base::FilePath extension_path =
        test_data_dir_.AppendASCII("certificate_provider/request_pin");
    extension_ = LoadExtension(extension_path);
  }

  chromeos::CertificateProviderService* cert_provider_service_ = nullptr;
  const extensions::Extension* extension_ = nullptr;
  std::unique_ptr<ExtensionTestMessageListener> command_request_listener_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(CertificateProviderApiTest, Basic) {
  // Start an HTTPS test server that requests a client certificate.
  net::SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  net::SpawnedTestServer https_server(net::SpawnedTestServer::TYPE_HTTPS,
                                      ssl_options, base::FilePath());
  ASSERT_TRUE(https_server.Start());

  extensions::ResultCatcher catcher;

  const base::FilePath extension_path =
      test_data_dir_.AppendASCII("certificate_provider");
  const extensions::Extension* const extension = LoadExtension(extension_path);
  ui_test_utils::NavigateToURL(browser(),
                               extension->GetResourceURL("basic.html"));

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  VLOG(1) << "Extension registered. Navigate to the test https page.";

  content::WebContents* const extension_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::TestNavigationObserver navigation_observer(
      nullptr /* no WebContents */);
  navigation_observer.StartWatchingNewWebContents();
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server.GetURL("client-cert"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_NONE);

  content::WebContents* const https_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  VLOG(1) << "Wait for the extension to respond to the certificates request.";
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  VLOG(1) << "Wait for the extension to receive the sign request.";
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  VLOG(1) << "Fetch the digest from the sign request.";
  std::vector<uint8_t> request_digest;
  {
    base::RunLoop run_loop;
    extension_contents->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16("signDigestRequest.digest;"),
        base::BindOnce(&StoreDigest, &request_digest, run_loop.QuitClosure()));
    run_loop.Run();
  }

  VLOG(1) << "Sign the digest using the private key.";
  std::string key_pk8;
  {
    base::ScopedAllowBlockingForTesting allow_io;
    base::ReadFileToString(extension_path.AppendASCII("l1_leaf.pk8"), &key_pk8);
  }

  const uint8_t* const key_pk8_begin =
      reinterpret_cast<const uint8_t*>(key_pk8.data());
  std::unique_ptr<crypto::RSAPrivateKey> key(
      crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(
          std::vector<uint8_t>(key_pk8_begin, key_pk8_begin + key_pk8.size())));
  ASSERT_TRUE(key);

  std::vector<uint8_t> signature;
  EXPECT_TRUE(RsaSign(request_digest, key.get(), &signature));

  VLOG(1) << "Inject the signature back to the extension and let it reply.";
  {
    base::RunLoop run_loop;
    const std::string code =
        "replyWithSignature(" + JsUint8Array(signature) + ");";
    extension_contents->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16(code),
        base::BindOnce(&IgnoreResult, run_loop.QuitClosure()));
    run_loop.Run();
  }

  VLOG(1) << "Wait for the https navigation to finish.";
  navigation_observer.Wait();

  VLOG(1) << "Check whether the server acknowledged that a client certificate "
             "was presented.";
  // The fingerprint can be calculated independently using:
  // openssl x509 -inform DER -noout -fingerprint -in
  //   chrome/test/data/extensions/api_test/certificate_provider/l1_leaf.der
  EXPECT_EQ(GetPageTextContent(https_contents),
            "got client cert with fingerprint: "
            "edeb84ab3b5a36dd09a3203c74794b25efa8f126");

  // Replying to the same signature request a second time must fail.
  {
    base::RunLoop run_loop;
    const std::string code = "replyWithSignatureSecondTime();";
    bool result = false;
    extension_contents->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16(code),
        base::BindOnce(&StoreBool, &result, run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(result);
  }
}

// Test that the certificateProvider events are delivered correctly in the
// scenario when the event listener is in a lazy background page that gets idle.
IN_PROC_BROWSER_TEST_F(CertificateProviderApiTest, LazyBackgroundPage) {
  // Make extension background pages idle immediately.
  extensions::ProcessManager::SetEventPageIdleTimeForTesting(1);
  extensions::ProcessManager::SetEventPageSuspendingTimeForTesting(1);

  // Start an HTTPS test server that requests a client certificate.
  net::SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  net::SpawnedTestServer https_server(net::SpawnedTestServer::TYPE_HTTPS,
                                      ssl_options, base::FilePath());
  ASSERT_TRUE(https_server.Start());

  // Load the test extension.
  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  const extensions::Extension* const extension =
      LoadExtension(test_data_dir.AppendASCII("extensions")
                        .AppendASCII("test_certificate_provider")
                        .AppendASCII("extension"));
  ASSERT_TRUE(extension);
  std::unique_ptr<TestCertificateProviderExtension>
      test_certificate_provider_extension;
  {
    base::ScopedAllowBlockingForTesting allow_io;
    test_certificate_provider_extension =
        std::make_unique<TestCertificateProviderExtension>(profile(),
                                                           extension->id());
  }
  extensions::TestBackgroundPageFirstLoadObserver(profile(), extension->id())
      .Wait();

  // Navigate to the page that requests the client authentication. Use the
  // incognito profile in order to force re-authentication in the later request
  // made by the test.
  const GURL client_cert_url = https_server.GetURL("client-cert");
  const std::string client_cert_fingerprint =
      GetCertFingerprint1(*TestCertificateProviderExtension::GetCertificate());
  Browser* const incognito_browser = CreateIncognitoBrowser(profile());
  ASSERT_TRUE(incognito_browser);
  ui_test_utils::NavigateToURLWithDisposition(
      incognito_browser, client_cert_url,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_EQ(test_certificate_provider_extension->certificate_request_count(),
            1);
  EXPECT_EQ(GetPageTextContent(
                incognito_browser->tab_strip_model()->GetActiveWebContents()),
            "got client cert with fingerprint: " + client_cert_fingerprint);

  // Let the extension's background page become idle.
  WaitForExtensionIdle(extension->id());

  // Navigate again to the page with the client authentication. The extension
  // gets awakened and handles the request.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), client_cert_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_EQ(test_certificate_provider_extension->certificate_request_count(),
            2);
  EXPECT_EQ(
      GetPageTextContent(browser()->tab_strip_model()->GetActiveWebContents()),
      "got client cert with fingerprint: " + client_cert_fingerprint);
}

// User enters the correct PIN.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest, ShowPinDialogAccept) {
  AddFakeSignRequest(kFakeSignRequestId);
  NavigateTo("basic.html");

  // Enter the valid PIN.
  EnterCorrectPinAndWaitForMessage();

  // The view should be set to nullptr when the window is closed.
  EXPECT_FALSE(GetActivePinDialogView());
}

// User closes the dialog kMaxClosedDialogsPerMinute times, and the extension
// should be blocked from showing it again.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest, ShowPinDialogClose) {
  AddFakeSignRequest(kFakeSignRequestId);
  NavigateTo("basic.html");

  for (int i = 0;
       i < extensions::api::certificate_provider::kMaxClosedDialogsPerMinute;
       i++) {
    ExtensionTestMessageListener listener("User closed the dialog", false);
    GetActivePinDialogWindow()->Close();
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }

  ExtensionTestMessageListener close_listener("User closed the dialog", true);
  GetActivePinDialogWindow()->Close();
  ASSERT_TRUE(close_listener.WaitUntilSatisfied());
  close_listener.Reply("GetLastError");
  ExtensionTestMessageListener last_error_listener(
      "This request exceeds the MAX_PIN_DIALOGS_CLOSED_PER_MINUTE quota.",
      false);
  ASSERT_TRUE(last_error_listener.WaitUntilSatisfied());
  EXPECT_FALSE(GetActivePinDialogView());
}

// User enters a wrong PIN first and a correct PIN on the second try.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest,
                       ShowPinDialogWrongPin) {
  AddFakeSignRequest(kFakeSignRequestId);
  NavigateTo("basic.html");
  EnterWrongPinAndWaitForMessage();

  // The window should be active.
  EXPECT_TRUE(GetActivePinDialogWindow()->IsVisible());
  EXPECT_TRUE(GetActivePinDialogView());

  // Enter the valid PIN.
  EnterCorrectPinAndWaitForMessage();

  // The view should be set to nullptr when the window is closed.
  EXPECT_FALSE(GetActivePinDialogView());
}

// User enters wrong PIN three times.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest,
                       ShowPinDialogWrongPinThreeTimes) {
  AddFakeSignRequest(kFakeSignRequestId);
  NavigateTo("basic.html");
  for (int i = 0; i < kWrongPinAttemptsLimit; i++)
    EnterWrongPinAndWaitForMessage();

  // The textfield has to be disabled, as extension does not allow input now.
  EXPECT_FALSE(GetActivePinDialogView()->textfield_for_testing()->GetEnabled());

  // Close the dialog.
  ExtensionTestMessageListener listener("No attempt left", false);
  GetActivePinDialogWindow()->Close();
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_FALSE(GetActivePinDialogView());
}

// User closes the dialog while the extension is processing the request.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest,
                       ShowPinDialogCloseWhileProcessing) {
  AddFakeSignRequest(kFakeSignRequestId);
  NavigateTo("operated.html");

  EXPECT_TRUE(SendCommandAndWaitForMessage("Request", "request1:begun"));
  ExtensionTestMessageListener listener(
      base::StringPrintf("request1:success:%s", kWrongPin), false);
  EnterCode(kWrongPin);
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  GetActivePinDialogWindow()->Close();
  base::RunLoop().RunUntilIdle();

  // The view should be set to nullptr when the window is closed.
  EXPECT_FALSE(GetActivePinDialogView());
}

// Extension closes the dialog kMaxClosedDialogsPerMinute times after the user
// inputs some value, and it should be blocked from showing it again.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest,
                       RepeatedProgrammaticCloseAfterInput) {
  NavigateTo("operated.html");

  for (int i = 0;
       i <
       extensions::api::certificate_provider::kMaxClosedDialogsPerMinute + 1;
       i++) {
    AddFakeSignRequest(kFakeSignRequestId);
    EXPECT_TRUE(SendCommandAndWaitForMessage(
        "Request", base::StringPrintf("request%d:begun", i + 1)));

    EnterCode(kCorrectPin);
    EXPECT_TRUE(SendCommandAndWaitForMessage(
        "Stop", base::StringPrintf("stop%d:success", i + 1)));
    EXPECT_FALSE(GetActivePinDialogView());
  }

  AddFakeSignRequest(kFakeSignRequestId);
  EXPECT_TRUE(SendCommandAndWaitForMessage(
      "Request",
      base::StringPrintf(
          "request%d:error:This request exceeds the "
          "MAX_PIN_DIALOGS_CLOSED_PER_MINUTE quota.",
          extensions::api::certificate_provider::kMaxClosedDialogsPerMinute +
              2)));
  EXPECT_FALSE(GetActivePinDialogView());
}

// Extension erroneously attempts to close the PIN dialog twice.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest, DoubleClose) {
  AddFakeSignRequest(kFakeSignRequestId);
  NavigateTo("operated.html");

  EXPECT_TRUE(SendCommand("Request"));
  EXPECT_TRUE(SendCommandAndWaitForMessage("Stop", "stop1:success"));
  EXPECT_TRUE(SendCommandAndWaitForMessage(
      "Stop", "stop2:error:No active dialog from extension."));
  EXPECT_FALSE(GetActivePinDialogView());
}

// Extension closes the dialog kMaxClosedDialogsPerMinute times before the user
// inputs anything, and it should be blocked from showing it again.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest,
                       RepeatedProgrammaticCloseBeforeInput) {
  NavigateTo("operated.html");

  for (int i = 0;
       i <
       extensions::api::certificate_provider::kMaxClosedDialogsPerMinute + 1;
       i++) {
    AddFakeSignRequest(kFakeSignRequestId);
    EXPECT_TRUE(SendCommand("Request"));
    EXPECT_TRUE(SendCommandAndWaitForMessage(
        "Stop", base::StringPrintf("stop%d:success", i + 1)));
    EXPECT_FALSE(GetActivePinDialogView());
  }

  AddFakeSignRequest(kFakeSignRequestId);
  EXPECT_TRUE(SendCommandAndWaitForMessage(
      "Request",
      base::StringPrintf(
          "request%d:error:This request exceeds the "
          "MAX_PIN_DIALOGS_CLOSED_PER_MINUTE quota.",
          extensions::api::certificate_provider::kMaxClosedDialogsPerMinute +
              2)));
  EXPECT_FALSE(GetActivePinDialogView());
}

// Extension erroneously attempts to stop the PIN request with an error before
// the user provided any input.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest,
                       StopWithErrorBeforeInput) {
  AddFakeSignRequest(kFakeSignRequestId);
  NavigateTo("operated.html");

  EXPECT_TRUE(SendCommand("Request"));
  EXPECT_TRUE(SendCommandAndWaitForMessage(
      "StopWithUnknownError", "stop1:error:No user input received"));
  EXPECT_TRUE(GetActivePinDialogView()->textfield_for_testing()->GetEnabled());
}

// Extension erroneously uses an invalid sign request ID.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest, InvalidRequestId) {
  NavigateTo("operated.html");

  EXPECT_TRUE(SendCommandAndWaitForMessage(
      "Request", "request1:error:Invalid signRequestId"));
  EXPECT_FALSE(GetActivePinDialogView());
}

// Extension specifies zero left attempts in the very first PIN request.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest, ZeroAttemptsAtStart) {
  AddFakeSignRequest(kFakeSignRequestId);
  NavigateTo("operated.html");

  EXPECT_TRUE(SendCommandAndWaitForMessage("RequestWithZeroAttempts",
                                           "request1:begun"));

  // The textfield has to be disabled, as there are no attempts left.
  EXPECT_FALSE(GetActivePinDialogView()->textfield_for_testing()->GetEnabled());

  ExtensionTestMessageListener listener("request1:empty", false);
  GetActivePinDialogWindow()->Close();
  EXPECT_TRUE(listener.WaitUntilSatisfied());
}

// Extension erroneously passes a negative attempts left count.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest, NegativeAttempts) {
  AddFakeSignRequest(kFakeSignRequestId);
  NavigateTo("operated.html");

  EXPECT_TRUE(SendCommandAndWaitForMessage(
      "RequestWithNegativeAttempts", "request1:error:Invalid attemptsLeft"));
  EXPECT_FALSE(GetActivePinDialogView());
}

// Extension erroneously attempts to close a non-existing dialog.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest, CloseNonExisting) {
  AddFakeSignRequest(kFakeSignRequestId);
  NavigateTo("operated.html");

  EXPECT_TRUE(SendCommandAndWaitForMessage(
      "Stop", "stop1:error:No active dialog from extension."));
  EXPECT_FALSE(GetActivePinDialogView());
}

// Extension erroneously attempts to stop a non-existing dialog with an error.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest, StopNonExisting) {
  AddFakeSignRequest(kFakeSignRequestId);
  NavigateTo("operated.html");

  EXPECT_TRUE(SendCommandAndWaitForMessage(
      "StopWithUnknownError", "stop1:error:No active dialog from extension."));
  EXPECT_FALSE(GetActivePinDialogView());
}

// Extension erroneously attempts to start or stop the PIN request before the
// user closed the previously stopped with an error PIN request.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest,
                       UpdateAlreadyStopped) {
  AddFakeSignRequest(kFakeSignRequestId);
  NavigateTo("operated.html");

  EXPECT_TRUE(SendCommandAndWaitForMessage("Request", "request1:begun"));
  EnterCode(kWrongPin);
  EXPECT_TRUE(SendCommand("StopWithUnknownError"));

  EXPECT_TRUE(SendCommandAndWaitForMessage(
      "StopWithUnknownError", "stop2:error:No user input received"));
  EXPECT_TRUE(SendCommandAndWaitForMessage(
      "Request", "request2:error:Previous request not finished"));
  EXPECT_FALSE(GetActivePinDialogView()->textfield_for_testing()->GetEnabled());
}

// Extension starts a new PIN request after it stopped the previous one with an
// error.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest, StartAfterStop) {
  AddFakeSignRequest(kFakeSignRequestId);
  NavigateTo("operated.html");

  EXPECT_TRUE(SendCommandAndWaitForMessage("Request", "request1:begun"));
  EnterCode(kWrongPin);
  EXPECT_TRUE(SendCommandAndWaitForMessage("Stop", "stop1:success"));
  EXPECT_FALSE(GetActivePinDialogView());

  EXPECT_TRUE(SendCommandAndWaitForMessage("Request", "request2:begun"));
  ExtensionTestMessageListener listener(
      base::StringPrintf("request2:success:%s", kCorrectPin), false);
  EnterCode(kCorrectPin);
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_FALSE(GetActivePinDialogView()->textfield_for_testing()->GetEnabled());
}

// Test that no quota is applied to the first PIN requests for each requestId.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest,
                       RepeatedCloseWithDifferentIds) {
  NavigateTo("operated.html");

  for (int i = 0;
       i <
       extensions::api::certificate_provider::kMaxClosedDialogsPer10Minutes + 2;
       i++) {
    AddFakeSignRequest(kFakeSignRequestId + i);
    EXPECT_TRUE(SendCommandAndWaitForMessage(
        "Request", base::StringPrintf("request%d:begun", i + 1)));

    ExtensionTestMessageListener listener(
        base::StringPrintf("request%d:empty", i + 1), false);
    ASSERT_TRUE(GetActivePinDialogView());
    GetActivePinDialogView()->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kCloseButtonClicked);
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_FALSE(GetActivePinDialogView());

    EXPECT_TRUE(SendCommand("IncrementRequestId"));
  }
}

// Test that disabling the extension closes its PIN dialog.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest, ExtensionDisable) {
  AddFakeSignRequest(kFakeSignRequestId);
  NavigateTo("operated.html");

  EXPECT_TRUE(SendCommandAndWaitForMessage("Request", "request1:begun"));
  EXPECT_TRUE(GetActivePinDialogView());

  extensions::TestExtensionRegistryObserver registry_observer(
      extensions::ExtensionRegistry::Get(profile()),
      pin_request_extension_id());
  extensions::ExtensionSystem::Get(profile())
      ->extension_service()
      ->DisableExtension(pin_request_extension_id(),
                         extensions::disable_reason::DISABLE_USER_ACTION);
  registry_observer.WaitForExtensionUnloaded();
  // Let the events from the extensions subsystem propagate to the code that
  // manages the PIN dialog.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(GetActivePinDialogView());
}

// Test that reloading the extension closes its PIN dialog.
IN_PROC_BROWSER_TEST_F(CertificateProviderRequestPinTest, ExtensionReload) {
  AddFakeSignRequest(kFakeSignRequestId);
  NavigateTo("operated.html");

  EXPECT_TRUE(SendCommandAndWaitForMessage("Request", "request1:begun"));
  EXPECT_TRUE(GetActivePinDialogView());

  // Create a second browser, in order to suppress Chrome shutdown logic when
  // reloading the extension (as the tab with the extension's file gets closed).
  CreateBrowser(profile());

  // Trigger the chrome.runtime.reload() call from the extension.
  extensions::TestExtensionRegistryObserver registry_observer(
      extensions::ExtensionRegistry::Get(profile()),
      pin_request_extension_id());
  EXPECT_TRUE(SendCommand("Reload"));
  registry_observer.WaitForExtensionUnloaded();
  registry_observer.WaitForExtensionLoaded();

  EXPECT_FALSE(GetActivePinDialogView());
}
