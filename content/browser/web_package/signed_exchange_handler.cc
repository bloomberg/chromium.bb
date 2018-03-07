// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_handler.h"

#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "components/cbor/cbor_reader.h"
#include "content/browser/loader/merkle_integrity_source_stream.h"
#include "content/browser/web_package/signed_exchange_cert_fetcher.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/browser/web_package/signed_exchange_header_parser.h"
#include "content/browser/web_package/signed_exchange_signature_verifier.h"
#include "content/public/common/content_features.h"
#include "content/public/common/shared_url_loader_factory.h"
#include "content/public/common/url_loader_throttle.h"
#include "mojo/public/cpp/system/string_data_pipe_producer.h"
#include "net/base/io_buffer.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/filter/source_stream.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_config_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/url_loader_completion_status.h"

namespace content {

namespace {

constexpr size_t kBufferSizeForRead = 65536;

constexpr char kMiHeader[] = "MI";

net::CertVerifier* g_cert_verifier_for_testing = nullptr;

cbor::CBORValue BytestringFromString(base::StringPiece in_string) {
  return cbor::CBORValue(
      std::vector<uint8_t>(in_string.begin(), in_string.end()));
}

bool IsStringEqualTo(const cbor::CBORValue& value, const char* str) {
  return value.is_string() && value.GetString() == str;
}

// TODO(https://crbug.com/803774): Just for now, remove once we have streaming
// CBOR parser.
class BufferSourceStream : public net::SourceStream {
 public:
  BufferSourceStream(const std::vector<uint8_t>& bytes)
      : net::SourceStream(SourceStream::TYPE_NONE), buf_(bytes), ptr_(0u) {}
  int Read(net::IOBuffer* dest_buffer,
           int buffer_size,
           const net::CompletionCallback& callback) override {
    int bytes = std::min(static_cast<int>(buf_.size() - ptr_), buffer_size);
    if (bytes > 0) {
      memcpy(dest_buffer->data(), &buf_[ptr_], bytes);
      ptr_ += bytes;
    }
    return bytes;
  }
  std::string Description() const override { return "buffer"; }

 private:
  std::vector<uint8_t> buf_;
  size_t ptr_;
};

}  // namespace

// static
void SignedExchangeHandler::SetCertVerifierForTesting(
    net::CertVerifier* cert_verifier) {
  g_cert_verifier_for_testing = cert_verifier;
}

SignedExchangeHandler::SignedExchangeHandler(
    std::unique_ptr<net::SourceStream> body,
    ExchangeHeadersCallback headers_callback,
    url::Origin request_initiator,
    scoped_refptr<SharedURLLoaderFactory> url_loader_factory,
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

  // Triggering the first read (asynchronously) for CBOR parsing.
  read_buf_ = base::MakeRefCounted<net::IOBufferWithSize>(kBufferSizeForRead);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&SignedExchangeHandler::ReadLoop,
                                weak_factory_.GetWeakPtr()));
}

SignedExchangeHandler::~SignedExchangeHandler() = default;

SignedExchangeHandler::SignedExchangeHandler() : weak_factory_(this) {}

void SignedExchangeHandler::ReadLoop() {
  DCHECK(headers_callback_);
  DCHECK(read_buf_);
  int rv = source_->Read(
      read_buf_.get(), read_buf_->size(),
      base::BindRepeating(&SignedExchangeHandler::DidRead,
                          base::Unretained(this), false /* sync */));
  if (rv != net::ERR_IO_PENDING)
    DidRead(true /* sync */, rv);
}

void SignedExchangeHandler::DidRead(bool completed_syncly, int result) {
  if (result < 0) {
    DVLOG(1) << "Error reading body stream: " << result;
    RunErrorCallback(static_cast<net::Error>(result));
    return;
  }

  if (result == 0) {
    if (!RunHeadersCallback())
      RunErrorCallback(net::ERR_FAILED);
    return;
  }

  // TODO(https://crbug.com/815019): This logic can cause browser-side DoS. We
  // MUST fix it before shipping.
  original_body_string_.append(read_buf_->data(), result);

  if (completed_syncly) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&SignedExchangeHandler::ReadLoop,
                                  weak_factory_.GetWeakPtr()));
  } else {
    ReadLoop();
  }
}

