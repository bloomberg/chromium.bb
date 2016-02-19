// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/session/assignment_source.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "blimp/client/app/blimp_client_switches.h"
#include "blimp/common/protocol_version.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"

namespace blimp {
namespace client {

namespace {

// Assignment request JSON keys.
const char kProtocolVersionKey[] = "protocol_version";

// Assignment response JSON keys.
const char kClientTokenKey[] = "clientToken";
const char kHostKey[] = "host";
const char kPortKey[] = "port";
const char kCertificateFingerprintKey[] = "certificateFingerprint";
const char kCertificateKey[] = "certificate";

// URL scheme constants for custom assignments.  See the '--blimplet-endpoint'
// documentation in blimp_client_switches.cc for details.
const char kCustomSSLScheme[] = "ssl";
const char kCustomTCPScheme[] = "tcp";
const char kCustomQUICScheme[] = "quic";

Assignment GetCustomBlimpletAssignment() {
  GURL url(base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kBlimpletEndpoint));

  std::string host;
  int port;
  if (url.is_empty() || !url.is_valid() || !url.has_scheme() ||
      !net::ParseHostAndPort(url.path(), &host, &port)) {
    return Assignment();
  }

  net::IPAddress ip_address;
  if (!ip_address.AssignFromIPLiteral(host)) {
    CHECK(false) << "Invalid BlimpletAssignment host " << host;
  }

  if (!base::IsValueInRangeForNumericType<uint16_t>(port)) {
    CHECK(false) << "Invalid BlimpletAssignment port " << port;
  }

  Assignment::TransportProtocol protocol =
      Assignment::TransportProtocol::UNKNOWN;
  if (url.has_scheme()) {
    if (url.SchemeIs(kCustomSSLScheme)) {
      protocol = Assignment::TransportProtocol::SSL;
    } else if (url.SchemeIs(kCustomTCPScheme)) {
      protocol = Assignment::TransportProtocol::TCP;
    } else if (url.SchemeIs(kCustomQUICScheme)) {
      protocol = Assignment::TransportProtocol::QUIC;
    } else {
      CHECK(false) << "Invalid BlimpletAssignment scheme " << url.scheme();
    }
  }

  Assignment assignment;
  assignment.transport_protocol = protocol;
  assignment.ip_endpoint = net::IPEndPoint(ip_address, port);
  assignment.client_token = kDummyClientToken;
  return assignment;
}

GURL GetBlimpAssignerURL() {
  // TODO(dtrainor): Add a way to specify another assigner.
  return GURL(kDefaultAssignerURL);
}

class SimpleURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  SimpleURLRequestContextGetter(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_loop_task_runner)
      : io_loop_task_runner_(io_loop_task_runner),
        proxy_config_service_(net::ProxyService::CreateSystemProxyConfigService(
            io_loop_task_runner_,
            io_loop_task_runner_)) {}

  // net::URLRequestContextGetter implementation.
  net::URLRequestContext* GetURLRequestContext() override {
    if (!url_request_context_) {
      net::URLRequestContextBuilder builder;
      builder.set_proxy_config_service(std::move(proxy_config_service_));
      builder.DisableHttpCache();
      url_request_context_ = builder.Build();
    }

    return url_request_context_.get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override {
    return io_loop_task_runner_;
  }

 private:
  ~SimpleURLRequestContextGetter() override {}

  scoped_refptr<base::SingleThreadTaskRunner> io_loop_task_runner_;
  scoped_ptr<net::URLRequestContext> url_request_context_;

  // Temporary storage for the ProxyConfigService, which needs to be created on
  // the main thread but cleared on the IO thread.  This will be built in the
  // constructor and cleared on the IO thread.  Due to the usage of this class
  // this is safe.
  scoped_ptr<net::ProxyConfigService> proxy_config_service_;

  DISALLOW_COPY_AND_ASSIGN(SimpleURLRequestContextGetter);
};

}  // namespace

Assignment::Assignment() : transport_protocol(TransportProtocol::UNKNOWN) {}

Assignment::~Assignment() {}

bool Assignment::is_null() const {
  return ip_endpoint.address().empty() || ip_endpoint.port() == 0 ||
         transport_protocol == TransportProtocol::UNKNOWN;
}

AssignmentSource::AssignmentSource(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : main_task_runner_(main_task_runner),
      url_request_context_(new SimpleURLRequestContextGetter(io_task_runner)) {}

AssignmentSource::~AssignmentSource() {}

void AssignmentSource::GetAssignment(const std::string& client_auth_token,
                                     const AssignmentCallback& callback) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Cancel any outstanding callback.
  if (!callback_.is_null()) {
    base::ResetAndReturn(&callback_)
        .Run(AssignmentSource::Result::RESULT_SERVER_INTERRUPTED, Assignment());
  }
  callback_ = AssignmentCallback(callback);

  Assignment assignment = GetCustomBlimpletAssignment();
  if (!assignment.is_null()) {
    // Post the result so that the behavior of this function is consistent.
    main_task_runner_->PostTask(
        FROM_HERE, base::Bind(base::ResetAndReturn(&callback_),
                              AssignmentSource::Result::RESULT_OK, assignment));
    return;
  }

  // Call out to the network for a real assignment.  Build the network request
  // to hit the assigner.
  url_fetcher_ = net::URLFetcher::Create(GetBlimpAssignerURL(),
                                         net::URLFetcher::POST, this);
  url_fetcher_->SetRequestContext(url_request_context_.get());
  url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                             net::LOAD_DO_NOT_SEND_COOKIES);
  url_fetcher_->AddExtraRequestHeader("Authorization: Bearer " +
                                      client_auth_token);

