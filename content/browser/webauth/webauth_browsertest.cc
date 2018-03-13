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
#include "device/fido/fake_hid_impl_for_testing.h"
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

class MockCreateCallback {
 public:
  MockCreateCallback() = default;
  MOCK_METHOD1_T(Run, void(AuthenticatorStatus));

  using MakeCredentialCallback =
      base::OnceCallback<void(AuthenticatorStatus,
                              MakeCredentialAuthenticatorResponsePtr)>;

  void RunWrapper(AuthenticatorStatus status,
                  MakeCredentialAuthenticatorResponsePtr unused_response) {
    Run(status);
  }

  MakeCredentialCallback Get() {
    return base::BindOnce(&MockCreateCallback::RunWrapper,
                          base::Unretained(this));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCreateCallback);
};

class MockGetCallback {
 public:
  MockGetCallback() = default;
  MOCK_METHOD1_T(Run, void(AuthenticatorStatus));

  using GetAssertionCallback =
      base::OnceCallback<void(AuthenticatorStatus,
                              GetAssertionAuthenticatorResponsePtr)>;

  void RunWrapper(AuthenticatorStatus status,
                  GetAssertionAuthenticatorResponsePtr unused_response) {
    Run(status);
  }

  GetAssertionCallback Get() {
    return base::BindOnce(&MockGetCallback::RunWrapper, base::Unretained(this));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockGetCallback);
};

}  // namespace

class WebAuthBrowserTest : public content::ContentBrowserTest {
 public:
  WebAuthBrowserTest() : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    scoped_feature_list_.InitAndEnableFeature(features::kWebAuth);
  }

  void SetUp() override { content::ContentBrowserTest::SetUp(); }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(https_server_.Start());

    NavigateToURL(shell(), GetHttpsURL("www.example.com", "/title1.html"));
    authenticator_ptr_ = ConnectToAuthenticatorWithTestConnector();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  static constexpr int32_t kCOSEAlgorithmIdentifierES256 = -7;

  webauth::mojom::PublicKeyCredentialCreationOptionsPtr
  BuildBasicCreateOptions() {
    auto rp = webauth::mojom::PublicKeyCredentialRpEntity::New(
        "example.com", "example.com", base::nullopt);

    std::vector<uint8_t> kTestUserId{0, 0, 0};
    auto user = webauth::mojom::PublicKeyCredentialUserEntity::New(
        kTestUserId, "name", base::nullopt, "displayName");

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

  GURL GetHttpsURL(const std::string& hostname,
                   const std::string& relative_url) {
    return https_server_.GetURL(hostname, relative_url);
  }

  AuthenticatorPtr ConnectToAuthenticatorWithTestConnector() {
    // Set up service_manager::Connector for tests.
    auto fake_hid_manager = std::make_unique<device::FakeHidManager>();
    service_manager::mojom::ConnectorRequest request;
    auto connector = service_manager::Connector::Create(&request);
    service_manager::Connector::TestApi test_api(connector.get());
    test_api.OverrideBinderForTesting(
        service_manager::Identity(device::mojom::kServiceName),
        device::mojom::HidManager::Name_,
        base::BindRepeating(&device::FakeHidManager::AddBinding,
                            base::Unretained(fake_hid_manager.get())));

    authenticator_impl_.reset(new content::AuthenticatorImpl(
        shell()->web_contents()->GetMainFrame(), connector.get(),
        std::make_unique<base::OneShotTimer>()));
    AuthenticatorPtr authenticator;
    authenticator_impl_->Bind(mojo::MakeRequest(&authenticator));
    return authenticator;
  }

  void ResetAuthenticatorImplAndWaitForConnectionError() {
    EXPECT_TRUE(authenticator_impl_);
    EXPECT_TRUE(authenticator_ptr_);
    EXPECT_TRUE(authenticator_ptr_.is_bound());
    EXPECT_FALSE(authenticator_ptr_.encountered_error());

    base::RunLoop run_loop;
    authenticator_ptr_.set_connection_error_handler(run_loop.QuitClosure());

    authenticator_impl_.reset();
    run_loop.Run();
  }

  AuthenticatorPtr& authenticator() { return authenticator_ptr_; }

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
  net::EmbeddedTestServer https_server_;
  std::unique_ptr<content::AuthenticatorImpl> authenticator_impl_;
  AuthenticatorPtr authenticator_ptr_;

  DISALLOW_COPY_AND_ASSIGN(WebAuthBrowserTest);
};

// Tests that no crash occurs when the implementation is destroyed with a
// pending create(publicKey) request.
IN_PROC_BROWSER_TEST_F(WebAuthBrowserTest,
                       CreatePublicKeyCredentialNavigateAway) {
  MockCreateCallback create_callback;
  EXPECT_CALL(create_callback, Run(::testing::_)).Times(0);

  authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                  create_callback.Get());
  authenticator().FlushForTesting();

  ResetAuthenticatorImplAndWaitForConnectionError();
}

// Tests that no crash occurs when the implementation is destroyed with a
// pending get(publicKey) request.
IN_PROC_BROWSER_TEST_F(WebAuthBrowserTest, GetPublicKeyCredentialNavigateAway) {
  MockGetCallback get_callback;
  EXPECT_CALL(get_callback, Run(::testing::_)).Times(0);

  authenticator()->GetAssertion(BuildBasicGetOptions(), get_callback.Get());
  authenticator().FlushForTesting();

  ResetAuthenticatorImplAndWaitForConnectionError();
}

// Regression test for https://crbug.com/818219.
IN_PROC_BROWSER_TEST_F(WebAuthBrowserTest,
                       CreatePublicKeyCredentialTwiceInARow) {
  MockCreateCallback callback_1;
  MockCreateCallback callback_2;
  EXPECT_CALL(callback_1, Run(::testing::_)).Times(0);
  EXPECT_CALL(callback_2, Run(AuthenticatorStatus::PENDING_REQUEST)).Times(1);
  authenticator()->MakeCredential(BuildBasicCreateOptions(), callback_1.Get());
  authenticator()->MakeCredential(BuildBasicCreateOptions(), callback_2.Get());
  authenticator().FlushForTesting();
}

// Regression test for https://crbug.com/818219.
IN_PROC_BROWSER_TEST_F(WebAuthBrowserTest, GetPublicKeyCredentialTwiceInARow) {
  MockGetCallback callback_1;
  MockGetCallback callback_2;
  EXPECT_CALL(callback_1, Run(::testing::_)).Times(0);
  EXPECT_CALL(callback_2, Run(AuthenticatorStatus::PENDING_REQUEST)).Times(1);
  authenticator()->GetAssertion(BuildBasicGetOptions(), callback_1.Get());
  authenticator()->GetAssertion(BuildBasicGetOptions(), callback_2.Get());
  authenticator().FlushForTesting();
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

}  // namespace content
