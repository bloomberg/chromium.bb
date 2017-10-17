// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/authenticator_impl.h"

#include <stdint.h>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_render_frame_host.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using ::testing::_;

using webauth::mojom::AuthenticatorPtr;
using webauth::mojom::AuthenticatorStatus;
using webauth::mojom::MakeCredentialOptions;
using webauth::mojom::MakeCredentialOptionsPtr;
using webauth::mojom::PublicKeyCredentialEntity;
using webauth::mojom::PublicKeyCredentialEntityPtr;
using webauth::mojom::PublicKeyCredentialInfoPtr;
using webauth::mojom::PublicKeyCredentialParameters;
using webauth::mojom::PublicKeyCredentialParametersPtr;

namespace {

constexpr char kOrigin1[] = "https://google.com";
constexpr int32_t kES256 = -7;

PublicKeyCredentialEntityPtr GetTestPublicKeyCredentialRPEntity() {
  auto entity = PublicKeyCredentialEntity::New();
  entity->id = std::string("localhost");
  entity->name = std::string("TestRP@example.com");
  return entity;
}

PublicKeyCredentialEntityPtr GetTestPublicKeyCredentialUserEntity() {
  auto entity = PublicKeyCredentialEntity::New();
  entity->display_name = std::string("User A. Name");
  entity->id = std::string("1098237235409872");
  entity->name = std::string("username@example.com");
  entity->icon = GURL("fakeurl2.png");
  return entity;
}

std::vector<PublicKeyCredentialParametersPtr>
GetTestPublicKeyCredentialParameters() {
  std::vector<PublicKeyCredentialParametersPtr> parameters;
  auto fake_parameter = PublicKeyCredentialParameters::New();
  fake_parameter->type = webauth::mojom::PublicKeyCredentialType::PUBLIC_KEY;
  fake_parameter->algorithm_identifier = kES256;
  parameters.push_back(std::move(fake_parameter));
  return parameters;
}

MakeCredentialOptionsPtr GetTestMakeCredentialOptions() {
  auto options = MakeCredentialOptions::New();
  std::vector<uint8_t> buffer(32, 0x0A);
  options->relying_party = GetTestPublicKeyCredentialRPEntity();
  options->user = GetTestPublicKeyCredentialUserEntity();
  options->crypto_parameters = GetTestPublicKeyCredentialParameters();
  options->challenge = std::move(buffer);
  options->adjusted_timeout = base::TimeDelta::FromMinutes(1);
  return options;
}

class AuthenticatorImplTest : public content::RenderViewHostTestHarness {
 public:
  AuthenticatorImplTest() {}
  ~AuthenticatorImplTest() override {}

 protected:
  // Simulates navigating to a page and getting the page contents and language
  // for that navigation.
  void SimulateNavigation(const GURL& url) {
    if (main_rfh()->GetLastCommittedURL() != url)
      NavigateAndCommit(url);
  }

  AuthenticatorPtr ConnectToAuthenticator() {
    AuthenticatorPtr authenticator;
    AuthenticatorImpl::Create(main_rfh(), mojo::MakeRequest(&authenticator));
    return authenticator;
  }
};

class TestMakeCredentialCallback {
 public:
  TestMakeCredentialCallback()
      : callback_(base::Bind(&TestMakeCredentialCallback::ReceivedCallback,
                             base::Unretained(this))) {}
  ~TestMakeCredentialCallback() {}

  void ReceivedCallback(AuthenticatorStatus status,
                        PublicKeyCredentialInfoPtr credential) {
    response_ = std::make_pair(status, std::move(credential));
    closure_.Run();
  }

  std::pair<AuthenticatorStatus, PublicKeyCredentialInfoPtr>&
  WaitForCallback() {
    closure_ = run_loop_.QuitClosure();
    run_loop_.Run();
    return response_;
  }

  const base::Callback<void(AuthenticatorStatus status,
                            PublicKeyCredentialInfoPtr credential)>&
  callback() {
    return callback_;
  }

 private:
  std::pair<AuthenticatorStatus, PublicKeyCredentialInfoPtr> response_;
  base::Closure closure_;
  base::Callback<void(AuthenticatorStatus status,
                      PublicKeyCredentialInfoPtr credential)>
      callback_;
  base::RunLoop run_loop_;
};

}  // namespace

// Test that service returns NOT_IMPLEMENTED on a call to MakeCredential.
TEST_F(AuthenticatorImplTest, MakeCredentialNotImplemented) {
  SimulateNavigation(GURL(kOrigin1));
  AuthenticatorPtr authenticator = ConnectToAuthenticator();
  MakeCredentialOptionsPtr options = GetTestMakeCredentialOptions();

  TestMakeCredentialCallback cb;
  authenticator->MakeCredential(std::move(options), cb.callback());
  std::pair<webauth::mojom::AuthenticatorStatus,
            webauth::mojom::PublicKeyCredentialInfoPtr>& response =
      cb.WaitForCallback();
  EXPECT_EQ(webauth::mojom::AuthenticatorStatus::NOT_IMPLEMENTED,
            response.first);
}

// Test that service returns NOT_ALLOWED_ERROR on a call to MakeCredential with
// an opaque origin.
TEST_F(AuthenticatorImplTest, MakeCredentialOpaqueOrigin) {
  NavigateAndCommit(GURL("data:text/html,opaque"));
  AuthenticatorPtr authenticator = ConnectToAuthenticator();

  MakeCredentialOptionsPtr options = GetTestMakeCredentialOptions();

  TestMakeCredentialCallback cb;
  authenticator->MakeCredential(std::move(options), cb.callback());
  std::pair<webauth::mojom::AuthenticatorStatus,
            webauth::mojom::PublicKeyCredentialInfoPtr>& response =
      cb.WaitForCallback();
  EXPECT_EQ(webauth::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR,
            response.first);
}

}  // namespace content
