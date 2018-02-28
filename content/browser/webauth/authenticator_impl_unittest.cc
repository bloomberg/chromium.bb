// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/authenticator_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/json/json_parser.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "content/public/browser/render_frame_host.h"
#include "content/test/test_render_frame_host.h"
#include "device/fido/fake_hid_impl_for_testing.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using ::testing::_;

using webauth::mojom::AuthenticatorPtr;
using webauth::mojom::AuthenticatorStatus;
using webauth::mojom::GetAssertionAuthenticatorResponsePtr;
using webauth::mojom::MakeCredentialAuthenticatorResponsePtr;
using webauth::mojom::PublicKeyCredentialCreationOptions;
using webauth::mojom::PublicKeyCredentialCreationOptionsPtr;
using webauth::mojom::PublicKeyCredentialParameters;
using webauth::mojom::PublicKeyCredentialParametersPtr;
using webauth::mojom::PublicKeyCredentialRequestOptions;
using webauth::mojom::PublicKeyCredentialRequestOptionsPtr;
using webauth::mojom::PublicKeyCredentialRpEntity;
using webauth::mojom::PublicKeyCredentialRpEntityPtr;
using webauth::mojom::PublicKeyCredentialUserEntity;
using webauth::mojom::PublicKeyCredentialUserEntityPtr;

