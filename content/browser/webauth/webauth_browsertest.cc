// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/webauth/authenticator_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "device/fido/fake_fido_discovery.h"
#include "device/fido/fake_hid_impl_for_testing.h"
#include "device/fido/test_callback_receiver.h"
#include "net/dns/mock_host_resolver.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/modules/webauth/authenticator.mojom.h"

namespace content {

namespace {

using webauth::mojom::AuthenticatorPtr;
using webauth::mojom::AuthenticatorStatus;
using webauth::mojom::GetAssertionAuthenticatorResponsePtr;
using webauth::mojom::MakeCredentialAuthenticatorResponsePtr;

using TestCreateCallbackReceiver =
    ::device::test::StatusAndValueCallbackReceiver<
        AuthenticatorStatus,
        MakeCredentialAuthenticatorResponsePtr>;

using TestGetCallbackReceiver = ::device::test::StatusAndValueCallbackReceiver<
    AuthenticatorStatus,
    GetAssertionAuthenticatorResponsePtr>;

}  // namespace

// Test fixture base class for common tasks.
class WebAuthBrowserTestBase : public content::ContentBrowserTest {
 protected:
  WebAuthBrowserTestBase()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server().ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(https_server().Start());

    NavigateToURL(shell(), GetHttpsURL("www.example.com", "/title1.html"));
    ConnectToAuthenticator(shell()->web_contents());
  }

  void ConnectToAuthenticator(WebContents* web_contents) {
    authenticator_impl_.reset(
        new content::AuthenticatorImpl(web_contents->GetMainFrame()));
    authenticator_impl_->Bind(mojo::MakeRequest(&authenticator_ptr_));
  }

  webauth::mojom::PublicKeyCredentialCreationOptionsPtr
  BuildBasicCreateOptions() {
    auto rp = webauth::mojom::PublicKeyCredentialRpEntity::New(
        "example.com", "example.com", base::nullopt);

    std::vector<uint8_t> kTestUserId{0, 0, 0};
    auto user = webauth::mojom::PublicKeyCredentialUserEntity::New(
        kTestUserId, "name", base::nullopt, "displayName");

    static constexpr int32_t kCOSEAlgorithmIdentifierES256 = -7;
    auto param = webauth::mojom::PublicKeyCredentialParameters::New();
    param->type = webauth::mojom::PublicKeyCredentialType::PUBLIC_KEY;
    param->algorithm_identifier = kCOSEAlgorithmIdentifierES256;
    std::vector<webauth::mojom::PublicKeyCredentialParametersPtr> parameters;
    parameters.push_back(std::move(param));

    std::vector<uint8_t> kTestChallenge{0, 0, 0};
    auto mojo_options = webauth::mojom::PublicKeyCredentialCreationOptions::New(
        std::move(rp), std::move(user), kTestChallenge, std::move(parameters),
        base::TimeDelta::FromSeconds(30),
        std::vector<webauth::mojom::PublicKeyCredentialDescriptorPtr>(),
        nullptr, webauth::mojom::AttestationConveyancePreference::NONE);

    return mojo_options;
  }

  webauth::mojom::PublicKeyCredentialRequestOptionsPtr BuildBasicGetOptions() {
    std::vector<webauth::mojom::PublicKeyCredentialDescriptorPtr> credentials;
    std::vector<webauth::mojom::AuthenticatorTransport> transports;
    transports.push_back(webauth::mojom::AuthenticatorTransport::USB);

    std::vector<uint8_t> kCredentialId{0, 0, 0};
    auto descriptor = webauth::mojom::PublicKeyCredentialDescriptor::New(
        webauth::mojom::PublicKeyCredentialType::PUBLIC_KEY, kCredentialId,
        transports);
    credentials.push_back(std::move(descriptor));

    std::vector<uint8_t> kTestChallenge{0, 0, 0};
    auto mojo_options = webauth::mojom::PublicKeyCredentialRequestOptions::New(
        kTestChallenge, base::TimeDelta::FromSeconds(30), "example.com",
        std::move(credentials),
        webauth::mojom::UserVerificationRequirement::PREFERRED, base::nullopt);

    return mojo_options;
  }

  void SimulateNavigationAndWaitForConnectionError() {
    EXPECT_TRUE(authenticator_impl_);
    EXPECT_TRUE(authenticator_ptr_);
    EXPECT_TRUE(authenticator_ptr_.is_bound());
    EXPECT_FALSE(authenticator_ptr_.encountered_error());

    base::RunLoop run_loop;
    authenticator_ptr_.set_connection_error_handler(run_loop.QuitClosure());

    authenticator_impl_.reset();
    run_loop.Run();
  }

  GURL GetHttpsURL(const std::string& hostname,
                   const std::string& relative_url) {
    return https_server_.GetURL(hostname, relative_url);
  }

  AuthenticatorPtr& authenticator() { return authenticator_ptr_; }
  net::EmbeddedTestServer& https_server() { return https_server_; }
  device::test::ScopedFakeFidoDiscoveryFactory* discovery_factory() {
    return &factory_;
  }

