// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_handler.h"

#include "base/feature_list.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/loader/merkle_integrity_source_stream.h"
#include "content/browser/web_package/signed_exchange_cert_fetcher_factory.h"
#include "content/browser/web_package/signed_exchange_certificate_chain.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/browser/web_package/signed_exchange_header.h"
#include "content/browser/web_package/signed_exchange_signature_verifier.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
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
#include "net/ssl/ssl_info.h"
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

void AddErrorMessageToConsole(int frame_tree_node_id,
                              const std::string& message) {
  // |frame_tree_node_id| is -1 for unittests.
  if (frame_tree_node_id == -1)
    return;
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&AddErrorMessageToConsole, frame_tree_node_id, message));
    return;
  }
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  WebContents* web_contents =
      WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (!web_contents)
    return;
  web_contents->GetMainFrame()->AddMessageToConsole(
      content::CONSOLE_MESSAGE_LEVEL_ERROR, message);
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
    std::string content_type,
    std::unique_ptr<net::SourceStream> body,
    ExchangeHeadersCallback headers_callback,
    std::unique_ptr<SignedExchangeCertFetcherFactory> cert_fetcher_factory,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    int frame_tree_node_id)
    : headers_callback_(std::move(headers_callback)),
      source_(std::move(body)),
      cert_fetcher_factory_(std::move(cert_fetcher_factory)),
      request_context_getter_(std::move(request_context_getter)),
      net_log_(net::NetLogWithSource::Make(
          request_context_getter_->GetURLRequestContext()->net_log(),
          net::NetLogSourceType::CERT_VERIFIER_JOB)),
      error_message_callback_(
          base::BindRepeating(&AddErrorMessageToConsole, frame_tree_node_id)),
      weak_factory_(this) {
  DCHECK(base::FeatureList::IsEnabled(features::kSignedHTTPExchange));
  TRACE_EVENT_BEGIN0(TRACE_DISABLED_BY_DEFAULT("loading"),
                     "SignedExchangeHandler::SignedExchangeHandler");

  base::Optional<std::string> content_type_version_param;
  if (!SignedExchangeHeaderParser::GetVersionParamFromContentType(
          content_type, &content_type_version_param) ||
      !content_type_version_param || *content_type_version_param != "b0") {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&SignedExchangeHandler::RunErrorCallback,
                                  weak_factory_.GetWeakPtr(), net::ERR_FAILED));
    signed_exchange_utils::RunErrorMessageCallbackAndEndTraceEvent(
        "SignedExchangeHandler::SignedExchangeHandler", error_message_callback_,
        base::StringPrintf("Unsupported version of the content type. Currentry "
                           "content type must be "
                           "\"application/signed-exchange;v=b0\". But the "
                           "response content type was \"%s\"",
                           content_type.c_str()));
    return;
  }

  // Triggering the read (asynchronously) for the encoded header length.
  SetupBuffers(SignedExchangeHeader::kEncodedHeaderLengthInBytes);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&SignedExchangeHandler::DoHeaderLoop,
                                weak_factory_.GetWeakPtr()));
  TRACE_EVENT_END0(TRACE_DISABLED_BY_DEFAULT("loading"),
                   "SignedExchangeCertFetcher::SignedExchangeHandler");
}

SignedExchangeHandler::~SignedExchangeHandler() = default;

SignedExchangeHandler::SignedExchangeHandler()
    : error_message_callback_(base::BindRepeating(&AddErrorMessageToConsole,
                                                  -1 /* frame_tree_node_id */)),
      weak_factory_(this) {}

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
  TRACE_EVENT_BEGIN0(TRACE_DISABLED_BY_DEFAULT("loading"),
                     "SignedExchangeHandler::DidReadHeader");
  if (result < 0) {
    RunErrorCallback(static_cast<net::Error>(result));
    signed_exchange_utils::RunErrorMessageCallbackAndEndTraceEvent(
        "SignedExchangeHandler::DidReadHeader", error_message_callback_,
        base::StringPrintf("Error reading body stream. result: %d", result));
    return;
  }

  if (result == 0) {
    RunErrorCallback(net::ERR_FAILED);
    signed_exchange_utils::RunErrorMessageCallbackAndEndTraceEvent(
        "SignedExchangeHandler::DidReadHeader", error_message_callback_,
        "Stream ended while reading signed exchange header.");
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
      state_ != State::kReadingHeaders) {
    TRACE_EVENT_END1(TRACE_DISABLED_BY_DEFAULT("loading"),
                     "SignedExchangeHandler::DidReadHeader", "state",
                     static_cast<int>(state_));
    return;
  }

  // Trigger the next read.
  if (completed_syncly) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&SignedExchangeHandler::DoHeaderLoop,
                                  weak_factory_.GetWeakPtr()));
  } else {
    DoHeaderLoop();
  }
  TRACE_EVENT_END0(TRACE_DISABLED_BY_DEFAULT("loading"),
                   "SignedExchangeCertFetcher::DidReadHeader");
}

