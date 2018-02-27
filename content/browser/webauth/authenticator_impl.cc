// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/authenticator_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/base64url.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/timer/timer.h"
#include "content/browser/bad_message.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/service_manager_connection.h"
#include "crypto/sha2.h"
#include "device/fido/u2f_register.h"
#include "device/fido/u2f_request.h"
#include "device/fido/u2f_return_code.h"
#include "device/fido/u2f_sign.h"
#include "device/fido/u2f_transport_protocol.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/service_manager/public/cpp/connector.h"
#include "url/url_util.h"

namespace content {

namespace client_data {
const char kCreateType[] = "webauthn.create";
const char kGetType[] = "webauthn.get";
}  // namespace client_data

namespace {
constexpr int32_t kCoseEs256 = -7;

// Ensure that the origin's effective domain is a valid domain.
// Only the domain format of host is valid.
// Reference https://url.spec.whatwg.org/#valid-domain-string and
// https://html.spec.whatwg.org/multipage/origin.html#concept-origin-effective-domain.
bool HasValidEffectiveDomain(url::Origin caller_origin) {
  return !caller_origin.unique() &&
         !url::HostIsIPAddress(caller_origin.host()) &&
         content::IsOriginSecure(caller_origin.GetURL());
}

// Ensure the relying party ID is a registrable domain suffix of or equal
// to the origin's effective domain. Reference:
// https://html.spec.whatwg.org/multipage/origin.html#is-a-registrable-domain-suffix-of-or-is-equal-to.
bool IsRelyingPartyIdValid(const std::string& relying_party_id,
                           url::Origin caller_origin) {
  if (relying_party_id.empty())
    return false;

  if (caller_origin.host() == relying_party_id)
    return true;

  if (!caller_origin.DomainIs(relying_party_id))
    return false;
  if (!net::registry_controlled_domains::HostHasRegistryControlledDomain(
          caller_origin.host(),
          net::registry_controlled_domains::INCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES))
    return false;
  if (!net::registry_controlled_domains::HostHasRegistryControlledDomain(
          relying_party_id,
          net::registry_controlled_domains::INCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES))
    // TODO(crbug.com/803414): Accept corner-case situations like the following
    // origin: "https://login.awesomecompany",
    // relying_party_id: "awesomecompany".
    return false;
  return true;
}

bool IsAppIdAllowedForOrigin(const GURL& appid, const url::Origin& origin) {
  // See
  // https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-appid-and-facets-v1.2-ps-20170411.html#determining-if-a-caller-s-facetid-is-authorized-for-an-appid

  // Step 1: "If the AppID is not an HTTPS URL, and matches the FacetID of the
  // caller, no additional processing is necessary and the operation may
  // proceed."

  // Webauthn is only supported on secure origins and |HasValidEffectiveDomain|
  // has already checked this property of |origin| before this call. Thus this
  // step is moot.
  DCHECK(content::IsOriginSecure(origin.GetURL()));

  // Step 2: "If the AppID is null or empty, the client must set the AppID to be
  // the FacetID of the caller, and the operation may proceed without additional
  // processing."

  // This step is handled before calling this function.

  // Step 3: "If the caller's FacetID is an https:// Origin sharing the same
  // host as the AppID, (e.g. if an application hosted at
  // https://fido.example.com/myApp set an AppID of
  // https://fido.example.com/myAppId), no additional processing is necessary
  // and the operation may proceed."
  if (origin.scheme() != url::kHttpsScheme ||
      appid.scheme_piece() != origin.scheme()) {
    return false;
  }

  // This check is repeated inside |SameDomainOrHost|, just after this. However
  // it's cheap and mirrors the structure of the spec.
  if (appid.host_piece() == origin.host()) {
    return true;
  }

  // At this point we diverge from the specification in order to avoid the
  // complexity of making a network request which isn't believed to be
  // neccessary in practice. See also
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1244959#c8
  if (net::registry_controlled_domains::SameDomainOrHost(
          appid, origin,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    return true;
  }

  // As a compatibility hack, sites within google.com are allowed to assert two
  // special-case AppIDs. Firefox also does this:
  // https://groups.google.com/forum/#!msg/mozilla.dev.platform/Uiu3fwnA2xw/201ynAiPAQAJ
  const GURL kGstatic1 =
      GURL("https://www.gstatic.com/securitykey/origins.json");
  const GURL kGstatic2 =
      GURL("https://www.gstatic.com/a/google.com/securitykey/origins.json");
  DCHECK(kGstatic1.is_valid() && kGstatic2.is_valid());

  if (origin.DomainIs("google.com") && !appid.has_ref() &&
      (appid.EqualsIgnoringRef(kGstatic1) ||
       appid.EqualsIgnoringRef(kGstatic2))) {
    return true;
  }

  return false;
}

bool HasValidAlgorithm(
    const std::vector<webauth::mojom::PublicKeyCredentialParametersPtr>&
        parameters) {
  for (const auto& params : parameters) {
    if (params->algorithm_identifier == kCoseEs256)
      return true;
  }
  return false;
}

std::vector<std::vector<uint8_t>> FilterCredentialList(
    const std::vector<webauth::mojom::PublicKeyCredentialDescriptorPtr>&
        descriptors) {
  std::vector<std::vector<uint8_t>> handles;
  for (const auto& credential_descriptor : descriptors) {
    if (credential_descriptor->type ==
        webauth::mojom::PublicKeyCredentialType::PUBLIC_KEY) {
      handles.push_back(credential_descriptor->id);
    }
  }
  return handles;
}

std::vector<uint8_t> ConstructClientDataHash(const std::string& client_data) {
  // SHA-256 hash of the JSON data structure.
  std::vector<uint8_t> client_data_hash(crypto::kSHA256Length);
  crypto::SHA256HashString(client_data, client_data_hash.data(),
                           client_data_hash.size());
  return client_data_hash;
}

// The application parameter is the SHA-256 hash of the UTF-8 encoding of
// the application identity (i.e. relying_party_id) of the application
// requesting the registration.
std::vector<uint8_t> CreateApplicationParameter(
    const std::string& relying_party_id) {
  std::vector<uint8_t> application_parameter(crypto::kSHA256Length);
  crypto::SHA256HashString(relying_party_id, application_parameter.data(),
                           application_parameter.size());
  return application_parameter;
}

base::Optional<std::vector<uint8_t>> ProcessAppIdExtension(
    std::string appid,
    const url::Origin& caller_origin) {
  if (appid.empty()) {
    // See step two in the comments in |IsAppIdAllowedForOrigin|.
    appid = caller_origin.Serialize() + "/";
  }

  GURL appid_url = GURL(appid);
  if (!(appid_url.is_valid() &&
        IsAppIdAllowedForOrigin(appid_url, caller_origin))) {
    return base::nullopt;
  }

  return CreateApplicationParameter(appid);
}

webauth::mojom::MakeCredentialAuthenticatorResponsePtr
CreateMakeCredentialResponse(const std::string& client_data_json,
                             device::RegisterResponseData response_data) {
  auto response = webauth::mojom::MakeCredentialAuthenticatorResponse::New();
  auto common_info = webauth::mojom::CommonCredentialInfo::New();
  common_info->client_data_json.assign(client_data_json.begin(),
                                       client_data_json.end());
  common_info->raw_id = response_data.raw_id();
  common_info->id = response_data.GetId();
  response->info = std::move(common_info);
  response->attestation_object =
      response_data.GetCBOREncodedAttestationObject();
  return response;
}

webauth::mojom::GetAssertionAuthenticatorResponsePtr CreateGetAssertionResponse(
    const std::string& client_data_json,
    device::SignResponseData response_data) {
  auto response = webauth::mojom::GetAssertionAuthenticatorResponse::New();
  auto common_info = webauth::mojom::CommonCredentialInfo::New();
  common_info->client_data_json.assign(client_data_json.begin(),
                                       client_data_json.end());
  common_info->raw_id = response_data.raw_id();
  common_info->id = response_data.GetId();
  response->info = std::move(common_info);
  response->authenticator_data = response_data.GetAuthenticatorDataBytes();
  response->signature = response_data.signature();
  response->user_handle.emplace();
  return response;
}

std::string Base64UrlEncode(const base::span<const uint8_t> input) {
  std::string ret;
  base::Base64UrlEncode(
      base::StringPiece(reinterpret_cast<const char*>(input.data()),
                        input.size()),
      base::Base64UrlEncodePolicy::OMIT_PADDING, &ret);
  return ret;
}

}  // namespace

AuthenticatorImpl::AuthenticatorImpl(RenderFrameHost* render_frame_host)
    : timer_(std::make_unique<base::OneShotTimer>()),
      render_frame_host_(render_frame_host),
      weak_factory_(this) {
  DCHECK(render_frame_host_);
  DCHECK(timer_);
}

AuthenticatorImpl::AuthenticatorImpl(RenderFrameHost* render_frame_host,
                                     service_manager::Connector* connector,
                                     std::unique_ptr<base::OneShotTimer> timer)
    : protocols_({/* no protocols in tests */}),
      timer_(std::move(timer)),
      render_frame_host_(render_frame_host),
      connector_(connector),
      weak_factory_(this) {
  DCHECK(render_frame_host_);
  DCHECK(timer_);
}

AuthenticatorImpl::~AuthenticatorImpl() {
  bindings_.CloseAllBindings();
}

void AuthenticatorImpl::Bind(webauth::mojom::AuthenticatorRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

// static
std::string AuthenticatorImpl::SerializeCollectedClientDataToJson(
    const std::string& type,
    const url::Origin& origin,
    base::span<const uint8_t> challenge,
    base::Optional<base::span<const uint8_t>> token_binding) {
  static constexpr char kTypeKey[] = "type";
  static constexpr char kChallengeKey[] = "challenge";
  static constexpr char kOriginKey[] = "origin";
  static constexpr char kTokenBindingKey[] = "tokenBinding";

  base::DictionaryValue client_data;
  client_data.SetKey(kTypeKey, base::Value(type));
  client_data.SetKey(kChallengeKey, base::Value(Base64UrlEncode(challenge)));
  client_data.SetKey(kOriginKey, base::Value(origin.Serialize()));

  base::DictionaryValue token_binding_dict;
  static constexpr char kTokenBindingStatusKey[] = "status";
  static constexpr char kTokenBindingIdKey[] = "id";
  static constexpr char kTokenBindingSupportedStatus[] = "supported";
  static constexpr char kTokenBindingNotSupportedStatus[] = "not-supported";
  static constexpr char kTokenBindingPresentStatus[] = "present";

  if (token_binding) {
    if (token_binding->empty()) {
      token_binding_dict.SetKey(kTokenBindingStatusKey,
                                base::Value(kTokenBindingSupportedStatus));
    } else {
      token_binding_dict.SetKey(kTokenBindingStatusKey,
                                base::Value(kTokenBindingPresentStatus));
      token_binding_dict.SetKey(kTokenBindingIdKey,
                                base::Value(Base64UrlEncode(*token_binding)));
    }
  } else {
    token_binding_dict.SetKey(kTokenBindingStatusKey,
                              base::Value(kTokenBindingNotSupportedStatus));
  }

  client_data.SetKey(kTokenBindingKey, std::move(token_binding_dict));

  if (base::RandDouble() < 0.2) {
    // An extra key is sometimes added to ensure that RPs do not make
    // unreasonably specific assumptions about the clientData JSON. This is
    // done in the fashion of
    // https://tools.ietf.org/html/draft-davidben-tls-grease-01
    client_data.SetKey("new_keys_may_be_added_here",
                       base::Value("do not compare clientDataJSON against a "
                                   "template. See https://goo.gl/yabPex"));
  }

  std::string json;
  base::JSONWriter::Write(client_data, &json);
  return json;
}

// mojom::Authenticator
void AuthenticatorImpl::MakeCredential(
    webauth::mojom::PublicKeyCredentialCreationOptionsPtr options,
    MakeCredentialCallback callback) {
  if (u2f_request_) {
    InvokeCallbackAndCleanup(
        std::move(callback),
        webauth::mojom::AuthenticatorStatus::PENDING_REQUEST, nullptr);
    return;
  }

  url::Origin caller_origin = render_frame_host_->GetLastCommittedOrigin();
  relying_party_id_ = options->relying_party->id;

  if (!HasValidEffectiveDomain(caller_origin)) {
    bad_message::ReceivedBadMessage(render_frame_host_->GetProcess(),
                                    bad_message::AUTH_INVALID_EFFECTIVE_DOMAIN);
    InvokeCallbackAndCleanup(
        std::move(callback),
        webauth::mojom::AuthenticatorStatus::INVALID_DOMAIN, nullptr);
    return;
  }

  if (!IsRelyingPartyIdValid(relying_party_id_, caller_origin)) {
    bad_message::ReceivedBadMessage(render_frame_host_->GetProcess(),
                                    bad_message::AUTH_INVALID_RELYING_PARTY);
    InvokeCallbackAndCleanup(
        std::move(callback),
        webauth::mojom::AuthenticatorStatus::INVALID_DOMAIN, nullptr);
    return;
  }

  // Check that at least one of the cryptographic parameters is supported.
  // Only ES256 is currently supported by U2F_V2.
  if (!HasValidAlgorithm(options->public_key_parameters)) {
    InvokeCallbackAndCleanup(
        std::move(callback),
        webauth::mojom::AuthenticatorStatus::NOT_SUPPORTED_ERROR, nullptr);
    return;
  }

  DCHECK(make_credential_response_callback_.is_null());
  make_credential_response_callback_ = std::move(callback);

  timer_->Start(
      FROM_HERE, options->adjusted_timeout,
      base::Bind(&AuthenticatorImpl::OnTimeout, base::Unretained(this)));
  if (!connector_)
    connector_ = ServiceManagerConnection::GetForProcess()->GetConnector();

  // Extract list of credentials to exclude.
  std::vector<std::vector<uint8_t>> registered_keys;
  for (const auto& credential : options->exclude_credentials) {
    registered_keys.push_back(credential->id);
  }

  // Save client data to return with the authenticator response.
  // TODO(kpaulhamus): Fetch and add the Token Binding ID public key used to
  // communicate with the origin.
  client_data_json_ = SerializeCollectedClientDataToJson(
      client_data::kCreateType, caller_origin, std::move(options->challenge),
      base::nullopt);

  const bool individual_attestation =
      GetContentClient()
          ->browser()
          ->ShouldPermitIndividualAttestationForWebauthnRPID(
              render_frame_host_->GetProcess()->GetBrowserContext(),
              relying_party_id_);

  attestation_preference_ = options->attestation;

  // TODO(kpaulhamus): Mock U2fRegister for unit tests.
  // http://crbug.com/785955.
  // Per fido-u2f-raw-message-formats:
  // The challenge parameter is the SHA-256 hash of the Client Data,
  // Among other things, the Client Data contains the challenge from the
  // relying party (hence the name of the parameter).
  u2f_request_ = device::U2fRegister::TryRegistration(
      connector_, protocols_, registered_keys,
      ConstructClientDataHash(client_data_json_),
      CreateApplicationParameter(relying_party_id_), individual_attestation,
      base::BindOnce(&AuthenticatorImpl::OnRegisterResponse,
                     weak_factory_.GetWeakPtr()));
}

// mojom:Authenticator
void AuthenticatorImpl::GetAssertion(
    webauth::mojom::PublicKeyCredentialRequestOptionsPtr options,
    GetAssertionCallback callback) {
  if (u2f_request_) {
    InvokeCallbackAndCleanup(
        std::move(callback),
        webauth::mojom::AuthenticatorStatus::PENDING_REQUEST, nullptr);
    return;
  }

  url::Origin caller_origin = render_frame_host_->GetLastCommittedOrigin();

  if (!HasValidEffectiveDomain(caller_origin)) {
    bad_message::ReceivedBadMessage(render_frame_host_->GetProcess(),
                                    bad_message::AUTH_INVALID_EFFECTIVE_DOMAIN);
    InvokeCallbackAndCleanup(
        std::move(callback),
        webauth::mojom::AuthenticatorStatus::INVALID_DOMAIN, nullptr);
    return;
  }

  if (!IsRelyingPartyIdValid(options->relying_party_id, caller_origin)) {
    bad_message::ReceivedBadMessage(render_frame_host_->GetProcess(),
                                    bad_message::AUTH_INVALID_RELYING_PARTY);
    InvokeCallbackAndCleanup(
        std::move(callback),
        webauth::mojom::AuthenticatorStatus::INVALID_DOMAIN, nullptr);
    return;
  }

  std::vector<uint8_t> application_parameter =
      CreateApplicationParameter(options->relying_party_id);

  base::Optional<std::vector<uint8_t>> alternative_application_parameter;
  if (options->appid) {
    auto appid_hash = ProcessAppIdExtension(*options->appid, caller_origin);
    if (!appid_hash) {
      std::move(callback).Run(
          webauth::mojom::AuthenticatorStatus::INVALID_DOMAIN, nullptr);
      return;
    }

    alternative_application_parameter = std::move(appid_hash);
  }

  DCHECK(get_assertion_response_callback_.is_null());
  get_assertion_response_callback_ = std::move(callback);

  // Pass along valid keys from allow_list, if any.
  std::vector<std::vector<uint8_t>> handles =
      FilterCredentialList(std::move(options->allow_credentials));

  timer_->Start(
      FROM_HERE, options->adjusted_timeout,
      base::Bind(&AuthenticatorImpl::OnTimeout, base::Unretained(this)));

  if (!connector_)
    connector_ = ServiceManagerConnection::GetForProcess()->GetConnector();

  // Save client data to return with the authenticator response.
  // TODO(kpaulhamus): Fetch and add the Token Binding ID public key used to
  // communicate with the origin.
  client_data_json_ = SerializeCollectedClientDataToJson(
      client_data::kGetType, caller_origin, std::move(options->challenge),
      base::nullopt);

  u2f_request_ = device::U2fSign::TrySign(
      connector_, protocols_, handles,
      ConstructClientDataHash(client_data_json_), application_parameter,
      alternative_application_parameter,
      base::BindOnce(&AuthenticatorImpl::OnSignResponse,
                     weak_factory_.GetWeakPtr()));
}

// Callback to handle the async registration response from a U2fDevice.
void AuthenticatorImpl::OnRegisterResponse(
    device::U2fReturnCode status_code,
    base::Optional<device::RegisterResponseData> response_data) {
  switch (status_code) {
    case device::U2fReturnCode::CONDITIONS_NOT_SATISFIED:
      // Duplicate registration: the new credential would be created on an
      // authenticator that already contains one of the credentials in
      // |exclude_credentials|.
      InvokeCallbackAndCleanup(
          std::move(make_credential_response_callback_),
          webauth::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR, nullptr);
      return;
    case device::U2fReturnCode::FAILURE:
      // The response from the authenticator was corrupted.
      InvokeCallbackAndCleanup(
          std::move(make_credential_response_callback_),
          webauth::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR, nullptr);
      return;
    case device::U2fReturnCode::INVALID_PARAMS:
      NOTREACHED();
      return;
    case device::U2fReturnCode::SUCCESS:
      DCHECK(response_data.has_value());

      if (attestation_preference_ !=
          webauth::mojom::AttestationConveyancePreference::NONE) {
        // Potentially show a permission prompt before returning the
        // attestation data.
        GetContentClient()->browser()->ShouldReturnAttestationForWebauthnRPID(
            render_frame_host_, relying_party_id_,
            render_frame_host_->GetLastCommittedOrigin(),
            base::BindOnce(
                &AuthenticatorImpl::OnRegisterResponseAttestationDecided,
                weak_factory_.GetWeakPtr(), std::move(*response_data)));
        return;
      }

      response_data->EraseAttestationStatement();
      InvokeCallbackAndCleanup(
          std::move(make_credential_response_callback_),
          webauth::mojom::AuthenticatorStatus::SUCCESS,
          CreateMakeCredentialResponse(std::move(client_data_json_),
                                       std::move(*response_data)));
      return;
  }
  NOTREACHED();
}

void AuthenticatorImpl::OnRegisterResponseAttestationDecided(
    device::RegisterResponseData response_data,
    bool attestation_permitted) {
  DCHECK(attestation_preference_ !=
         webauth::mojom::AttestationConveyancePreference::NONE);

  if (!attestation_permitted) {
    // To protect users from being identified without consent, we let the
    // timeout run out.
    // See https://w3c.github.io/webauthn/#sec-assertion-privacy.
    return;
  } else {
    InvokeCallbackAndCleanup(
        std::move(make_credential_response_callback_),
        webauth::mojom::AuthenticatorStatus::SUCCESS,
        CreateMakeCredentialResponse(std::move(client_data_json_),
                                     std::move(response_data)));
  }
}

void AuthenticatorImpl::OnSignResponse(
    device::U2fReturnCode status_code,
    base::Optional<device::SignResponseData> response_data) {
  switch (status_code) {
    case device::U2fReturnCode::CONDITIONS_NOT_SATISFIED:
      // No authenticators contained the credential.
      InvokeCallbackAndCleanup(
          std::move(get_assertion_response_callback_),
          webauth::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR, nullptr);
      return;
    case device::U2fReturnCode::FAILURE:
      // The response from the authenticator was corrupted.
      InvokeCallbackAndCleanup(
          std::move(make_credential_response_callback_),
          webauth::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR, nullptr);
      return;
    case device::U2fReturnCode::INVALID_PARAMS:
      NOTREACHED();
      return;
    case device::U2fReturnCode::SUCCESS:
      DCHECK(response_data.has_value());
      InvokeCallbackAndCleanup(
          std::move(get_assertion_response_callback_),
          webauth::mojom::AuthenticatorStatus::SUCCESS,
          CreateGetAssertionResponse(std::move(client_data_json_),
                                     std::move(*response_data)));
      return;
  }
  NOTREACHED();
}

void AuthenticatorImpl::OnTimeout() {
  // TODO(crbug.com/814418): Add layout tests to verify timeouts are
  // indistinguishable from NOT_ALLOWED_ERROR cases.
  DCHECK(make_credential_response_callback_ ||
         get_assertion_response_callback_);
  if (make_credential_response_callback_) {
    InvokeCallbackAndCleanup(
        std::move(make_credential_response_callback_),
        webauth::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR, nullptr);
  } else if (get_assertion_response_callback_) {
    InvokeCallbackAndCleanup(
        std::move(get_assertion_response_callback_),
        webauth::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR, nullptr);
  }
}

void AuthenticatorImpl::InvokeCallbackAndCleanup(
    MakeCredentialCallback callback,
    webauth::mojom::AuthenticatorStatus status,
    webauth::mojom::MakeCredentialAuthenticatorResponsePtr response) {
  std::move(callback).Run(status, std::move(response));
  Cleanup();
}

void AuthenticatorImpl::InvokeCallbackAndCleanup(
    GetAssertionCallback callback,
    webauth::mojom::AuthenticatorStatus status,
    webauth::mojom::GetAssertionAuthenticatorResponsePtr response) {
  std::move(callback).Run(status, std::move(response));
  Cleanup();
}

void AuthenticatorImpl::Cleanup() {
  timer_->Stop();
  u2f_request_.reset();
  make_credential_response_callback_.Reset();
  get_assertion_response_callback_.Reset();
  client_data_json_.clear();
}

}  // namespace content