namespace {

typedef struct {
  const char* origin;
  const char* relying_party_id;
} OriginRelyingPartyIdPair;

constexpr char kTestOrigin1[] = "https://a.google.com";
constexpr char kTestRelyingPartyId[] = "google.com";

// Test data. CBOR test data can be built using the given diagnostic strings
// and the utility at "http://cbor.me/".
constexpr int32_t kCoseEs256 = -7;

constexpr uint8_t kTestChallengeBytes[] = {
    0x68, 0x71, 0x34, 0x96, 0x82, 0x22, 0xEC, 0x17, 0x20, 0x2E, 0x42,
    0x50, 0x5F, 0x8E, 0xD2, 0xB1, 0x6A, 0xE2, 0x2F, 0x16, 0xBB, 0x05,
    0xB8, 0x8C, 0x25, 0xDB, 0x9E, 0x60, 0x26, 0x45, 0xF1, 0x41};

constexpr char kTestRegisterClientDataJsonString[] =
    R"({"challenge":"aHE0loIi7BcgLkJQX47SsWriLxa7BbiMJdueYCZF8UE","origin":)"
    R"("https://a.google.com","tokenBinding":{"status":"not-supported"},)"
    R"("type":"webauthn.create"})";

constexpr char kTestSignClientDataJsonString[] =
    R"({"challenge":"aHE0loIi7BcgLkJQX47SsWriLxa7BbiMJdueYCZF8UE","origin":)"
    R"("https://a.google.com","tokenBinding":{"status":"not-supported"},)"
    R"("type":"webauthn.get"})";

constexpr OriginRelyingPartyIdPair kValidRelyingPartyTestCases[] = {
    {"http://localhost", "localhost"},
    {"https://myawesomedomain", "myawesomedomain"},
    {"https://foo.bar.google.com", "foo.bar.google.com"},
    {"https://foo.bar.google.com", "bar.google.com"},
    {"https://foo.bar.google.com", "google.com"},
    {"https://earth.login.awesomecompany", "login.awesomecompany"},
    {"https://google.com:1337", "google.com"},

    // Hosts with trailing dot valid for rpIds with or without trailing dot.
    // Hosts without trailing dots only matches rpIDs without trailing dot.
    // Two trailing dots only matches rpIDs with two trailing dots.
    {"https://google.com.", "google.com"},
    {"https://google.com.", "google.com."},
    {"https://google.com..", "google.com.."},

    // Leading dots are ignored in canonicalized hosts.
    {"https://.google.com", "google.com"},
    {"https://..google.com", "google.com"},
    {"https://.google.com", ".google.com"},
    {"https://..google.com", ".google.com"},
    {"https://accounts.google.com", ".google.com"},

    // The spec notes that RPs should not use non-https schemes, but this is
    // technically still valid according to the authoritative parts.
    {"wss:///google.com", "google.com"},
};

constexpr OriginRelyingPartyIdPair kInvalidRelyingPartyTestCases[] = {
    {"https://google.com", "com"},
    {"http://google.com", "google.com"},
    {"http://myawesomedomain", "myawesomedomain"},
    {"https://google.com", "foo.bar.google.com"},
    {"http://myawesomedomain", "randomdomain"},
    {"https://myawesomedomain", "randomdomain"},
    {"https://notgoogle.com", "google.com)"},
    {"https://not-google.com", "google.com)"},
    {"https://evil.appspot.com", "appspot.com"},
    {"https://evil.co.uk", "co.uk"},

    {"https://google.com", "google.com."},
    {"https://google.com", "google.com.."},
    {"https://google.com", ".google.com"},
    {"https://google.com..", "google.com"},
    {"https://.com", "com."},
    {"https://.co.uk", "co.uk."},

    {"https://1.2.3", "1.2.3"},
    {"https://1.2.3", "2.3"},

    {"https://127.0.0.1", "127.0.0.1"},
    {"https://127.0.0.1", "27.0.0.1"},
    {"https://127.0.0.1", ".0.0.1"},
    {"https://127.0.0.1", "0.0.1"},

    {"https://[::127.0.0.1]", "127.0.0.1"},
    {"https://[::127.0.0.1]", "[127.0.0.1]"},

    {"https://[::1]", "1"},
    {"https://[::1]", "1]"},
    {"https://[::1]", "::1"},
    {"https://[::1]", "[::1]"},
    {"https://[1::1]", "::1"},
    {"https://[1::1]", "::1]"},
    {"https://[1::1]", "[::1]"},

    {"http://google.com:443", "google.com"},
    {"data:google.com", "google.com"},
    {"data:text/html,google.com", "google.com"},
    {"ws://google.com", "google.com"},
    {"gopher://google.com", "google.com"},
    {"ftp://google.com", "google.com"},
    {"file:///google.com", "google.com"},

    {"data:,", ""},
    {"https://google.com", ""},
    {"ws:///google.com", ""},
    {"wss:///google.com", ""},
    {"gopher://google.com", ""},
    {"ftp://google.com", ""},
    {"file:///google.com", ""},

    // This case is acceptable according to spec, but both renderer
    // and browser handling currently do not permit it.
    {"https://login.awesomecompany", "awesomecompany"},
};

std::vector<uint8_t> GetTestChallengeBytes() {
  return std::vector<uint8_t>(std::begin(kTestChallengeBytes),
                              std::end(kTestChallengeBytes));
}

PublicKeyCredentialRpEntityPtr GetTestPublicKeyCredentialRPEntity() {
  auto entity = PublicKeyCredentialRpEntity::New();
  entity->id = std::string(kTestRelyingPartyId);
  entity->name = "TestRP@example.com";
  return entity;
}

PublicKeyCredentialUserEntityPtr GetTestPublicKeyCredentialUserEntity() {
  auto entity = PublicKeyCredentialUserEntity::New();
  entity->display_name = "User A. Name";
  std::vector<uint8_t> id(32, 0x0A);
  entity->id = id;
  entity->name = "username@example.com";
  entity->icon = GURL("fakeurl2.png");
  return entity;
}

std::vector<PublicKeyCredentialParametersPtr>
GetTestPublicKeyCredentialParameters(int32_t algorithm_identifier) {
  std::vector<PublicKeyCredentialParametersPtr> parameters;
  auto fake_parameter = PublicKeyCredentialParameters::New();
  fake_parameter->type = webauth::mojom::PublicKeyCredentialType::PUBLIC_KEY;
  fake_parameter->algorithm_identifier = algorithm_identifier;
  parameters.push_back(std::move(fake_parameter));
  return parameters;
}

PublicKeyCredentialCreationOptionsPtr
GetTestPublicKeyCredentialCreationOptions() {
  auto options = PublicKeyCredentialCreationOptions::New();
  options->relying_party = GetTestPublicKeyCredentialRPEntity();
  options->user = GetTestPublicKeyCredentialUserEntity();
  options->public_key_parameters =
      GetTestPublicKeyCredentialParameters(kCoseEs256);
  options->challenge.assign(32, 0x0A);
  options->adjusted_timeout = base::TimeDelta::FromMinutes(1);
  return options;
}

PublicKeyCredentialRequestOptionsPtr
GetTestPublicKeyCredentialRequestOptions() {
  auto options = PublicKeyCredentialRequestOptions::New();
  options->relying_party_id = std::string(kTestRelyingPartyId);
  options->challenge.assign(32, 0x0A);
  options->adjusted_timeout = base::TimeDelta::FromMinutes(1);
  return options;
}

class TestMakeCredentialCallback {
 public:
  TestMakeCredentialCallback()
      : callback_(base::BindOnce(&TestMakeCredentialCallback::ReceivedCallback,
                                 base::Unretained(this))) {}
  ~TestMakeCredentialCallback() {}

