// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/webauth/authenticator_impl.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/did_commit_navigation_interceptor.h"
#include "device/base/features.h"
#include "device/fido/fake_fido_discovery.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/hid/fake_hid_impl_for_testing.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/scoped_virtual_fido_device.h"
#include "device/fido/test_callback_receiver.h"
#include "net/dns/mock_host_resolver.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/webauthn/authenticator.mojom.h"

namespace content {

namespace {

using blink::mojom::Authenticator;
using blink::mojom::AuthenticatorPtr;
using blink::mojom::AuthenticatorStatus;
using blink::mojom::GetAssertionAuthenticatorResponsePtr;
using blink::mojom::MakeCredentialAuthenticatorResponsePtr;

using TestCreateCallbackReceiver =
    ::device::test::StatusAndValueCallbackReceiver<
        AuthenticatorStatus,
        MakeCredentialAuthenticatorResponsePtr>;

using TestGetCallbackReceiver = ::device::test::StatusAndValueCallbackReceiver<
    AuthenticatorStatus,
    GetAssertionAuthenticatorResponsePtr>;

constexpr char kPublicKeyErrorMessage[] =
    "webauth: NotSupportedError: Required parameters missing in "
    "`options.publicKey`.";

constexpr char kTimeoutErrorMessage[] =
    "webauth: NotAllowedError: The operation either timed out or was not "
    "allowed. See: https://w3c.github.io/webauthn/#sec-assertion-privacy.";

constexpr char kResidentCredentialsErrorMessage[] =
    "webauth: NotSupportedError: Resident credentials or empty "
    "'allowCredentials' lists are not supported at this time.";

constexpr char kRelyingPartySecurityErrorMessage[] =
    "webauth: SecurityError: The relying party ID 'localhost' is not a "
    "registrable domain suffix of, nor equal to 'https://www.acme.com";

constexpr char kRelyingPartyUserIconUrlSecurityErrorMessage[] =
    "webauth: SecurityError: 'user.icon' should be a secure URL";

constexpr char kRelyingPartyRpIconUrlSecurityErrorMessage[] =
    "webauth: SecurityError: 'rp.icon' should be a secure URL";

// Templates to be used with base::ReplaceStringPlaceholders. Can be
// modified to include up to 9 replacements. The default values for
// any additional replacements added should also be added to the
// CreateParameters struct.
constexpr char kCreatePublicKeyTemplate[] =
    "navigator.credentials.create({ publicKey: {"
    "  challenge: new TextEncoder().encode('climb a mountain'),"
    "  rp: { id: '$3', name: 'Acme', icon: '$7'},"
    "  user: { "
    "    id: new TextEncoder().encode('1098237235409872'),"
    "    name: 'avery.a.jones@example.com',"
    "    displayName: 'Avery A. Jones', "
    "    icon: '$8'},"
    "  pubKeyCredParams: [{ type: 'public-key', alg: '$4'}],"
    "  timeout: 1000,"
    "  excludeCredentials: [],"
    "  authenticatorSelection: {"
    "     requireResidentKey: $1,"
    "     userVerification: '$2',"
    "     authenticatorAttachment: '$5',"
    "  },"
    "  attestation: '$6',"
    "}}).then(c => window.domAutomationController.send('webauth: OK'),"
    "         e => window.domAutomationController.send("
    "                  'webauth: ' + e.toString()));";

constexpr char kPlatform[] = "platform";
constexpr char kCrossPlatform[] = "cross-platform";
constexpr char kPreferredVerification[] = "preferred";
constexpr char kRequiredVerification[] = "required";

// Default values for kCreatePublicKeyTemplate.
struct CreateParameters {
  const char* rp_id = "acme.com";
  bool require_resident_key = false;
  const char* user_verification = kPreferredVerification;
  const char* authenticator_attachment = kCrossPlatform;
  const char* algorithm_identifier = "-7";
  const char* attestation = "none";
  const char* rp_icon = "https://pics.acme.com/00/p/aBjjjpqPb.png";
  const char* user_icon = "https://pics.acme.com/00/p/aBjjjpqPb.png";
};

std::string BuildCreateCallWithParameters(const CreateParameters& parameters) {
  std::vector<std::string> substitutions;
  substitutions.push_back(parameters.require_resident_key ? "true" : "false");
  substitutions.push_back(parameters.user_verification);
  substitutions.push_back(parameters.rp_id);
  substitutions.push_back(parameters.algorithm_identifier);
  substitutions.push_back(parameters.authenticator_attachment);
  substitutions.push_back(parameters.attestation);
  substitutions.push_back(parameters.rp_icon);
  substitutions.push_back(parameters.user_icon);
  return base::ReplaceStringPlaceholders(kCreatePublicKeyTemplate,
                                         substitutions, nullptr);
}

constexpr char kGetPublicKeyTemplate[] =
    "navigator.credentials.get({ publicKey: {"
    "  challenge: new TextEncoder().encode('climb a mountain'),"
    "  rpId: 'acme.com',"
    "  timeout: 1000,"
    "  userVerification: '$1',"
    "  $2}"
    "}).catch(c => window.domAutomationController.send("
    "                  'webauth: ' + c.toString()));";

// Default values for kGetPublicKeyTemplate.
struct GetParameters {
  const char* user_verification = kPreferredVerification;
  const char* allow_credentials =
      "allowCredentials: [{ type: 'public-key',"
      "     id: new TextEncoder().encode('allowedCredential'),"
      "     transports: ['usb', 'nfc', 'ble']}]";
};

std::string BuildGetCallWithParameters(const GetParameters& parameters) {
  std::vector<std::string> substititions;
  substititions.push_back(parameters.user_verification);
  substititions.push_back(parameters.allow_credentials);
  return base::ReplaceStringPlaceholders(kGetPublicKeyTemplate, substititions,
                                         nullptr);
}

// Helper class that executes the given |closure| the very last moment before
// the next navigation commits in a given WebContents.
class ClosureExecutorBeforeNavigationCommit
    : public DidCommitNavigationInterceptor {
 public:
  ClosureExecutorBeforeNavigationCommit(WebContents* web_contents,
                                        base::OnceClosure closure)
      : DidCommitNavigationInterceptor(web_contents),
        closure_(std::move(closure)) {}
  ~ClosureExecutorBeforeNavigationCommit() override = default;

 protected:
  bool WillProcessDidCommitNavigation(
      RenderFrameHost* render_frame_host,
      NavigationRequest* navigation_request,
      ::FrameHostMsg_DidCommitProvisionalLoad_Params* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params)
      override {
    if (closure_)
      std::move(closure_).Run();
    return true;
  }

 private:
  base::OnceClosure closure_;
  DISALLOW_COPY_AND_ASSIGN(ClosureExecutorBeforeNavigationCommit);
};

// Cancels all navigations in a WebContents while in scope.
class ScopedNavigationCancellingThrottleInstaller : public WebContentsObserver {
 public:
  explicit ScopedNavigationCancellingThrottleInstaller(
      WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  ~ScopedNavigationCancellingThrottleInstaller() override = default;

 protected:
  class CancellingThrottle : public NavigationThrottle {
   public:
    explicit CancellingThrottle(NavigationHandle* handle)
        : NavigationThrottle(handle) {}
    ~CancellingThrottle() override = default;

   protected:
    const char* GetNameForLogging() override {
      return "ScopedNavigationCancellingThrottleInstaller::CancellingThrottle";
    }

    ThrottleCheckResult WillStartRequest() override {
      return ThrottleCheckResult(CANCEL);
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(CancellingThrottle);
  };

  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    navigation_handle->RegisterThrottleForTesting(
        std::make_unique<CancellingThrottle>(navigation_handle));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedNavigationCancellingThrottleInstaller);
};

struct WebAuthBrowserTestState {
  // Called when the browser is asked to display an attestation prompt. There is
  // no default so if no callback is installed then the test will crash.
  base::OnceCallback<void(base::OnceCallback<void(bool)>)>
      attestation_prompt_callback_;