bool SignedExchangeHandler::RunHeadersCallback() {
  DCHECK(headers_callback_);

  cbor::CBORReader::DecoderError error;
  base::Optional<cbor::CBORValue> root = cbor::CBORReader::Read(
      base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(original_body_string_.data()),
          original_body_string_.size()),
      &error);
  if (!root) {
    DVLOG(1) << "CBOR parsing failed: "
             << cbor::CBORReader::ErrorCodeToString(error);
    return false;
  }
  original_body_string_.clear();

  if (!root->is_array()) {
    DVLOG(1) << "CBOR root is not an array";
    return false;
  }
  const auto& root_array = root->GetArray();
  if (!IsStringEqualTo(root_array[0], kHtxg)) {
    DVLOG(1) << "CBOR has no htxg signature";
    return false;
  }

  if (!IsStringEqualTo(root_array[1], kRequest)) {
    DVLOG(1) << "request field not found";
    return false;
  }

  if (!root_array[2].is_map()) {
    DVLOG(1) << "request field is not a map";
    return false;
  }
  const auto& request_map = root_array[2].GetMap();

  // TODO(https://crbug.com/803774): request payload may come here.

  if (!IsStringEqualTo(root_array[3], kResponse)) {
    DVLOG(1) << "response field not found";
    return false;
  }

  if (!root_array[4].is_map()) {
    DVLOG(1) << "response field is not a map";
    return false;
  }
  const auto& response_map = root_array[4].GetMap();

  if (!IsStringEqualTo(root_array[5], kPayload)) {
    DVLOG(1) << "payload field not found";
    return false;
  }

  if (!root_array[6].is_bytestring()) {
    DVLOG(1) << "payload field is not a bytestring";
    return false;
  }
  const auto& payload_bytes = root_array[6].GetBytestring();

  auto url_iter = request_map.find(BytestringFromString(kUrlKey));
  if (url_iter == request_map.end() || !url_iter->second.is_bytestring()) {
    DVLOG(1) << ":url is not found or not a bytestring";
    return false;
  }
  request_url_ = GURL(url_iter->second.GetBytestringAsString());

  auto method_iter = request_map.find(BytestringFromString(kMethodKey));
  if (method_iter == request_map.end() ||
      !method_iter->second.is_bytestring()) {
    DVLOG(1) << ":method is not found or not a bytestring";
    return false;
  }
  request_method_ = std::string(method_iter->second.GetBytestringAsString());

  auto status_iter = response_map.find(BytestringFromString(kStatusKey));
  if (status_iter == response_map.end() ||
      !status_iter->second.is_bytestring()) {
    DVLOG(1) << ":status is not found or not a bytestring";
    return false;
  }
  base::StringPiece status_code_str =
      status_iter->second.GetBytestringAsString();
  int status_code;
  if (!base::StringToInt(status_code_str, &status_code)) {
    DVLOG(1) << "Invalid status code " << status_code_str;
    return false;
  }

  base::Optional<std::vector<SignedExchangeHeaderParser::Signature>> signatures;
  // TODO(https://crbug.com/803774): Rename
  // SignedExchangeSignatureVerifier::Input to SignedExchangeSignatureHeader and
  // implement the following logic in SignedExchangeHeaderParser.
  auto verifier_input =
      std::make_unique<SignedExchangeSignatureVerifier::Input>();

  std::string fake_header_str("HTTP/1.1 ");
  status_code_str.AppendToString(&fake_header_str);
  fake_header_str.append(" OK\r\n");
  for (const auto& it : response_map) {
    if (!it.first.is_bytestring() || !it.second.is_bytestring()) {
      DVLOG(1) << "Non-bytestring value in the response map";
      return false;
    }
    base::StringPiece name = it.first.GetBytestringAsString();
    base::StringPiece value = it.second.GetBytestringAsString();
    if (name == kMethodKey)
      continue;

    // TODO(https://crbug.com/803774): Stop going through
    // net::HttpResponseHeaders but just use
    // SignedExchangeSignatureVerifier::Input.
    // TODO(https://crbug.com/803774): Check that name and value don't contain
    // special characters using IsValidHeaderName and IsValidHeaderValue.
    name.AppendToString(&fake_header_str);
    fake_header_str.append(": ");
    value.AppendToString(&fake_header_str);
    fake_header_str.append("\r\n");
    if (name == "signature")
      signatures = SignedExchangeHeaderParser::ParseSignature(value);
    verifier_input->response_headers[name.as_string()] = value.as_string();
  }
  fake_header_str.append("\r\n");
  response_head_.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(fake_header_str.c_str(),
                                        fake_header_str.size()));
  // TODO(https://crbug.com/803774): |mime_type| should be derived from
  // "Content-Type" header.
  response_head_.mime_type = "text/html";

  // TODO(https://crbug.com/803774): Check that the Signature header entry has
  // integrity="mi".
  std::string mi_header_value;
  if (!response_head_.headers->EnumerateHeader(nullptr, kMiHeader,
                                               &mi_header_value)) {
    DVLOG(1) << "Signed exchange has no MI: header";
    return false;
  }
  auto payload_stream = std::make_unique<BufferSourceStream>(payload_bytes);
  mi_stream_ = std::make_unique<MerkleIntegritySourceStream>(
      mi_header_value, std::move(payload_stream));

  if (!signatures || signatures->empty())
    return false;

  verifier_input->method = request_method_;
  verifier_input->url = request_url_.spec();
  verifier_input->response_code = status_code;
  verifier_input->signature = (*signatures)[0];

  // Copy |cert_url| to keep after |verifier_input| is passed to base::BindOnce.
  const GURL cert_url = verifier_input->signature.cert_url;
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
                     base::Unretained(this), std::move(verifier_input)));
  return true;
}

void SignedExchangeHandler::RunErrorCallback(net::Error error) {
  DCHECK(headers_callback_);
  std::move(headers_callback_)
      .Run(error, GURL(), std::string(), network::ResourceResponseHead(),
           nullptr, base::nullopt);
}

void SignedExchangeHandler::OnCertReceived(
    std::unique_ptr<SignedExchangeSignatureVerifier::Input> verifier_input,
    scoped_refptr<net::X509Certificate> cert) {
  if (!cert) {
    DVLOG(1) << "Fetching certificate error";
    RunErrorCallback(net::ERR_FAILED);
    return;
  }
  verifier_input->certificate = cert;
  if (SignedExchangeSignatureVerifier::Verify(*verifier_input) !=
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
          unverified_cert_, request_url_.host(), config.GetCertVerifyFlags(),
          std::string() /* ocsp_response */, net::CertificateList()),
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
      .Run(net::OK, request_url_, request_method_, response_head_,
           std::move(mi_stream_), ssl_info);
}

}  // namespace content