  void ReceivedCallback(AuthenticatorStatus status,
                        MakeCredentialAuthenticatorResponsePtr credential) {
    response_ = std::make_pair(status, std::move(credential));
    std::move(closure_).Run();
  }

  // TODO(crbug.com/799044) - simplify the runloop usage.
  void WaitForCallback() {
    closure_ = run_loop_.QuitClosure();
    run_loop_.Run();
  }

  AuthenticatorImpl::MakeCredentialCallback callback() {
    return std::move(callback_);
  }

  AuthenticatorStatus GetResponseStatus() { return response_.first; }

 private:
  std::pair<AuthenticatorStatus, MakeCredentialAuthenticatorResponsePtr>
      response_;
  base::Closure closure_;
  AuthenticatorImpl::MakeCredentialCallback callback_;
  base::RunLoop run_loop_;
};

class TestGetAssertionCallback {
 public:
  TestGetAssertionCallback()
      : callback_(base::BindOnce(&TestGetAssertionCallback::ReceivedCallback,
                                 base::Unretained(this))) {}
  ~TestGetAssertionCallback() {}

  void ReceivedCallback(AuthenticatorStatus status,
                        GetAssertionAuthenticatorResponsePtr credential) {
    response_ = std::make_pair(status, std::move(credential));
    std::move(closure_).Run();
  }

  // TODO(crbug.com/799044) - simplify the runloop usage.
  void WaitForCallback() {
    closure_ = run_loop_.QuitClosure();
    run_loop_.Run();
  }

  AuthenticatorStatus GetResponseStatus() { return response_.first; }

  AuthenticatorImpl::GetAssertionCallback callback() {
    return std::move(callback_);
  }

 private:
  std::pair<AuthenticatorStatus, GetAssertionAuthenticatorResponsePtr>
      response_;
  base::OnceClosure closure_;
  AuthenticatorImpl::GetAssertionCallback callback_;
  base::RunLoop run_loop_;
};

}  // namespace

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
    authenticator_impl_ = std::make_unique<AuthenticatorImpl>(main_rfh());
    AuthenticatorPtr authenticator;
    authenticator_impl_->Bind(mojo::MakeRequest(&authenticator));
    return authenticator;
  }

  AuthenticatorPtr ConnectToAuthenticator(
      service_manager::Connector* connector,
      std::unique_ptr<base::OneShotTimer> timer) {
    authenticator_impl_.reset(
        new AuthenticatorImpl(main_rfh(), connector, std::move(timer)));
    AuthenticatorPtr authenticator;
    authenticator_impl_->Bind(mojo::MakeRequest(&authenticator));
    return authenticator;
  }

  url::Origin GetTestOrigin() {
    const GURL test_relying_party_url(kTestOrigin1);
    CHECK(test_relying_party_url.is_valid());
    return url::Origin::Create(test_relying_party_url);
  }

  std::string GetTestClientDataJSON(std::string type) {
    return AuthenticatorImpl::SerializeCollectedClientDataToJson(
        std::move(type), GetTestOrigin(), GetTestChallengeBytes(),
        base::nullopt);
  }

  std::string GetTokenBindingTestClientDataJSON(
      base::Optional<base::span<const uint8_t>> token_binding) {
    return AuthenticatorImpl::SerializeCollectedClientDataToJson(
        client_data::kGetType, GetTestOrigin(), GetTestChallengeBytes(),
        token_binding);
  }

 private:
  std::unique_ptr<AuthenticatorImpl> authenticator_impl_;
};