  // Set when |IsFocused| is called.
  bool focus_checked = false;

  // This is incremented when an |AuthenticatorRequestClientDelegate| is
  // created.
  int delegate_create_count = 0;
};

class WebAuthBrowserTestClientDelegate
    : public AuthenticatorRequestClientDelegate {
 public:
  explicit WebAuthBrowserTestClientDelegate(WebAuthBrowserTestState* test_state)
      : test_state_(test_state) {}

  void ShouldReturnAttestation(
      const std::string& relying_party_id,
      base::OnceCallback<void(bool)> callback) override {
    std::move(test_state_->attestation_prompt_callback_)
        .Run(std::move(callback));
  }

  bool IsFocused() override {
    test_state_->focus_checked = true;
    return AuthenticatorRequestClientDelegate::IsFocused();
  }

 private:
  WebAuthBrowserTestState* const test_state_;

  DISALLOW_COPY_AND_ASSIGN(WebAuthBrowserTestClientDelegate);
};

// Implements ContentBrowserClient and allows webauthn-related calls to be
// mocked.
class WebAuthBrowserTestContentBrowserClient : public ContentBrowserClient {
 public:
  explicit WebAuthBrowserTestContentBrowserClient(
      WebAuthBrowserTestState* test_state)
      : test_state_(test_state) {}

  std::unique_ptr<AuthenticatorRequestClientDelegate>
  GetWebAuthenticationRequestDelegate(
      RenderFrameHost* render_frame_host) override {
    test_state_->delegate_create_count++;
    return std::make_unique<WebAuthBrowserTestClientDelegate>(test_state_);
  }

 private:
  WebAuthBrowserTestState* const test_state_;

