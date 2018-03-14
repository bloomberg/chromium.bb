// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_handler.h"

#include "base/feature_list.h"
#include "base/time/time.h"
#include "content/browser/loader/merkle_integrity_source_stream.h"
#include "content/browser/web_package/signed_exchange_cert_fetcher.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/browser/web_package/signed_exchange_header.h"
#include "content/browser/web_package/signed_exchange_signature_verifier.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_loader_throttle.h"
#include "mojo/public/cpp/system/string_data_pipe_producer.h"
#include "net/base/io_buffer.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/filter/source_stream.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_config_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_completion_status.h"

namespace content {

namespace {

// 256KB (Maximum header size) * 2, since signed exchange header contains
// request and response headers.
constexpr size_t kMaxHeadersCBORLength = 512 * 1024;

constexpr char kMiHeader[] = "MI";

net::CertVerifier* g_cert_verifier_for_testing = nullptr;

base::Optional<base::Time> g_verification_time_for_testing;

base::Time GetVerificationTime() {
  if (g_verification_time_for_testing)
    return *g_verification_time_for_testing;
  return base::Time::Now();
}

}  // namespace

// static
void SignedExchangeHandler::SetCertVerifierForTesting(
    net::CertVerifier* cert_verifier) {
  g_cert_verifier_for_testing = cert_verifier;
}

// static
void SignedExchangeHandler::SetVerificationTimeForTesting(
    base::Optional<base::Time> verification_time_for_testing) {
  g_verification_time_for_testing = verification_time_for_testing;
}

SignedExchangeHandler::SignedExchangeHandler(
    std::unique_ptr<net::SourceStream> body,
    ExchangeHeadersCallback headers_callback,
    url::Origin request_initiator,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    URLLoaderThrottlesGetter url_loader_throttles_getter,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter)
    : headers_callback_(std::move(headers_callback)),
      source_(std::move(body)),
      request_initiator_(std::move(request_initiator)),
      url_loader_factory_(std::move(url_loader_factory)),
      url_loader_throttles_getter_(std::move(url_loader_throttles_getter)),
      request_context_getter_(std::move(request_context_getter)),
      net_log_(net::NetLogWithSource::Make(
          request_context_getter_->GetURLRequestContext()->net_log(),
          net::NetLogSourceType::CERT_VERIFIER_JOB)),
      weak_factory_(this) {
  DCHECK(base::FeatureList::IsEnabled(features::kSignedHTTPExchange));

  // Triggering the read (asynchronously) for the encoded header length.
  SetupBuffers(SignedExchangeHeader::kEncodedHeaderLengthInBytes);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&SignedExchangeHandler::DoHeaderLoop,
                                weak_factory_.GetWeakPtr()));
}

SignedExchangeHandler::~SignedExchangeHandler() = default;

SignedExchangeHandler::SignedExchangeHandler() : weak_factory_(this) {}

void SignedExchangeHandler::SetupBuffers(size_t size) {
  header_buf_ = base::MakeRefCounted<net::IOBuffer>(size);
  header_read_buf_ =
      base::MakeRefCounted<net::DrainableIOBuffer>(header_buf_.get(), size);
}

void SignedExchangeHandler::DoHeaderLoop() {
  DCHECK(state_ == State::kReadingHeadersLength ||
         state_ == State::kReadingHeaders);
  int rv = source_->Read(
      header_read_buf_.get(), header_read_buf_->BytesRemaining(),
      base::BindRepeating(&SignedExchangeHandler::DidReadHeader,
                          base::Unretained(this), false /* sync */));
  if (rv != net::ERR_IO_PENDING)
    DidReadHeader(true /* sync */, rv);
}

void SignedExchangeHandler::DidReadHeader(bool completed_syncly, int result) {
  if (result < 0) {
    DVLOG(1) << "Error reading body stream: " << result;
    RunErrorCallback(static_cast<net::Error>(result));
    return;
  }

  if (result == 0) {
    DVLOG(1) << "Stream ended while reading signed exchange header.";
    RunErrorCallback(net::ERR_FAILED);
    return;
  }

  header_read_buf_->DidConsume(result);
  if (header_read_buf_->BytesRemaining() == 0) {
    switch (state_) {
      case State::kReadingHeadersLength:
        if (!ParseHeadersLength())
          RunErrorCallback(net::ERR_FAILED);
        break;
      case State::kReadingHeaders:
        if (!ParseHeadersAndFetchCertificate())
          RunErrorCallback(net::ERR_FAILED);
        break;
      default:
        NOTREACHED();
    }
  }

  if (state_ != State::kReadingHeadersLength &&
      state_ != State::kReadingHeaders)
    return;

  // Trigger the next read.
  if (completed_syncly) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&SignedExchangeHandler::DoHeaderLoop,
                                  weak_factory_.GetWeakPtr()));
  } else {
    DoHeaderLoop();
  }
}

bool SignedExchangeHandler::ParseHeadersLength() {
  DCHECK_EQ(state_, State::kReadingHeadersLength);

  headers_length_ = SignedExchangeHeader::ParseHeadersLength(
      base::make_span(reinterpret_cast<uint8_t*>(header_buf_->data()),
                      SignedExchangeHeader::kEncodedHeaderLengthInBytes));
  if (headers_length_ == 0 || headers_length_ > kMaxHeadersCBORLength) {
    DVLOG(1) << "Invalid CBOR header length: " << headers_length_;
    return false;
  }

  // Set up a new buffer for CBOR-encoded buffer reading.
  SetupBuffers(headers_length_);
  state_ = State::kReadingHeaders;
  return true;
}