 private:
  net::EmbeddedTestServer https_server_;
  device::test::ScopedFakeFidoDiscoveryFactory factory_;
  std::unique_ptr<content::AuthenticatorImpl> authenticator_impl_;
  AuthenticatorPtr authenticator_ptr_;
};


class WebAuthBrowserTest : public WebAuthBrowserTestBase {
 public:
  WebAuthBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kWebAuth, features::kWebAuthBle}, {});
  }

  // Templates to be used with base::ReplaceStringPlaceholders. Can be
  // modified to include up to 9 replacements.
  base::StringPiece CREATE_PUBLIC_KEY_TEMPLATE =
      "navigator.credentials.create({ publicKey: {"
      "  challenge: new TextEncoder().encode('climb a mountain'),"
      "  rp: { id: 'example.com', name: 'Acme' },"
      "  user: { "
      "    id: new TextEncoder().encode('1098237235409872'),"
      "    name: 'avery.a.jones@example.com',"
      "    displayName: 'Avery A. Jones', "
      "    icon: 'https://pics.acme.com/00/p/aBjjjpqPb.png'},"
      "  pubKeyCredParams: [{ type: 'public-key', alg: '-7'}],"
      "  timeout: 60000,"
      "  excludeCredentials: [],"
      "  authenticatorSelection : { "
      "     requireResidentKey: $1, "
      "     userVerification: '$2' }}"
      "}).catch(c => window.domAutomationController.send(c.toString()));";

  base::StringPiece GET_PUBLIC_KEY_TEMPLATE =
      "navigator.credentials.get({ publicKey: {"
      "  challenge: new TextEncoder().encode('climb a mountain'),"
      "  rp: 'example.com',"
      "  timeout: 60000,"
      "  userVerification: '$1',"
      "  allowCredentials: [{ type: 'public-key',"
      "     id: new TextEncoder().encode('allowedCredential'),"
      "     transports: ['usb', 'nfc', 'ble']}] }"
      "}).catch(c => window.domAutomationController.send(c.toString()));";

  void CreatePublicKeyCredentialWithUserVerificationAndExpectNotSupported(
      content::WebContents* web_contents) {
    std::string result;
    std::vector<std::string> subst;
    subst.push_back("false");
    subst.push_back("required");
    std::string script = base::ReplaceStringPlaceholders(
        CREATE_PUBLIC_KEY_TEMPLATE, subst, nullptr);

    ASSERT_TRUE(
        content::ExecuteScriptAndExtractString(web_contents, script, &result));
    ASSERT_EQ(
        "NotSupportedError: Parameters for this operation are not supported.",
        result);
  }

  void CreatePublicKeyCredentialWithResidentKeyRequiredAndExpectNotSupported(
      content::WebContents* web_contents) {
    std::string result;
    std::vector<std::string> subst;
    subst.push_back("true");
    subst.push_back("preferred");
    std::string script = base::ReplaceStringPlaceholders(
        CREATE_PUBLIC_KEY_TEMPLATE, subst, nullptr);

    ASSERT_TRUE(
        content::ExecuteScriptAndExtractString(web_contents, script, &result));
    ASSERT_EQ(
        "NotSupportedError: Parameters for this operation are not supported.",
        result);
  }

  void GetPublicKeyCredentialWithUserVerificationAndExpectNotSupported(
      content::WebContents* web_contents) {
    std::string result;
    std::vector<std::string> subst;
    subst.push_back("required");
    std::string script = base::ReplaceStringPlaceholders(
        GET_PUBLIC_KEY_TEMPLATE, subst, nullptr);
    ASSERT_TRUE(
        content::ExecuteScriptAndExtractString(web_contents, script, &result));
    ASSERT_EQ(
        "NotSupportedError: Parameters for this operation are not supported.",
        result);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(WebAuthBrowserTest);
};

// A test fixture that does not enable BLE discovery.
class WebAuthBrowserBleDisabledTest : public WebAuthBrowserTestBase {
 public:
  WebAuthBrowserBleDisabledTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kWebAuth);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(WebAuthBrowserBleDisabledTest);
};

// Tests that no crash occurs when the implementation is destroyed with a
// pending create(publicKey) request.
IN_PROC_BROWSER_TEST_F(WebAuthBrowserTest,
                       CreatePublicKeyCredentialNavigateAway) {
  auto* fake_hid_discovery = discovery_factory()->ForgeNextHidDiscovery();
  TestCreateCallbackReceiver create_callback_receiver;
  authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                  create_callback_receiver.callback());

  fake_hid_discovery->WaitForCallToStartAndSimulateSuccess();
  SimulateNavigationAndWaitForConnectionError();
}

// Tests that no crash occurs when the implementation is destroyed with a
// pending get(publicKey) request.
IN_PROC_BROWSER_TEST_F(WebAuthBrowserTest, GetPublicKeyCredentialNavigateAway) {
  auto* fake_hid_discovery = discovery_factory()->ForgeNextHidDiscovery();
  TestGetCallbackReceiver get_callback_receiver;
  authenticator()->GetAssertion(BuildBasicGetOptions(),
                                get_callback_receiver.callback());

  fake_hid_discovery->WaitForCallToStartAndSimulateSuccess();
  SimulateNavigationAndWaitForConnectionError();
}