  DISALLOW_COPY_AND_ASSIGN(WebAuthBrowserTestContentBrowserClient);
};

// Test fixture base class for common tasks.
class WebAuthBrowserTestBase : public content::ContentBrowserTest {
 protected:
  WebAuthBrowserTestBase()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  virtual std::vector<base::Feature> GetFeaturesToEnable() {
    return {features::kWebAuth, features::kWebAuthBle};
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    https_server().ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(https_server().Start());

    test_client_.reset(
        new WebAuthBrowserTestContentBrowserClient(&test_state_));
    old_client_ = SetBrowserClientForTesting(test_client_.get());

    NavigateToURL(shell(), GetHttpsURL("www.acme.com", "/title1.html"));
  }

  void TearDown() override {
    CHECK_EQ(SetBrowserClientForTesting(old_client_), test_client_.get());
    ContentBrowserTest::TearDown();
  }

  GURL GetHttpsURL(const std::string& hostname,
                   const std::string& relative_url) {
    return https_server_.GetURL(hostname, relative_url);
  }

  net::EmbeddedTestServer& https_server() { return https_server_; }

  WebAuthBrowserTestState* test_state() { return &test_state_; }

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_feature_list_.InitWithFeatures(GetFeaturesToEnable(), {});
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_;
  std::unique_ptr<WebAuthBrowserTestContentBrowserClient> test_client_;
  WebAuthBrowserTestState test_state_;
  ContentBrowserClient* old_client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WebAuthBrowserTestBase);
};

// WebAuthLocalClientBrowserTest ----------------------------------------------

// Browser test fixture where the blink::mojom::Authenticator interface is
// accessed from a testing client in the browser process.
class WebAuthLocalClientBrowserTest : public WebAuthBrowserTestBase {
 public:
  WebAuthLocalClientBrowserTest() = default;
  ~WebAuthLocalClientBrowserTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    WebAuthBrowserTestBase::SetUpOnMainThread();
    ConnectToAuthenticator();
  }

  void ConnectToAuthenticator() {
    auto* interface_provider =
        static_cast<service_manager::mojom::InterfaceProvider*>(
            static_cast<RenderFrameHostImpl*>(
                shell()->web_contents()->GetMainFrame()));

    interface_provider->GetInterface(
        Authenticator::Name_,
        mojo::MakeRequest(&authenticator_ptr_).PassMessagePipe());
  }

  blink::mojom::PublicKeyCredentialCreationOptionsPtr
  BuildBasicCreateOptions() {
    auto rp = blink::mojom::PublicKeyCredentialRpEntity::New(
        "acme.com", "acme.com", base::nullopt);

    std::vector<uint8_t> kTestUserId{0, 0, 0};
    auto user = blink::mojom::PublicKeyCredentialUserEntity::New(
        kTestUserId, "name", base::nullopt, "displayName");

    static constexpr int32_t kCOSEAlgorithmIdentifierES256 = -7;
    auto param = blink::mojom::PublicKeyCredentialParameters::New();
    param->type = blink::mojom::PublicKeyCredentialType::PUBLIC_KEY;
    param->algorithm_identifier = kCOSEAlgorithmIdentifierES256;
    std::vector<blink::mojom::PublicKeyCredentialParametersPtr> parameters;
    parameters.push_back(std::move(param));

    std::vector<uint8_t> kTestChallenge{0, 0, 0};
    auto mojo_options = blink::mojom::PublicKeyCredentialCreationOptions::New(
        std::move(rp), std::move(user), kTestChallenge, std::move(parameters),
        base::TimeDelta::FromSeconds(30),
        std::vector<blink::mojom::PublicKeyCredentialDescriptorPtr>(), nullptr,
        blink::mojom::AttestationConveyancePreference::NONE, nullptr,
        false /* no hmac_secret */);

    return mojo_options;
  }

  blink::mojom::PublicKeyCredentialRequestOptionsPtr BuildBasicGetOptions() {
    std::vector<blink::mojom::PublicKeyCredentialDescriptorPtr> credentials;
    std::vector<blink::mojom::AuthenticatorTransport> transports;
    transports.push_back(blink::mojom::AuthenticatorTransport::USB);

    auto descriptor = blink::mojom::PublicKeyCredentialDescriptor::New(
        blink::mojom::PublicKeyCredentialType::PUBLIC_KEY,
        device::fido_parsing_utils::Materialize(
            device::test_data::kTestGetAssertionCredentialId),
        transports);
    credentials.push_back(std::move(descriptor));

    std::vector<uint8_t> kTestChallenge{0, 0, 0};
    auto mojo_options = blink::mojom::PublicKeyCredentialRequestOptions::New(
        kTestChallenge, base::TimeDelta::FromSeconds(30), "acme.com",
        std::move(credentials),
        blink::mojom::UserVerificationRequirement::PREFERRED, base::nullopt,
        std::vector<blink::mojom::CableAuthenticationPtr>());
    return mojo_options;
  }

  void WaitForConnectionError() {
    ASSERT_TRUE(authenticator_ptr_);
    ASSERT_TRUE(authenticator_ptr_.is_bound());
    if (authenticator_ptr_.encountered_error())
      return;

    base::RunLoop run_loop;
    authenticator_ptr_.set_connection_error_handler(run_loop.QuitClosure());
    run_loop.Run();
  }