  // Write the JSON for the request data.
  base::DictionaryValue dictionary;
  dictionary.SetString(kProtocolVersionKey, blimp::kEngineVersion);
  std::string json;
  base::JSONWriter::Write(dictionary, &json);
  url_fetcher_->SetUploadData("application/json", json);

  url_fetcher_->Start();
}

void AssignmentSource::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(!callback_.is_null());
  DCHECK_EQ(url_fetcher_.get(), source);

  if (!source->GetStatus().is_success()) {
    DVLOG(1) << "Assignment request failed due to network error: "
             << net::ErrorToString(source->GetStatus().error());
    base::ResetAndReturn(&callback_)
        .Run(AssignmentSource::Result::RESULT_NETWORK_FAILURE, Assignment());
    return;
  }

  switch (source->GetResponseCode()) {
    case net::HTTP_OK:
      ParseAssignerResponse();
      break;
    case net::HTTP_BAD_REQUEST:
      base::ResetAndReturn(&callback_)
          .Run(AssignmentSource::Result::RESULT_BAD_REQUEST, Assignment());
      break;
    case net::HTTP_UNAUTHORIZED:
      base::ResetAndReturn(&callback_)
          .Run(AssignmentSource::Result::RESULT_EXPIRED_ACCESS_TOKEN,
               Assignment());
      break;
    case net::HTTP_FORBIDDEN:
      base::ResetAndReturn(&callback_)
          .Run(AssignmentSource::Result::RESULT_USER_INVALID, Assignment());
      break;
    case 429:  // Too Many Requests
      base::ResetAndReturn(&callback_)
          .Run(AssignmentSource::Result::RESULT_OUT_OF_VMS, Assignment());
      break;
    case net::HTTP_INTERNAL_SERVER_ERROR:
      base::ResetAndReturn(&callback_)
          .Run(AssignmentSource::Result::RESULT_SERVER_ERROR, Assignment());
      break;
    default:
      base::ResetAndReturn(&callback_)
          .Run(AssignmentSource::Result::RESULT_BAD_RESPONSE, Assignment());
      break;
  }
}

void AssignmentSource::ParseAssignerResponse() {
  DCHECK(url_fetcher_.get());
  DCHECK(url_fetcher_->GetStatus().is_success());
  DCHECK_EQ(net::HTTP_OK, url_fetcher_->GetResponseCode());

  // Grab the response from the assigner request.
  std::string response;
  if (!url_fetcher_->GetResponseAsString(&response)) {
    base::ResetAndReturn(&callback_)
        .Run(AssignmentSource::Result::RESULT_BAD_RESPONSE, Assignment());
    return;
  }

  // Attempt to interpret the response as JSON and treat it as a dictionary.
  scoped_ptr<base::Value> json = base::JSONReader::Read(response);
  if (!json) {
    base::ResetAndReturn(&callback_)
        .Run(AssignmentSource::Result::RESULT_BAD_RESPONSE, Assignment());
    return;
  }

  const base::DictionaryValue* dict;
  if (!json->GetAsDictionary(&dict)) {
    base::ResetAndReturn(&callback_)
        .Run(AssignmentSource::Result::RESULT_BAD_RESPONSE, Assignment());
    return;
  }

  // Validate that all the expected fields are present.
  std::string client_token;
  std::string host;
  int port;
  std::string cert_fingerprint;
  std::string cert;
  if (!(dict->GetString(kClientTokenKey, &client_token) &&
        dict->GetString(kHostKey, &host) && dict->GetInteger(kPortKey, &port) &&
        dict->GetString(kCertificateFingerprintKey, &cert_fingerprint) &&
        dict->GetString(kCertificateKey, &cert))) {
    base::ResetAndReturn(&callback_)
        .Run(AssignmentSource::Result::RESULT_BAD_RESPONSE, Assignment());
    return;
  }

  net::IPAddress ip_address;
  if (!ip_address.AssignFromIPLiteral(host)) {
    base::ResetAndReturn(&callback_)
        .Run(AssignmentSource::Result::RESULT_BAD_RESPONSE, Assignment());
    return;
  }

  if (!base::IsValueInRangeForNumericType<uint16_t>(port)) {
    base::ResetAndReturn(&callback_)
        .Run(AssignmentSource::Result::RESULT_BAD_RESPONSE, Assignment());
    return;
  }

  Assignment assignment;
  // The assigner assumes SSL-only and all engines it assigns only communicate
  // over SSL.
  assignment.transport_protocol = Assignment::TransportProtocol::SSL;
  assignment.ip_endpoint = net::IPEndPoint(ip_address, port);
  assignment.client_token = client_token;
  assignment.certificate = cert;
  assignment.certificate_fingerprint = cert_fingerprint;

  base::ResetAndReturn(&callback_)
      .Run(AssignmentSource::Result::RESULT_OK, assignment);
}

}  // namespace client
}  // namespace blimp
