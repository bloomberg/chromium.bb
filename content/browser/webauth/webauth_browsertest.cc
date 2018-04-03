// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/webauth/authenticator_impl.h"
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
#include "content/shell/browser/shell.h"
#include "content/test/did_commit_provisional_load_interceptor.h"
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

using webauth::mojom::Authenticator;
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

constexpr char kNotAllowedErrorMessage[] =
    "NotAllowedError: The operation either timed out or was not allowed. See: "
    "https://w3c.github.io/webauthn/#sec-assertion-privacy.";

constexpr char kRelyingPartySecurityErrorMessage[] =
    "SecurityError: The relying party ID 'localhost' is not a registrable "
    "domain suffix of, nor equal to 'https://www.example.com";

constexpr char kNotSupportedErrorMessage[] =
    "NotSupportedError: Parameters for this operation are not supported.";

// Templates to be used with base::ReplaceStringPlaceholders. Can be
// modified to include up to 9 replacements. The default values for
// any additional replacements added should also be added to the
// CreateParameters struct.
constexpr char kCreatePublicKeyTemplate[] =
    "navigator.credentials.create({ publicKey: {"
    "  challenge: new TextEncoder().encode('climb a mountain'),"
    "  rp: { id: '$3', name: 'Acme' },"
    "  user: { "
    "    id: new TextEncoder().encode('1098237235409872'),"
    "    name: 'avery.a.jones@example.com',"
    "    displayName: 'Avery A. Jones', "
    "    icon: 'https://pics.acme.com/00/p/aBjjjpqPb.png'},"
    "  pubKeyCredParams: [{ type: 'public-key', alg: '$4'}],"
    "  timeout: 60000,"
    "  excludeCredentials: [],"
    "  authenticatorSelection : {"
    "     requireResidentKey: $1,"
    "     userVerification: '$2',"
    "     authenticatorAttachment: '$5' }}"
    "}).catch(c => window.domAutomationController.send(c.toString()));";

constexpr char kPlatform[] = "platform";
constexpr char kCrossPlatform[] = "cross-platform";
constexpr char kPreferredVerification[] = "preferred";
constexpr char kRequiredVerification[] = "required";

// Default values for kCreatePublicKeyTemplate.
struct CreateParameters {
  const char* rp_id = "example.com";
  bool require_resident_key = false;
  const char* user_verification = kPreferredVerification;
  const char* authenticator_attachment = kCrossPlatform;
  const char* algorithm_identifier = "-7";
};

std::string BuildCreateCallWithParameters(const CreateParameters& parameters) {
  std::vector<std::string> substititions;
  substititions.push_back(parameters.require_resident_key ? "true" : "false");
  substititions.push_back(parameters.user_verification);
  substititions.push_back(parameters.rp_id);
  substititions.push_back(parameters.algorithm_identifier);
  substititions.push_back(parameters.authenticator_attachment);
  return base::ReplaceStringPlaceholders(kCreatePublicKeyTemplate,
                                         substititions, nullptr);
}

constexpr char kGetPublicKeyTemplate[] =
    "navigator.credentials.get({ publicKey: {"
    "  challenge: new TextEncoder().encode('climb a mountain'),"
    "  rp: 'example.com',"
    "  timeout: 60000,"
    "  userVerification: '$1',"
    "  allowCredentials: [{ type: 'public-key',"
    "     id: new TextEncoder().encode('allowedCredential'),"
    "     transports: ['usb', 'nfc', 'ble']}] }"
    "}).catch(c => window.domAutomationController.send(c.toString()));";

// Helper class that executes the given |closure| the very last moment before
// the next navigation commits in a given WebContents.
class ClosureExecutorBeforeNavigationCommit
    : public DidCommitProvisionalLoadInterceptor {
 public:
  ClosureExecutorBeforeNavigationCommit(WebContents* web_contents,
                                        base::OnceClosure closure)
      : DidCommitProvisionalLoadInterceptor(web_contents),
        closure_(std::move(closure)) {}
  ~ClosureExecutorBeforeNavigationCommit() override = default;

 protected:
  void WillDispatchDidCommitProvisionalLoad(
      RenderFrameHost* render_frame_host,
      ::FrameHostMsg_DidCommitProvisionalLoad_Params* params,
      service_manager::mojom::InterfaceProviderRequest*
          interface_provider_request) override {
    if (closure_)
      std::move(closure_).Run();
  }

 private:
  base::OnceClosure closure_;
  DISALLOW_COPY_AND_ASSIGN(ClosureExecutorBeforeNavigationCommit);
};