  AuthenticatorPtr& authenticator() { return authenticator_ptr_; }

 private:
  AuthenticatorPtr authenticator_ptr_;

  DISALLOW_COPY_AND_ASSIGN(WebAuthLocalClientBrowserTest);
};

// Tests that no crash occurs when the implementation is destroyed with a
// pending navigator.credentials.create({publicKey: ...}) call.
IN_PROC_BROWSER_TEST_F(WebAuthLocalClientBrowserTest,
                       CreatePublicKeyCredentialThenNavigateAway) {
  device::test::ScopedFakeFidoDiscoveryFactory discovery_factory;
  auto* fake_hid_discovery = discovery_factory.ForgeNextHidDiscovery();
  TestCreateCallbackReceiver create_callback_receiver;
  authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                  create_callback_receiver.callback());

  fake_hid_discovery->WaitForCallToStartAndSimulateSuccess();
  NavigateToURL(shell(), GetHttpsURL("www.acme.com", "/title2.html"));
  WaitForConnectionError();

  // The next active document should be able to successfully call
  // navigator.credentials.create({publicKey: ...}) again.
  ConnectToAuthenticator();
  fake_hid_discovery = discovery_factory.ForgeNextHidDiscovery();
  authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                  create_callback_receiver.callback());
  fake_hid_discovery->WaitForCallToStartAndSimulateSuccess();
}

// Tests that no crash occurs when the implementation is destroyed with a
// pending navigator.credentials.get({publicKey: ...}) call.
IN_PROC_BROWSER_TEST_F(WebAuthLocalClientBrowserTest,
                       GetPublicKeyCredentialThenNavigateAway) {
  device::test::ScopedFakeFidoDiscoveryFactory discovery_factory;
  auto* fake_hid_discovery = discovery_factory.ForgeNextHidDiscovery();
  TestGetCallbackReceiver get_callback_receiver;
  authenticator()->GetAssertion(BuildBasicGetOptions(),
                                get_callback_receiver.callback());

  fake_hid_discovery->WaitForCallToStartAndSimulateSuccess();
  NavigateToURL(shell(), GetHttpsURL("www.acme.com", "/title2.html"));
  WaitForConnectionError();

  // The next active document should be able to successfully call
  // navigator.credentials.get({publicKey: ...}) again.
  ConnectToAuthenticator();
  fake_hid_discovery = discovery_factory.ForgeNextHidDiscovery();
  authenticator()->GetAssertion(BuildBasicGetOptions(),
                                get_callback_receiver.callback());
  fake_hid_discovery->WaitForCallToStartAndSimulateSuccess();
}

enum class AttestationCallbackBehavior {
  IGNORE_CALLBACK,
  BEFORE_NAVIGATION,
  AFTER_NAVIGATION,
};

const char* AttestationCallbackBehaviorToString(
    AttestationCallbackBehavior behavior) {
  switch (behavior) {
    case AttestationCallbackBehavior::IGNORE_CALLBACK:
      return "IGNORE_CALLBACK";
    case AttestationCallbackBehavior::BEFORE_NAVIGATION:
      return "BEFORE_NAVIGATION";
    case AttestationCallbackBehavior::AFTER_NAVIGATION:
      return "AFTER_NAVIGATION";
  }
}

const AttestationCallbackBehavior kAllAttestationCallbackBehaviors[] = {
    AttestationCallbackBehavior::IGNORE_CALLBACK,
    AttestationCallbackBehavior::BEFORE_NAVIGATION,
    AttestationCallbackBehavior::AFTER_NAVIGATION,
};

// Tests navigating while an attestation permission prompt is showing.
IN_PROC_BROWSER_TEST_F(WebAuthLocalClientBrowserTest,
                       PromptForAttestationThenNavigateAway) {
  for (auto behavior : kAllAttestationCallbackBehaviors) {
    SCOPED_TRACE(AttestationCallbackBehaviorToString(behavior));

    device::test::ScopedVirtualFidoDevice virtual_device;
    TestCreateCallbackReceiver create_callback_receiver;
    auto options = BuildBasicCreateOptions();
    options->attestation =
        blink::mojom::AttestationConveyancePreference::DIRECT;
    authenticator()->MakeCredential(std::move(options),
                                    create_callback_receiver.callback());
    bool attestation_callback_was_invoked = false;
    test_state()->attestation_prompt_callback_ = base::BindLambdaForTesting(
        [&](base::OnceCallback<void(bool)> callback) {
          attestation_callback_was_invoked = true;

          if (behavior == AttestationCallbackBehavior::BEFORE_NAVIGATION) {
            std::move(callback).Run(false);
          }
          NavigateToURL(shell(), GetHttpsURL("www.acme.com", "/title2.html"));
          if (behavior == AttestationCallbackBehavior::AFTER_NAVIGATION) {
            std::move(callback).Run(false);
          }
        });

    WaitForConnectionError();
    ASSERT_TRUE(attestation_callback_was_invoked);
    ConnectToAuthenticator();
  }
}