bool SignedExchangeHandler::ParseHeadersAndFetchCertificate() {
  DCHECK_EQ(state_, State::kReadingHeaders);

  header_ = SignedExchangeHeader::Parse(base::make_span(
      reinterpret_cast<uint8_t*>(header_buf_->data()), headers_length_));
  header_read_buf_ = nullptr;
  header_buf_ = nullptr;
  if (!header_) {
    DVLOG(1) << "Failed to parse CBOR header";
    return false;
  }

  const GURL cert_url = header_->signature().cert_url;
  // TODO(https://crbug.com/819467): When we will support ed25519Key, |cert_url|
  // may be empty.
  DCHECK(cert_url.is_valid());

  DCHECK(url_loader_factory_);
  DCHECK(url_loader_throttles_getter_);
  std::vector<std::unique_ptr<URLLoaderThrottle>> throttles =
      std::move(url_loader_throttles_getter_).Run();
  cert_fetcher_ = SignedExchangeCertFetcher::CreateAndStart(
      std::move(url_loader_factory_), std::move(throttles), cert_url,
      std::move(request_initiator_), false,
      base::BindOnce(&SignedExchangeHandler::OnCertReceived,
                     base::Unretained(this)));

  state_ = State::kFetchingCertificate;
  return true;
}

void SignedExchangeHandler::RunErrorCallback(net::Error error) {
  DCHECK_NE(state_, State::kHeadersCallbackCalled);
  std::move(headers_callback_)
      .Run(error, GURL(), std::string(), network::ResourceResponseHead(),
           nullptr, base::nullopt);
  state_ = State::kHeadersCallbackCalled;
}

void SignedExchangeHandler::OnCertReceived(
    scoped_refptr<net::X509Certificate> cert) {
  DCHECK_EQ(state_, State::kFetchingCertificate);
  if (!cert) {
    DVLOG(1) << "Fetching certificate error";
    RunErrorCallback(net::ERR_FAILED);
    return;
  }

  if (SignedExchangeSignatureVerifier::Verify(*header_, cert,
                                              GetVerificationTime()) !=
      SignedExchangeSignatureVerifier::Result::kSuccess) {
    RunErrorCallback(net::ERR_FAILED);
    return;
  }
  net::URLRequestContext* request_context =
      request_context_getter_->GetURLRequestContext();
  if (!request_context) {
    RunErrorCallback(net::ERR_CONTEXT_SHUT_DOWN);
    return;
  }

  unverified_cert_ = cert;

  net::SSLConfig config;
  request_context->ssl_config_service()->GetSSLConfig(&config);

  net::CertVerifier* cert_verifier = g_cert_verifier_for_testing
                                         ? g_cert_verifier_for_testing
                                         : request_context->cert_verifier();
  // TODO(https://crbug.com/815024): Get the OCSP response from the
  // “status_request” extension of the main-certificate, and check the lifetime
  // (nextUpdate - thisUpdate) is less than 7 days.
  int result = cert_verifier->Verify(
      net::CertVerifier::RequestParams(
          unverified_cert_, header_->request_url().host(),
          config.GetCertVerifyFlags(), std::string() /* ocsp_response */,
          net::CertificateList()),
      net::SSLConfigService::GetCRLSet().get(), &cert_verify_result_,
      base::BindRepeating(&SignedExchangeHandler::OnCertVerifyComplete,
                          base::Unretained(this)),
      &cert_verifier_request_, net_log_);
  // TODO(https://crbug.com/803774): Avoid these recursive patterns by using
  // explicit state machines (eg: DoLoop() in //net).
  if (result != net::ERR_IO_PENDING)
    OnCertVerifyComplete(result);
}

void SignedExchangeHandler::OnCertVerifyComplete(int result) {
  if (result != net::OK) {
    DVLOG(1) << "Certificate verification error: " << result;
    RunErrorCallback(static_cast<net::Error>(result));
    return;
  }

  network::ResourceResponseHead response_head;
  response_head.headers = header_->BuildHttpResponseHeaders();
  // TODO(https://crbug.com/803774): |mime_type| should be derived from
  // "Content-Type" header.
  response_head.mime_type = "text/html";

  std::string mi_header_value;
  if (!response_head.headers->EnumerateHeader(nullptr, kMiHeader,
                                              &mi_header_value)) {
    DVLOG(1) << "Signed exchange has no MI: header";
    RunErrorCallback(net::ERR_FAILED);
    return;
  }
  auto mi_stream = std::make_unique<MerkleIntegritySourceStream>(
      mi_header_value, std::move(source_));

  net::SSLInfo ssl_info;
  ssl_info.cert = cert_verify_result_.verified_cert;
  ssl_info.unverified_cert = unverified_cert_;
  ssl_info.cert_status = cert_verify_result_.cert_status;
  ssl_info.is_issued_by_known_root =
      cert_verify_result_.is_issued_by_known_root;
  ssl_info.public_key_hashes = cert_verify_result_.public_key_hashes;
  ssl_info.ocsp_result = cert_verify_result_.ocsp_result;
  ssl_info.is_fatal_cert_error =
      net::IsCertStatusError(ssl_info.cert_status) &&
      !net::IsCertStatusMinorError(ssl_info.cert_status);
  // TODO(https://crbug.com/815025): Verify the Certificate Transparency status.
  std::move(headers_callback_)
      .Run(net::OK, header_->request_url(), header_->request_method(),
           response_head, std::move(mi_stream), ssl_info);
  state_ = State::kHeadersCallbackCalled;
}

}  // namespace content