// Cancels all navigations in a WebContents while in scope.
class ScopedNavigationCancellingThrottleInstaller : public WebContentsObserver {
 public:
  ScopedNavigationCancellingThrottleInstaller(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  ~ScopedNavigationCancellingThrottleInstaller() override = default;

 protected:
  class CancellingThrottle : public NavigationThrottle {
   public:
    CancellingThrottle(NavigationHandle* handle) : NavigationThrottle(handle) {}
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

// Test fixture base class for common tasks.
class WebAuthBrowserTestBase : public content::ContentBrowserTest {
 protected:
  WebAuthBrowserTestBase()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  virtual std::vector<base::Feature> GetFeaturesToEnable() {
    return {features::kWebAuth, features::kWebAuthBle};
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server().ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(https_server().Start());

    NavigateToURL(shell(), GetHttpsURL("www.example.com", "/title1.html"));
  }

  GURL GetHttpsURL(const std::string& hostname,
                   const std::string& relative_url) {
    return https_server_.GetURL(hostname, relative_url);
  }

  net::EmbeddedTestServer& https_server() { return https_server_; }
  device::test::ScopedFakeFidoDiscoveryFactory* discovery_factory() {
    return &factory_;
  }

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_feature_list_.InitWithFeatures(GetFeaturesToEnable(), {});
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_;
  device::test::ScopedFakeFidoDiscoveryFactory factory_;

  DISALLOW_COPY_AND_ASSIGN(WebAuthBrowserTestBase);
};

}  // namespace

// WebAuthLocalClientBrowserTest ----------------------------------------------

// Browser test fixture where the webauth::mojom::Authenticator interface is
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
  auto* fake_hid_discovery = discovery_factory()->ForgeNextHidDiscovery();
  TestCreateCallbackReceiver create_callback_receiver;
  authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                  create_callback_receiver.callback());

  fake_hid_discovery->WaitForCallToStartAndSimulateSuccess();
  NavigateToURL(shell(), GetHttpsURL("www.example.com", "/title2.html"));
  WaitForConnectionError();

  // The next active document should be able to successfully call
  // navigator.credentials.create({publicKey: ...}) again.
  ConnectToAuthenticator();
  fake_hid_discovery = discovery_factory()->ForgeNextHidDiscovery();
  authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                  create_callback_receiver.callback());
  fake_hid_discovery->WaitForCallToStartAndSimulateSuccess();
}

// Tests that no crash occurs when the implementation is destroyed with a
// pending navigator.credentials.get({publicKey: ...}) call.
IN_PROC_BROWSER_TEST_F(WebAuthLocalClientBrowserTest,
                       GetPublicKeyCredentialThenNavigateAway) {
  auto* fake_hid_discovery = discovery_factory()->ForgeNextHidDiscovery();
  TestGetCallbackReceiver get_callback_receiver;
  authenticator()->GetAssertion(BuildBasicGetOptions(),
                                get_callback_receiver.callback());

  fake_hid_discovery->WaitForCallToStartAndSimulateSuccess();
  NavigateToURL(shell(), GetHttpsURL("www.example.com", "/title2.html"));
  WaitForConnectionError();

  // The next active document should be able to successfully call
  // navigator.credentials.get({publicKey: ...}) again.
  ConnectToAuthenticator();
  fake_hid_discovery = discovery_factory()->ForgeNextHidDiscovery();
  authenticator()->GetAssertion(BuildBasicGetOptions(),
                                get_callback_receiver.callback());
  fake_hid_discovery->WaitForCallToStartAndSimulateSuccess();
}