// Tests that the blink::mojom::Authenticator connection is not closed on a
// cancelled navigation.
IN_PROC_BROWSER_TEST_F(WebAuthLocalClientBrowserTest,
                       CreatePublicKeyCredentialAfterCancelledNavigation) {
  ScopedNavigationCancellingThrottleInstaller navigation_canceller(
      shell()->web_contents());

  NavigateToURL(shell(), GetHttpsURL("www.acme.com", "/title2.html"));

  device::test::ScopedFakeFidoDiscoveryFactory discovery_factory;
  auto* fake_hid_discovery = discovery_factory.ForgeNextHidDiscovery();
  TestCreateCallbackReceiver create_callback_receiver;
  authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                  create_callback_receiver.callback());

  fake_hid_discovery->WaitForCallToStartAndSimulateSuccess();
}

// Tests that a navigator.credentials.create({publicKey: ...}) issued at the
// moment just before a navigation commits is not serviced.
IN_PROC_BROWSER_TEST_F(WebAuthLocalClientBrowserTest,
                       CreatePublicKeyCredentialRacingWithNavigation) {
  TestCreateCallbackReceiver create_callback_receiver;
  auto request_options = BuildBasicCreateOptions();

  ClosureExecutorBeforeNavigationCommit executor(
      shell()->web_contents(), base::BindLambdaForTesting([&]() {
        authenticator()->MakeCredential(std::move(request_options),
                                        create_callback_receiver.callback());
      }));

  device::test::ScopedFakeFidoDiscoveryFactory discovery_factory;
  auto* fake_hid_discovery = discovery_factory.ForgeNextHidDiscovery();
  NavigateToURL(shell(), GetHttpsURL("www.acme.com", "/title2.html"));
  WaitForConnectionError();

  // Normally, when the request is serviced, the implementation retrieves the
  // factory as one of the first steps. Here, the request should not have been
  // serviced at all, so the fake request should still be pending on the fake
  // factory.
  auto hid_discovery = ::device::FidoDiscoveryFactory::Create(
      ::device::FidoTransportProtocol::kUsbHumanInterfaceDevice, nullptr);
  ASSERT_TRUE(!!hid_discovery);

  // The next active document should be able to successfully call
  // navigator.credentials.create({publicKey: ...}) again.
  ConnectToAuthenticator();
  fake_hid_discovery = discovery_factory.ForgeNextHidDiscovery();
  authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                  create_callback_receiver.callback());
  fake_hid_discovery->WaitForCallToStartAndSimulateSuccess();
}

// Regression test for https://crbug.com/818219.
IN_PROC_BROWSER_TEST_F(WebAuthLocalClientBrowserTest,
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
IN_PROC_BROWSER_TEST_F(WebAuthLocalClientBrowserTest,
                       GetPublicKeyCredentialTwiceInARow) {
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

IN_PROC_BROWSER_TEST_F(WebAuthLocalClientBrowserTest,
                       CreatePublicKeyCredentialWhileRequestIsPending) {
  device::test::ScopedFakeFidoDiscoveryFactory discovery_factory;
  auto* fake_hid_discovery = discovery_factory.ForgeNextHidDiscovery();
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

IN_PROC_BROWSER_TEST_F(WebAuthLocalClientBrowserTest,
                       GetPublicKeyCredentialWhileRequestIsPending) {
  device::test::ScopedFakeFidoDiscoveryFactory discovery_factory;
  auto* fake_hid_discovery = discovery_factory.ForgeNextHidDiscovery();
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

// WebAuthJavascriptClientBrowserTest -----------------------------------------

// Browser test fixture where the blink::mojom::Authenticator interface is
// normally accessed from Javascript in the renderer process.
class WebAuthJavascriptClientBrowserTest : public WebAuthBrowserTestBase {
 public:
  WebAuthJavascriptClientBrowserTest() = default;
  ~WebAuthJavascriptClientBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(WebAuthJavascriptClientBrowserTest);
};

constexpr device::ProtocolVersion kAllProtocols[] = {
    device::ProtocolVersion::kCtap, device::ProtocolVersion::kU2f};

// Tests that when navigator.credentials.create() is called with an invalid
// relying party id, we get a SecurityError.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       CreatePublicKeyCredentialInvalidRp) {
  CreateParameters parameters;
  parameters.rp_id = "localhost";
  std::string result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      shell()->web_contents()->GetMainFrame(),
      BuildCreateCallWithParameters(parameters), &result));

  ASSERT_EQ(kRelyingPartySecurityErrorMessage,
            result.substr(0, strlen(kRelyingPartySecurityErrorMessage)));
}

// Tests that when navigator.credentials.create() is called with a null
// relying party, we get a NotSupportedError.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       CreatePublicKeyWithNullRp) {
  CreateParameters parameters;
  parameters.rp_icon = "";
  std::string script = BuildCreateCallWithParameters(parameters);
  const char kExpectedSubstr[] = "{ id: 'acme.com', name: 'Acme', icon: ''}";
  const std::string::size_type offset = script.find(kExpectedSubstr);
  ASSERT_TRUE(offset != std::string::npos);
  script.replace(offset, sizeof(kExpectedSubstr) - 1, "null");

  std::string result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      shell()->web_contents()->GetMainFrame(), script, &result));
  ASSERT_EQ(kPublicKeyErrorMessage, result);
}