bool SignedExchangeHandler::ParseHeadersLength() {
  TRACE_EVENT_BEGIN0(TRACE_DISABLED_BY_DEFAULT("loading"),
                     "SignedExchangeHandler::ParseHeadersLength");
  DCHECK_EQ(state_, State::kReadingHeadersLength);

  headers_length_ = SignedExchangeHeader::ParseHeadersLength(
      base::make_span(reinterpret_cast<uint8_t*>(header_buf_->data()),
                      SignedExchangeHeader::kEncodedHeaderLengthInBytes));
  if (headers_length_ == 0 || headers_length_ > kMaxHeadersCBORLength) {
    signed_exchange_utils::RunErrorMessageCallbackAndEndTraceEvent(
        "SignedExchangeHandler::ParseHeadersLength", error_message_callback_,
        base::StringPrintf("Invalid CBOR header length: %zu", headers_length_));
    return false;
  }

  // Set up a new buffer for CBOR-encoded buffer reading.
  SetupBuffers(headers_length_);
  state_ = State::kReadingHeaders;
  TRACE_EVENT_END0(TRACE_DISABLED_BY_DEFAULT("loading"),
                   "SignedExchangeCertFetcher::ParseHeadersLength");
  return true;
}

bool SignedExchangeHandler::ParseHeadersAndFetchCertificate() {
  TRACE_EVENT_BEGIN0(TRACE_DISABLED_BY_DEFAULT("loading"),
                     "SignedExchangeHandler::ParseHeadersAndFetchCertificate");
  DCHECK_EQ(state_, State::kReadingHeaders);

  header_ = SignedExchangeHeader::Parse(
      base::make_span(reinterpret_cast<uint8_t*>(header_buf_->data()),
                      headers_length_),
      error_message_callback_);
  header_read_buf_ = nullptr;
  header_buf_ = nullptr;
  if (!header_) {
    signed_exchange_utils::RunErrorMessageCallbackAndEndTraceEvent(
        "SignedExchangeHandler::ParseHeadersAndFetchCertificate",
        error_message_callback_, "Failed to parse SignedExchange header.");
    return false;
  }

  const GURL cert_url = header_->signature().cert_url;
  // TODO(https://crbug.com/819467): When we will support ed25519Key, |cert_url|
  // may be empty.
  DCHECK(cert_url.is_valid());

  DCHECK(cert_fetcher_factory_);
  cert_fetcher_ = std::move(cert_fetcher_factory_)
                      ->CreateFetcherAndStart(
                          cert_url, false,
                          base::BindOnce(&SignedExchangeHandler::OnCertReceived,
                                         base::Unretained(this)),
                          error_message_callback_);

  state_ = State::kFetchingCertificate;
  TRACE_EVENT_END0(TRACE_DISABLED_BY_DEFAULT("loading"),
                   "SignedExchangeHandler::ParseHeadersAndFetchCertificate");
  return true;
}

void SignedExchangeHandler::RunErrorCallback(net::Error error) {
  DCHECK_NE(state_, State::kHeadersCallbackCalled);
  std::move(headers_callback_)
      .Run(error, GURL(), std::string(), network::ResourceResponseHead(),
           nullptr);
  state_ = State::kHeadersCallbackCalled;
}