// Tests that the webauth::mojom::Authenticator connection is not closed on a
// cancelled navigation.
IN_PROC_BROWSER_TEST_F(WebAuthLocalClientBrowserTest,
                       CreatePublicKeyCredentialAfterCancelledNavigation) {
  ScopedNavigationCancellingThrottleInstaller navigation_canceller(
      shell()->web_contents());

  NavigateToURL(shell(), GetHttpsURL("www.example.com", "/title2.html"));

  auto* fake_hid_discovery = discovery_factory()->ForgeNextHidDiscovery();
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

  auto* fake_hid_discovery = discovery_factory()->ForgeNextHidDiscovery();
  NavigateToURL(shell(), GetHttpsURL("www.example.com", "/title2.html"));
  WaitForConnectionError();

  // Normally, when the request is serviced, the implementation retrieves the
  // factory as one of the first steps. Here, the request should not have been
  // serviced at all, so the fake request should still be pending on the fake
  // factory.
  auto hid_discovery = ::device::FidoDiscovery::Create(
      ::device::U2fTransportProtocol::kUsbHumanInterfaceDevice, nullptr);
  ASSERT_TRUE(!!hid_discovery);

  // The next active document should be able to successfully call
  // navigator.credentials.create({publicKey: ...}) again.
  ConnectToAuthenticator();
  fake_hid_discovery = discovery_factory()->ForgeNextHidDiscovery();
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

IN_PROC_BROWSER_TEST_F(WebAuthLocalClientBrowserTest,
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

// WebAuthJavascriptClientBrowserTest -----------------------------------------

// Browser test fixture where the webauth::mojom::Authenticator interface is
// normally accessed from Javascript in the renderer process.
class WebAuthJavascriptClientBrowserTest : public WebAuthBrowserTestBase {
 public:
  WebAuthJavascriptClientBrowserTest() = default;
  ~WebAuthJavascriptClientBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(WebAuthJavascriptClientBrowserTest);
};

// Tests that when navigator.credentials.create() is called with user
// verification required we get a NotAllowedError.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       CreatePublicKeyCredentialWithUserVerification) {
  CreateParameters parameters;
  parameters.user_verification = kRequiredVerification;
  std::string result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      shell()->web_contents()->GetMainFrame(),
      BuildCreateCallWithParameters(parameters), &result));
  ASSERT_EQ(kNotAllowedErrorMessage, result);
}

// Tests that when navigator.credentials.create() is called with resident key
// required, we get a NotAllowedError.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       CreatePublicKeyCredentialWithResidentKeyRequired) {
  CreateParameters parameters;
  parameters.require_resident_key = true;
  std::string result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      shell()->web_contents()->GetMainFrame(),
      BuildCreateCallWithParameters(parameters), &result));

  ASSERT_EQ(kNotAllowedErrorMessage, result);
}

// Tests that when navigator.credentials.get() is called with user verification
// required, we get a NotAllowedError.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       GetPublicKeyCredentialUserVerification) {
  const std::string kScript = base::ReplaceStringPlaceholders(
      kGetPublicKeyTemplate, {"required"}, nullptr);

  std::string result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      shell()->web_contents()->GetMainFrame(), kScript, &result));
  ASSERT_EQ(kNotAllowedErrorMessage, result);
}

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

// Tests that when navigator.credentials.create() is called with an
// unsupported algorithm, we get a NotSupportedError.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       CreatePublicKeyCredentialAlgorithmNotSupported) {
  CreateParameters parameters;
  parameters.algorithm_identifier = "123";
  std::string result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      shell()->web_contents()->GetMainFrame(),
      BuildCreateCallWithParameters(parameters), &result));

  ASSERT_EQ(kNotSupportedErrorMessage, result);
}

// Tests that when navigator.credentials.create() is called with a
// platform authenticator requested, we get a NotAllowedError.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       CreatePublicKeyCredentialPlatformAuthenticator) {
  CreateParameters parameters;
  parameters.authenticator_attachment = kPlatform;
  std::string result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      shell()->web_contents()->GetMainFrame(),
      BuildCreateCallWithParameters(parameters), &result));

  ASSERT_EQ(kNotAllowedErrorMessage, result);
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