// Tests that when navigator.credentials.create() is called with an insecure
// user icon URL, we get a SecurityError.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       CreatePublicKeyWithInsecureUserIconURL) {
  CreateParameters parameters;
  parameters.user_icon = "http://fidoalliance.co.nz/testimages/catimage.png";
  std::string result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      shell()->web_contents()->GetMainFrame(),
      BuildCreateCallWithParameters(parameters), &result));
  ASSERT_EQ(kRelyingPartyUserIconUrlSecurityErrorMessage, result);
}

// Tests that when navigator.credentials.create() is called with an insecure
// Relying Party icon URL, we get a SecurityError.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       CreatePublicKeyWithInsecureRpIconURL) {
  CreateParameters parameters;
  parameters.rp_icon = "http://fidoalliance.co.nz/testimages/catimage.png";
  std::string result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      shell()->web_contents()->GetMainFrame(),
      BuildCreateCallWithParameters(parameters), &result));
  ASSERT_EQ(kRelyingPartyRpIconUrlSecurityErrorMessage, result);
}

// Tests that when navigator.credentials.create() is called with user
// verification required, request times out.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       CreatePublicKeyCredentialWithUserVerification) {
  for (const auto protocol : kAllProtocols) {
    device::test::ScopedVirtualFidoDevice virtual_device;
    virtual_device.SetSupportedProtocol(protocol);

    CreateParameters parameters;
    parameters.user_verification = kRequiredVerification;
    std::string result;
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        shell()->web_contents()->GetMainFrame(),
        BuildCreateCallWithParameters(parameters), &result));
    ASSERT_EQ(kTimeoutErrorMessage, result);
  }
}

// Tests that when navigator.credentials.create() is called with resident key
// required, request times out.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       CreatePublicKeyCredentialWithResidentKeyRequired) {
  for (const auto protocol : kAllProtocols) {
    device::test::ScopedVirtualFidoDevice virtual_device;
    virtual_device.SetSupportedProtocol(protocol);

    CreateParameters parameters;
    parameters.require_resident_key = true;
    std::string result;
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        shell()->web_contents()->GetMainFrame(),
        BuildCreateCallWithParameters(parameters), &result));

    ASSERT_EQ(kResidentCredentialsErrorMessage, result);
  }
}

// Tests that when navigator.credentials.create() is called with an
// unsupported algorithm, request times out.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       CreatePublicKeyCredentialAlgorithmNotSupported) {
  for (const auto protocol : kAllProtocols) {
    device::test::ScopedVirtualFidoDevice virtual_device;
    virtual_device.SetSupportedProtocol(protocol);

    CreateParameters parameters;
    parameters.algorithm_identifier = "123";
    std::string result;
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        shell()->web_contents()->GetMainFrame(),
        BuildCreateCallWithParameters(parameters), &result));

    ASSERT_EQ(kTimeoutErrorMessage, result);
  }
}

// Tests that when navigator.credentials.create() is called with a
// platform authenticator requested, request times out.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       CreatePublicKeyCredentialPlatformAuthenticator) {
  for (const auto protocol : kAllProtocols) {
    device::test::ScopedVirtualFidoDevice virtual_device;
    virtual_device.SetSupportedProtocol(protocol);

    CreateParameters parameters;
    parameters.authenticator_attachment = kPlatform;
    std::string result;
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        shell()->web_contents()->GetMainFrame(),
        BuildCreateCallWithParameters(parameters), &result));

    ASSERT_EQ(kTimeoutErrorMessage, result);
  }
}

// Tests that when navigator.credentials.get() is called with user verification
// required, we get a NotSupportedError.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       GetPublicKeyCredentialUserVerification) {
  for (const auto protocol : kAllProtocols) {
    device::test::ScopedVirtualFidoDevice virtual_device;
    virtual_device.SetSupportedProtocol(protocol);

    GetParameters parameters;
    parameters.user_verification = "required";
    std::string result;
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        shell()->web_contents()->GetMainFrame(),
        BuildGetCallWithParameters(parameters), &result));
    ASSERT_EQ(kTimeoutErrorMessage, result);
  }
}

// Tests that when navigator.credentials.get() is called with an empty
// allowCredentials list, we get a NotSupportedError.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       GetPublicKeyCredentialEmptyAllowCredentialsList) {
  device::test::ScopedVirtualFidoDevice virtual_device;
  GetParameters parameters;
  parameters.allow_credentials = "";
  std::string result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      shell()->web_contents()->GetMainFrame(),
      BuildGetCallWithParameters(parameters), &result));
  ASSERT_EQ(kResidentCredentialsErrorMessage, result);
}

