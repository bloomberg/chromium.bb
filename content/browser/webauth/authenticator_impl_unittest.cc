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
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using ::testing::_;

using webauth::mojom::RelyingPartyAccount;
using webauth::mojom::ScopedCredentialOptions;
using webauth::mojom::ScopedCredentialParameters;
using webauth::mojom::AuthenticatorPtr;
using webauth::mojom::AuthenticatorStatus;
using webauth::mojom::RelyingPartyAccountPtr;
using webauth::mojom::ScopedCredentialInfoPtr;
using webauth::mojom::ScopedCredentialOptionsPtr;
using webauth::mojom::ScopedCredentialParametersPtr;

const char* kOrigin1 = "https://google.com";

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
    AuthenticatorImpl::Create(main_rfh(), service_manager::BindSourceInfo(),
                              mojo::MakeRequest(&authenticator));
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
                        ScopedCredentialInfoPtr credential) {
    response_ = std::make_pair(status, std::move(credential));
    closure_.Run();
  }

  std::pair<AuthenticatorStatus, ScopedCredentialInfoPtr>& WaitForCallback() {
    closure_ = run_loop_.QuitClosure();
    run_loop_.Run();
    return response_;
  }

  const base::Callback<void(AuthenticatorStatus status,
                            ScopedCredentialInfoPtr credential)>&
  callback() {
    return callback_;
  }

 private:
  std::pair<AuthenticatorStatus, ScopedCredentialInfoPtr> response_;
  base::Closure closure_;
  base::Callback<void(AuthenticatorStatus status,
                      ScopedCredentialInfoPtr credential)>
      callback_;
  base::RunLoop run_loop_;
};

RelyingPartyAccountPtr GetTestRelyingPartyAccount() {
  RelyingPartyAccountPtr account = RelyingPartyAccount::New();
  account->relying_party_display_name = std::string("TestRP");
  account->display_name = std::string("Test A. Name");
  account->id = std::string("1098237235409872");
  account->name = std::string("Testname@example.com");
  account->image_url = std::string("fakeurl.png");
  return account;
}

std::vector<ScopedCredentialParametersPtr> GetTestScopedCredentialParameters() {
  std::vector<ScopedCredentialParametersPtr> parameters;
  auto fake_parameter = ScopedCredentialParameters::New();
  fake_parameter->type = webauth::mojom::ScopedCredentialType::SCOPEDCRED;
  parameters.push_back(std::move(fake_parameter));
  return parameters;
}

ScopedCredentialOptionsPtr GetTestScopedCredentialOptions() {
  ScopedCredentialOptionsPtr opts = ScopedCredentialOptions::New();
  opts->adjusted_timeout = 60;
  opts->relying_party_id = std::string("localhost");
  return opts;
}

// Test that service returns NOT_IMPLEMENTED on a call to MakeCredential.
TEST_F(AuthenticatorImplTest, MakeCredentialNotImplemented) {
  SimulateNavigation(GURL(kOrigin1));
  AuthenticatorPtr authenticator = ConnectToAuthenticator();

  RelyingPartyAccountPtr account = GetTestRelyingPartyAccount();

  std::vector<ScopedCredentialParametersPtr> parameters =
      GetTestScopedCredentialParameters();

  std::vector<uint8_t> buffer(32, 0x0A);
  ScopedCredentialOptionsPtr opts = GetTestScopedCredentialOptions();

  TestMakeCredentialCallback cb;
  authenticator->MakeCredential(std::move(account), std::move(parameters),
                                buffer, std::move(opts), cb.callback());
  std::pair<webauth::mojom::AuthenticatorStatus,
            webauth::mojom::ScopedCredentialInfoPtr>& response =
      cb.WaitForCallback();
  EXPECT_EQ(webauth::mojom::AuthenticatorStatus::NOT_IMPLEMENTED,
            response.first);
}

// Test that service returns NOT_ALLOWED_ERROR on a call to MakeCredential with
// an opaque origin.
TEST_F(AuthenticatorImplTest, MakeCredentialOpaqueOrigin) {
  NavigateAndCommit(GURL("data:text/html,opaque"));
  AuthenticatorPtr authenticator = ConnectToAuthenticator();
  RelyingPartyAccountPtr account = GetTestRelyingPartyAccount();

  std::vector<ScopedCredentialParametersPtr> parameters =
      GetTestScopedCredentialParameters();

  std::vector<uint8_t> buffer(32, 0x0A);
  ScopedCredentialOptionsPtr opts = GetTestScopedCredentialOptions();

  TestMakeCredentialCallback cb;
  authenticator->MakeCredential(std::move(account), std::move(parameters),
                                buffer, std::move(opts), cb.callback());
  std::pair<webauth::mojom::AuthenticatorStatus,
            webauth::mojom::ScopedCredentialInfoPtr>& response =
      cb.WaitForCallback();
  EXPECT_EQ(webauth::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR,
            response.first);
}
}  // namespace content