// Verify behavior for various combinations of origins and RP IDs.
TEST_F(AuthenticatorImplTest, MakeCredentialOriginAndRpIds) {
  // These instances should return security errors (for circumstances
  // that would normally crash the renderer).
  for (auto test_case : kInvalidRelyingPartyTestCases) {
    SCOPED_TRACE(std::string(test_case.relying_party_id) + " " +
                 std::string(test_case.origin));

    NavigateAndCommit(GURL(test_case.origin));
    AuthenticatorPtr authenticator = ConnectToAuthenticator();
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->relying_party->id = test_case.relying_party_id;
    TestMakeCredentialCallback cb;
    authenticator->MakeCredential(std::move(options), cb.callback());
    cb.WaitForCallback();
    EXPECT_EQ(webauth::mojom::AuthenticatorStatus::INVALID_DOMAIN,
              cb.GetResponseStatus());
  }

  // These instances pass the origin and relying party checks and return at
  // the algorithm check.
  for (auto test_case : kValidRelyingPartyTestCases) {
    SCOPED_TRACE(std::string(test_case.relying_party_id) + " " +
                 std::string(test_case.origin));

    NavigateAndCommit(GURL(test_case.origin));
    AuthenticatorPtr authenticator = ConnectToAuthenticator();
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->relying_party->id = test_case.relying_party_id;
    options->public_key_parameters = GetTestPublicKeyCredentialParameters(123);

    TestMakeCredentialCallback cb;
    authenticator->MakeCredential(std::move(options), cb.callback());
    cb.WaitForCallback();
    EXPECT_EQ(webauth::mojom::AuthenticatorStatus::NOT_SUPPORTED_ERROR,
              cb.GetResponseStatus());
  }
}

// Test that service returns NOT_IMPLEMENTED_ERROR if no parameters contain
// a supported algorithm.
TEST_F(AuthenticatorImplTest, MakeCredentialNoSupportedAlgorithm) {
  SimulateNavigation(GURL(kTestOrigin1));
  AuthenticatorPtr authenticator = ConnectToAuthenticator();

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->public_key_parameters = GetTestPublicKeyCredentialParameters(123);

  TestMakeCredentialCallback cb;
  authenticator->MakeCredential(std::move(options), cb.callback());
  cb.WaitForCallback();
  EXPECT_EQ(webauth::mojom::AuthenticatorStatus::NOT_SUPPORTED_ERROR,
            cb.GetResponseStatus());
}

// Parses its arguments as JSON and expects that all the keys in the first are
// also in the second, and with the same value.
void CheckJSONIsSubsetOfJSON(base::StringPiece subset_str,
                             base::StringPiece test_str) {
  std::unique_ptr<base::Value> subset(base::JSONReader::Read(subset_str));
  ASSERT_TRUE(subset);
  ASSERT_TRUE(subset->is_dict());
  std::unique_ptr<base::Value> test(base::JSONReader::Read(test_str));
  ASSERT_TRUE(test);
  ASSERT_TRUE(test->is_dict());

  for (const auto& item : subset->DictItems()) {
    base::Value* test_value = test->FindKey(item.first);
    if (test_value == nullptr) {
      ADD_FAILURE() << item.first << " does not exist in the test dictionary";
      continue;
    }

    if (!item.second.Equals(test_value)) {
      std::string want, got;
      ASSERT_TRUE(base::JSONWriter::Write(item.second, &want));
      ASSERT_TRUE(base::JSONWriter::Write(*test_value, &got));
      ADD_FAILURE() << "Value of " << item.first << " is unequal: want " << want
                    << " got " << got;
    }
  }
}