void SignedExchangeHandler::OnCertReceived(
    std::unique_ptr<SignedExchangeCertificateChain> cert_chain) {
  TRACE_EVENT_BEGIN0(TRACE_DISABLED_BY_DEFAULT("loading"),
                     "SignedExchangeHandler::OnCertReceived");
  DCHECK_EQ(state_, State::kFetchingCertificate);
  if (!cert_chain) {
    RunErrorCallback(net::ERR_FAILED);
    signed_exchange_utils::RunErrorMessageCallbackAndEndTraceEvent(
        "SignedExchangeHandler::OnCertReceived", error_message_callback_,
        "Failed to fetch the certificate.");
    return;
  }

  if (SignedExchangeSignatureVerifier::Verify(*header_, cert_chain->cert(),
                                              GetVerificationTime(),
                                              error_message_callback_) !=
      SignedExchangeSignatureVerifier::Result::kSuccess) {
    RunErrorCallback(net::ERR_FAILED);
    signed_exchange_utils::RunErrorMessageCallbackAndEndTraceEvent(
        "SignedExchangeHandler::OnCertReceived", error_message_callback_,
        "Failed to verify the signed exchange header.");
    return;
  }
  net::URLRequestContext* request_context =
      request_context_getter_->GetURLRequestContext();
  if (!request_context) {
    RunErrorCallback(net::ERR_CONTEXT_SHUT_DOWN);
    signed_exchange_utils::RunErrorMessageCallbackAndEndTraceEvent(
        "SignedExchangeHandler::OnCertReceived", error_message_callback_,
        "No request context available.");
    return;
  }

  unverified_cert_chain_ = std::move(cert_chain);

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
          unverified_cert_chain_->cert(), header_->request_url().host(),
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

  TRACE_EVENT_END0(TRACE_DISABLED_BY_DEFAULT("loading"),
                   "SignedExchangeHandler::OnCertReceived");
}

void SignedExchangeHandler::OnCertVerifyComplete(int result) {
  TRACE_EVENT_BEGIN0(TRACE_DISABLED_BY_DEFAULT("loading"),
                     "SignedExchangeHandler::OnCertVerifyComplete");

  if (result != net::OK) {
    RunErrorCallback(static_cast<net::Error>(result));
    signed_exchange_utils::RunErrorMessageCallbackAndEndTraceEvent(
        "SignedExchangeHandler::OnCertVerifyComplete", error_message_callback_,
        base::StringPrintf("Certificate verification error: %d", result));
    return;
  }

  network::ResourceResponseHead response_head;
  response_head.headers = header_->BuildHttpResponseHeaders();
  response_head.headers->GetMimeTypeAndCharset(&response_head.mime_type,
                                               &response_head.charset);

  // TODO(https://crbug.com/803774): Resource timing for signed exchange
  // loading is not speced yet. https://github.com/WICG/webpackage/issues/156
  response_head.load_timing.request_start_time = base::Time::Now();
  base::TimeTicks now(base::TimeTicks::Now());
  response_head.load_timing.request_start = now;
  response_head.load_timing.send_start = now;
  response_head.load_timing.send_end = now;
  response_head.load_timing.receive_headers_end = now;

  std::string mi_header_value;
  if (!response_head.headers->EnumerateHeader(nullptr, kMiHeader,
                                              &mi_header_value)) {
    RunErrorCallback(net::ERR_FAILED);
    signed_exchange_utils::RunErrorMessageCallbackAndEndTraceEvent(
        "SignedExchangeHandler::OnCertVerifyComplete", error_message_callback_,
        "Signed exchange has no MI: header");
    return;
  }
  auto mi_stream = std::make_unique<MerkleIntegritySourceStream>(
      mi_header_value, std::move(source_));

  net::SSLInfo ssl_info;
  ssl_info.cert = cert_verify_result_.verified_cert;
  ssl_info.unverified_cert = unverified_cert_chain_->cert();
  ssl_info.cert_status = cert_verify_result_.cert_status;
  ssl_info.is_issued_by_known_root =
      cert_verify_result_.is_issued_by_known_root;
  ssl_info.public_key_hashes = cert_verify_result_.public_key_hashes;
  ssl_info.ocsp_result = cert_verify_result_.ocsp_result;
  ssl_info.is_fatal_cert_error =
      net::IsCertStatusError(ssl_info.cert_status) &&
      !net::IsCertStatusMinorError(ssl_info.cert_status);
  response_head.ssl_info = std::move(ssl_info);
  // TODO(https://crbug.com/815025): Verify the Certificate Transparency status.
  std::move(headers_callback_)
      .Run(net::OK, header_->request_url(), header_->request_method(),
           response_head, std::move(mi_stream));
  state_ = State::kHeadersCallbackCalled;
  TRACE_EVENT_END0(TRACE_DISABLED_BY_DEFAULT("loading"),
                   "SignedExchangeHandler::OnCertVerifyComplete");
}

}  // namespace content