// WebAuthBrowserBleDisabledTest
// ----------------------------------------------

// A test fixture that does not enable BLE discovery.
class WebAuthBrowserBleDisabledTest : public WebAuthLocalClientBrowserTest {
 public:
  WebAuthBrowserBleDisabledTest() = default;

 protected:
  std::vector<base::Feature> GetFeaturesToEnable() override {
    return {features::kWebAuth};
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(WebAuthBrowserBleDisabledTest);
};

// Tests that the BLE discovery does not start when the WebAuthnBle feature
// flag is disabled.
IN_PROC_BROWSER_TEST_F(WebAuthBrowserBleDisabledTest, CheckBleDisabled) {
  device::test::ScopedFakeFidoDiscoveryFactory discovery_factory;
  auto* fake_hid_discovery = discovery_factory.ForgeNextHidDiscovery();
  auto* fake_ble_discovery = discovery_factory.ForgeNextBleDiscovery();

  // Do something that will start discoveries.
  TestCreateCallbackReceiver create_callback_receiver;
  authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                  create_callback_receiver.callback());

  fake_hid_discovery->WaitForCallToStart();
  EXPECT_TRUE(fake_hid_discovery->is_start_requested());
  EXPECT_FALSE(fake_ble_discovery->is_start_requested());
}

// Executes Javascript in the given WebContents and waits until a string with
// the given prefix is received. It will ignore values other than strings, and
// strings without the given prefix. Since messages are broadcast to
// DOMMessageQueues, this allows other functions that depend on ExecuteScript
// (and thus trigger the broadcast of values) to run while this function is
// waiting for a specific result.
base::Optional<std::string> ExecuteScriptAndExtractPrefixedString(
    WebContents* web_contents,
    const std::string& script,
    const std::string& result_prefix) {
  DOMMessageQueue dom_message_queue(web_contents);
  web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16(script));

  for (;;) {
    std::string json;
    if (!dom_message_queue.WaitForMessage(&json)) {
      return base::nullopt;
    }

    base::JSONReader reader(base::JSON_ALLOW_TRAILING_COMMAS);
    std::unique_ptr<base::Value> result = reader.ReadToValueDeprecated(json);
    if (!result) {
      return base::nullopt;
    }

    std::string str;
    if (result->GetAsString(&str) && str.find(result_prefix) == 0) {
      return str;
    }
  }
}

// Tests that a credentials.create() call triggered by the main frame will
// successfully complete even if a subframe navigation takes place while the
// request is waiting for user consent.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       NavigateSubframeDuringPress) {
  device::test::ScopedVirtualFidoDevice virtual_device;
  bool prompt_callback_was_invoked = false;
  virtual_device.mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting([&]() {
        prompt_callback_was_invoked = true;
        NavigateIframeToURL(shell()->web_contents(), "test_iframe",
                            GURL("/title2.html"));
      });

  NavigateToURL(shell(), GetHttpsURL("www.acme.com", "/page_with_iframe.html"));

  // The plain ExecuteScriptAndExtractString cannot be used because
  // NavigateIframeToURL uses it internally and they get confused about which
  // message is for whom.
  base::Optional<std::string> result = ExecuteScriptAndExtractPrefixedString(
      shell()->web_contents(),
      BuildCreateCallWithParameters(CreateParameters()), "webauth: ");
  ASSERT_TRUE(result);
  ASSERT_EQ("webauth: OK", *result);
  ASSERT_TRUE(prompt_callback_was_invoked);
}

IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       NavigateSubframeDuringAttestationPrompt) {
  device::test::ScopedVirtualFidoDevice virtual_device;

  for (auto behavior : kAllAttestationCallbackBehaviors) {
    if (behavior == AttestationCallbackBehavior::IGNORE_CALLBACK) {
      // If the callback is ignored, then the registration will not complete and
      // that hangs the test.
      continue;
    }

    SCOPED_TRACE(AttestationCallbackBehaviorToString(behavior));

    bool prompt_callback_was_invoked = false;
    test_state()->attestation_prompt_callback_ = base::BindOnce(
        [](WebContents* web_contents, bool* prompt_callback_was_invoked,
           AttestationCallbackBehavior behavior,
           base::OnceCallback<void(bool)> callback) {
          *prompt_callback_was_invoked = true;

          if (behavior == AttestationCallbackBehavior::BEFORE_NAVIGATION) {
            std::move(callback).Run(true);
          }
          // Can't use NavigateIframeToURL here because in the
          // BEFORE_NAVIGATION case we are racing AuthenticatorImpl and
          // NavigateIframeToURL can get confused by the "OK" message.
          base::Optional<std::string> result =
              ExecuteScriptAndExtractPrefixedString(
                  web_contents,
                  "document.getElementById('test_iframe').src = "
                  "'/title2.html'; "
                  "window.domAutomationController.send('iframe: done');",
                  "iframe: ");
          CHECK(result);
          CHECK_EQ("iframe: done", *result);
          if (behavior == AttestationCallbackBehavior::AFTER_NAVIGATION) {
            std::move(callback).Run(true);
          }
        },
        shell()->web_contents(), &prompt_callback_was_invoked, behavior);

    NavigateToURL(shell(),
                  GetHttpsURL("www.acme.com", "/page_with_iframe.html"));

    CreateParameters parameters;
    parameters.attestation = "direct";
    // The plain ExecuteScriptAndExtractString cannot be used because
    // NavigateIframeToURL uses it internally and they get confused about which
    // message is for whom.
    base::Optional<std::string> result = ExecuteScriptAndExtractPrefixedString(
        shell()->web_contents(), BuildCreateCallWithParameters(parameters),
        "webauth: ");
    ASSERT_TRUE(result);
    ASSERT_EQ("webauth: OK", *result);
    ASSERT_TRUE(prompt_callback_was_invoked);
  }
}