// Test that client data serializes to JSON properly.
TEST_F(AuthenticatorImplTest, TestSerializedRegisterClientData) {
  CheckJSONIsSubsetOfJSON(kTestRegisterClientDataJsonString,
                          GetTestClientDataJSON(client_data::kCreateType));
}

TEST_F(AuthenticatorImplTest, TestSerializedSignClientData) {
  CheckJSONIsSubsetOfJSON(kTestSignClientDataJsonString,
                          GetTestClientDataJSON(client_data::kGetType));
}

TEST_F(AuthenticatorImplTest, TestTokenBindingClientData) {
  const std::vector<
      std::pair<base::Optional<std::vector<uint8_t>>, const char*>>
      kTestCases = {
          std::make_pair(base::nullopt,
                         R"({"tokenBinding":{"status":"not-supported"}})"),
          std::make_pair(std::vector<uint8_t>{},
                         R"({"tokenBinding":{"status":"supported"}})"),
          std::make_pair(
              std::vector<uint8_t>{1, 2, 3, 4},
              R"({"tokenBinding":{"status":"present","id":"AQIDBA"}})"),
      };

  for (const auto& test : kTestCases) {
    const auto& token_binding = test.first;
    const std::string expected_json_subset = test.second;
    SCOPED_TRACE(expected_json_subset);

    CheckJSONIsSubsetOfJSON(expected_json_subset,
                            GetTokenBindingTestClientDataJSON(token_binding));
  }
}

TEST_F(AuthenticatorImplTest, TestMakeCredentialTimeout) {
  SimulateNavigation(GURL(kTestOrigin1));
  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  TestMakeCredentialCallback cb;

  // Set up service_manager::Connector for tests.
  auto fake_hid_manager = std::make_unique<device::FakeHidManager>();
  service_manager::mojom::ConnectorRequest request;
  auto connector = service_manager::Connector::Create(&request);
  service_manager::Connector::TestApi test_api(connector.get());
  test_api.OverrideBinderForTesting(
      service_manager::Identity(device::mojom::kServiceName),
      device::mojom::HidManager::Name_,
      base::Bind(&device::FakeHidManager::AddBinding,
                 base::Unretained(fake_hid_manager.get())));

  // Set up a timer for testing.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
      base::Time::Now(), base::TimeTicks::Now());
  auto tick_clock = task_runner->GetMockTickClock();
  auto timer = std::make_unique<base::OneShotTimer>(tick_clock.get());
  timer->SetTaskRunner(task_runner);
  AuthenticatorPtr authenticator =
      ConnectToAuthenticator(connector.get(), std::move(timer));

  authenticator->MakeCredential(std::move(options), cb.callback());

  // Trigger timer.
  base::RunLoop().RunUntilIdle();
  task_runner->FastForwardBy(base::TimeDelta::FromMinutes(1));
  cb.WaitForCallback();
  EXPECT_EQ(webauth::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR,
            cb.GetResponseStatus());
}

// Verify behavior for various combinations of origins and RP IDs.
TEST_F(AuthenticatorImplTest, GetAssertionOriginAndRpIds) {
  // These instances should return security errors (for circumstances
  // that would normally crash the renderer).
  for (const OriginRelyingPartyIdPair& test_case :
       kInvalidRelyingPartyTestCases) {
    SCOPED_TRACE(std::string(test_case.relying_party_id) + " " +
                 std::string(test_case.origin));

    NavigateAndCommit(GURL(test_case.origin));
    AuthenticatorPtr authenticator = ConnectToAuthenticator();
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->relying_party_id = test_case.relying_party_id;

    TestGetAssertionCallback cb;
    authenticator->GetAssertion(std::move(options), cb.callback());
    cb.WaitForCallback();
    EXPECT_EQ(webauth::mojom::AuthenticatorStatus::INVALID_DOMAIN,
              cb.GetResponseStatus());
  }
}