// Regression test for https://crbug.com/818219.
IN_PROC_BROWSER_TEST_F(WebAuthBrowserTest,
                       CreatePublicKeyCredentialTwiceInARow) {
  TestCreateCallbackReceiver callback_receiver_1;
  TestCreateCallbackReceiver callback_receiver_2;
  authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                  callback_receiver_1.callback());
  authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                  callback_receiver_2.callback());
  callback_receiver_2.WaitForCallback();

  EXPECT_EQ(AuthenticatorStatus::PENDING_REQUEST, callback_receiver_2.status());
  EXPECT_FALSE(callback_receiver_1.was_called());
}

// Regression test for https://crbug.com/818219.
IN_PROC_BROWSER_TEST_F(WebAuthBrowserTest, GetPublicKeyCredentialTwiceInARow) {
  TestGetCallbackReceiver callback_receiver_1;
  TestGetCallbackReceiver callback_receiver_2;
  authenticator()->GetAssertion(BuildBasicGetOptions(),
                                callback_receiver_1.callback());
  authenticator()->GetAssertion(BuildBasicGetOptions(),
                                callback_receiver_2.callback());
  callback_receiver_2.WaitForCallback();

  EXPECT_EQ(AuthenticatorStatus::PENDING_REQUEST, callback_receiver_2.status());
  EXPECT_FALSE(callback_receiver_1.was_called());
}

IN_PROC_BROWSER_TEST_F(WebAuthBrowserTest,
                       CreatePublicKeyCredentialWhileRequestIsPending) {
  auto* fake_hid_discovery = discovery_factory()->ForgeNextHidDiscovery();
  TestCreateCallbackReceiver callback_receiver_1;
  TestCreateCallbackReceiver callback_receiver_2;
  authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                  callback_receiver_1.callback());
  fake_hid_discovery->WaitForCallToStartAndSimulateSuccess();

  authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                  callback_receiver_2.callback());
  callback_receiver_2.WaitForCallback();

  EXPECT_EQ(AuthenticatorStatus::PENDING_REQUEST, callback_receiver_2.status());
  EXPECT_FALSE(callback_receiver_1.was_called());
}

IN_PROC_BROWSER_TEST_F(WebAuthBrowserTest,
                       GetPublicKeyCredentialWhileRequestIsPending) {
  auto* fake_hid_discovery = discovery_factory()->ForgeNextHidDiscovery();
  TestGetCallbackReceiver callback_receiver_1;
  TestGetCallbackReceiver callback_receiver_2;
  authenticator()->GetAssertion(BuildBasicGetOptions(),
                                callback_receiver_1.callback());
  fake_hid_discovery->WaitForCallToStartAndSimulateSuccess();

  authenticator()->GetAssertion(BuildBasicGetOptions(),
                                callback_receiver_2.callback());
  callback_receiver_2.WaitForCallback();

  EXPECT_EQ(AuthenticatorStatus::PENDING_REQUEST, callback_receiver_2.status());
  EXPECT_FALSE(callback_receiver_1.was_called());
}

// Tests that when navigator.credentials.create() is called with unsupported
// authenticator selection criteria, we get a NotSupportedError.
IN_PROC_BROWSER_TEST_F(WebAuthBrowserTest,
                       CreatePublicKeyCredentialUnsupportedSelectionCriteria) {
  ASSERT_NO_FATAL_FAILURE(
      CreatePublicKeyCredentialWithResidentKeyRequiredAndExpectNotSupported(
          shell()->web_contents()));
  ASSERT_NO_FATAL_FAILURE(
      CreatePublicKeyCredentialWithUserVerificationAndExpectNotSupported(
          shell()->web_contents()));
}

// Tests that when navigator.credentials.get() is called with required
// user verification, we get a NotSupportedError.
IN_PROC_BROWSER_TEST_F(WebAuthBrowserTest,
                       GetPublicKeyCredentialUserVerification) {
  ASSERT_NO_FATAL_FAILURE(
      GetPublicKeyCredentialWithUserVerificationAndExpectNotSupported(
          shell()->web_contents()));
}

// WebAuthBrowserBleDisabledTest ------------------------------

// Tests that the BLE discovery does not start when the WebAuthnBle feature
// flag is disabled.
IN_PROC_BROWSER_TEST_F(WebAuthBrowserBleDisabledTest, CheckBleDisabled) {
  auto* fake_hid_discovery = discovery_factory()->ForgeNextHidDiscovery();
  auto* fake_ble_discovery = discovery_factory()->ForgeNextBleDiscovery();

  // Do something that will start discoveries.
  TestCreateCallbackReceiver create_callback_receiver;
  authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                  create_callback_receiver.callback());

  fake_hid_discovery->WaitForCallToStart();
  EXPECT_TRUE(fake_hid_discovery->is_start_requested());
  EXPECT_FALSE(fake_ble_discovery->is_start_requested());
}

}  // namespace content