// WebAuthBrowserCtapTest ----------------------------------------------

class WebAuthBrowserCtapTest : public WebAuthLocalClientBrowserTest {
 public:
  WebAuthBrowserCtapTest() = default;
  ~WebAuthBrowserCtapTest() override = default;

  DISALLOW_COPY_AND_ASSIGN(WebAuthBrowserCtapTest);
};

IN_PROC_BROWSER_TEST_F(WebAuthBrowserCtapTest, TestMakeCredential) {
  for (const auto protocol : kAllProtocols) {
    device::test::ScopedVirtualFidoDevice virtual_device;
    virtual_device.SetSupportedProtocol(protocol);

    TestCreateCallbackReceiver create_callback_receiver;
    authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                    create_callback_receiver.callback());

    create_callback_receiver.WaitForCallback();
    EXPECT_EQ(AuthenticatorStatus::SUCCESS, create_callback_receiver.status());
  }
}

IN_PROC_BROWSER_TEST_F(WebAuthBrowserCtapTest,
                       TestMakeCredentialWithDuplicateKeyHandle) {
  for (const auto protocol : kAllProtocols) {
    device::test::ScopedVirtualFidoDevice virtual_device;
    virtual_device.SetSupportedProtocol(protocol);
    auto make_credential_request = BuildBasicCreateOptions();
    auto excluded_credential = blink::mojom::PublicKeyCredentialDescriptor::New(
        blink::mojom::PublicKeyCredentialType::PUBLIC_KEY,
        device::fido_parsing_utils::Materialize(
            device::test_data::kCtap2MakeCredentialCredentialId),
        std::vector<blink::mojom::AuthenticatorTransport>{
            blink::mojom::AuthenticatorTransport::USB});
    make_credential_request->exclude_credentials.push_back(
        std::move(excluded_credential));

    ASSERT_TRUE(virtual_device.mutable_state()->InjectRegistration(
        device::fido_parsing_utils::Materialize(
            device::test_data::kCtap2MakeCredentialCredentialId),
        make_credential_request->relying_party->id));

    TestCreateCallbackReceiver create_callback_receiver;
    authenticator()->MakeCredential(std::move(make_credential_request),
                                    create_callback_receiver.callback());

    create_callback_receiver.WaitForCallback();
    EXPECT_EQ(AuthenticatorStatus::CREDENTIAL_EXCLUDED,
              create_callback_receiver.status());
  }
}

IN_PROC_BROWSER_TEST_F(WebAuthBrowserCtapTest, TestGetAssertion) {
  for (const auto protocol : kAllProtocols) {
    device::test::ScopedVirtualFidoDevice virtual_device;
    virtual_device.SetSupportedProtocol(protocol);
    auto get_assertion_request_params = BuildBasicGetOptions();
    ASSERT_TRUE(virtual_device.mutable_state()->InjectRegistration(
        device::fido_parsing_utils::Materialize(
            device::test_data::kTestGetAssertionCredentialId),
        get_assertion_request_params->relying_party_id));

    TestGetCallbackReceiver get_callback_receiver;
    authenticator()->GetAssertion(std::move(get_assertion_request_params),
                                  get_callback_receiver.callback());
    get_callback_receiver.WaitForCallback();
    EXPECT_EQ(AuthenticatorStatus::SUCCESS, get_callback_receiver.status());
  }
}

IN_PROC_BROWSER_TEST_F(WebAuthBrowserCtapTest,
                       TestGetAssertionWithNoMatchingKeyHandles) {
  for (const auto protocol : kAllProtocols) {
    device::test::ScopedVirtualFidoDevice virtual_device;
    virtual_device.SetSupportedProtocol(protocol);
    auto get_assertion_request_params = BuildBasicGetOptions();

    TestGetCallbackReceiver get_callback_receiver;
    authenticator()->GetAssertion(std::move(get_assertion_request_params),
                                  get_callback_receiver.callback());
    get_callback_receiver.WaitForCallback();
    EXPECT_EQ(AuthenticatorStatus::CREDENTIAL_NOT_RECOGNIZED,
              get_callback_receiver.status());
  }
}

}  // namespace

}  // namespace content