typedef struct {
  const char* origin;
  const char* appid;
  bool should_succeed;
} OriginAppIdPair;

constexpr OriginAppIdPair kInvalidAppIdCases[] = {
    {"https://example.com", "https://com/foo", false},
    {"https://example.com", "https://foo.com/", false},
    {"https://example.com", "http://example.com", false},
    {"http://example.com", "https://example.com", false},
    {"https://127.0.0.1", "https://127.0.0.1", false},
    {"https://www.notgoogle.com",
     "https://www.gstatic.com/securitykey/origins.json", false},

    /* Cannot be tested yet:
    {"https://example.com", "https://example.com", true},
    {"https://www.example.com", "https://example.com", true},
    {"https://example.com", "https://www.example.com", true},
    {"https://example.com", "https://foo.bar.example.com", true},
    {"https://example.com", "https://foo.bar.example.com/foo/bar", true},
    {"https://www.google.com",
     "https://www.gstatic.com/securitykey/origins.json", true},
    {"https://www.google.com",
     "https://www.gstatic.com/securitykey/a/google.com/origins.json", true},
     */
};

// Verify behavior for various combinations of origins and RP IDs.
TEST_F(AuthenticatorImplTest, AppIdExtension) {
  for (const auto& test_case : kInvalidAppIdCases) {
    SCOPED_TRACE(std::string(test_case.origin) + " " +
                 std::string(test_case.appid));

    CHECK(!test_case.should_succeed) << "can't test this yet";

    const GURL origin_url(test_case.origin);
    NavigateAndCommit(origin_url);
    AuthenticatorPtr authenticator = ConnectToAuthenticator();
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->relying_party_id = origin_url.host();
    options->appid = std::string(test_case.appid);

    TestGetAssertionCallback cb;
    authenticator->GetAssertion(std::move(options), cb.callback());
    cb.WaitForCallback();
    EXPECT_EQ(webauth::mojom::AuthenticatorStatus::INVALID_DOMAIN,
              cb.GetResponseStatus());
  }

  // TODO(agl): test positive cases once a mock U2F device exists.
}

TEST_F(AuthenticatorImplTest, TestGetAssertionTimeout) {
  SimulateNavigation(GURL(kTestOrigin1));
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  TestGetAssertionCallback cb;

  // Set up service_manager::Connector for tests.
  auto fake_hid_manager = std::make_unique<device::FakeHidManager>();
  service_manager::mojom::ConnectorRequest request;
  auto connector = service_manager::Connector::Create(&request);
  service_manager::Connector::TestApi test_api(connector.get());
  test_api.OverrideBinderForTesting(
      service_manager::Identity(device::mojom::kServiceName),
      device::mojom::HidManager::Name_,
      base::Bind(&device::FakeHidManager::AddBinding,
                 base::Unretained(fake_hid_manager.get())));

  // Set up a timer for testing.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
      base::Time::Now(), base::TimeTicks::Now());
  auto tick_clock = task_runner->GetMockTickClock();
  auto timer = std::make_unique<base::OneShotTimer>(tick_clock.get());
  timer->SetTaskRunner(task_runner);
  AuthenticatorPtr authenticator =
      ConnectToAuthenticator(connector.get(), std::move(timer));

  authenticator->GetAssertion(std::move(options), cb.callback());

  // Trigger timer.
  base::RunLoop().RunUntilIdle();
  task_runner->FastForwardBy(base::TimeDelta::FromMinutes(1));
  cb.WaitForCallback();
  EXPECT_EQ(webauth::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR,
            cb.GetResponseStatus());
}
}  // namespace content
