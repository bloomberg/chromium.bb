// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/webauth/authenticator_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/browser_test.h"
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

using webauth::mojom::AuthenticatorPtr;
using webauth::mojom::AuthenticatorStatus;
using webauth::mojom::GetAssertionAuthenticatorResponsePtr;
using webauth::mojom::MakeCredentialAuthenticatorResponsePtr;

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
        webauth::mojom::AttestationConveyancePreference::NONE);

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
        std::move(credentials), base::nullopt);

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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_;
  std::unique_ptr<content::AuthenticatorImpl> authenticator_impl_;

  DISALLOW_COPY_AND_ASSIGN(WebAuthBrowserTest);
};

class MockCreateCallback {
 public:
  MockCreateCallback() = default;
  MOCK_METHOD0_T(Run, void());

  using MakeCredentialCallback =
      base::OnceCallback<void(AuthenticatorStatus,
                              MakeCredentialAuthenticatorResponsePtr)>;

  void RunWrapper(AuthenticatorStatus unused,
                  MakeCredentialAuthenticatorResponsePtr unused2) {
    Run();
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
  MOCK_METHOD0_T(Run, void());

  using GetAssertionCallback =
      base::OnceCallback<void(AuthenticatorStatus,
                              GetAssertionAuthenticatorResponsePtr)>;

  void RunWrapper(AuthenticatorStatus unused,
                  GetAssertionAuthenticatorResponsePtr unused2) {
    Run();
  }

  GetAssertionCallback Get() {
    return base::BindOnce(&MockGetCallback::RunWrapper, base::Unretained(this));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockGetCallback);
};

// Tests.

// Tests that no crash occurs when navigating away during a pending
// create(publicKey) request.
IN_PROC_BROWSER_TEST_F(WebAuthBrowserTest,
                       CreatePublicKeyCredentialNavigateAway) {
  const GURL a_url1 = GetHttpsURL("www.example.com", "/title1.html");
  const GURL b_url1 = GetHttpsURL("www.test.com", "/title1.html");

  NavigateToURL(shell(), a_url1);

  AuthenticatorPtr authenticator = ConnectToAuthenticatorWithTestConnector();

  MockCreateCallback create_callback;
  EXPECT_CALL(create_callback, Run()).Times(0);
  authenticator->MakeCredential(BuildBasicCreateOptions(),
                                create_callback.Get());

  ASSERT_NO_FATAL_FAILURE(NavigateToURL(shell(), b_url1));
}

// Tests that no crash occurs when navigating away during a pending
// get(publicKey) request.
IN_PROC_BROWSER_TEST_F(WebAuthBrowserTest, GetPublicKeyCredentialNavigateAway) {
  const GURL a_url1 = GetHttpsURL("www.example.com", "/title1.html");
  const GURL b_url1 = GetHttpsURL("www.test.com", "/title1.html");

  NavigateToURL(shell(), a_url1);

  AuthenticatorPtr authenticator = ConnectToAuthenticatorWithTestConnector();

  MockGetCallback get_callback;
  EXPECT_CALL(get_callback, Run()).Times(0);
  authenticator->GetAssertion(BuildBasicGetOptions(), get_callback.Get());

  ASSERT_NO_FATAL_FAILURE(NavigateToURL(shell(), b_url1));
}

}  // namespace content
