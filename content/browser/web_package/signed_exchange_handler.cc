// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_handler.h"

#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/loader/merkle_integrity_source_stream.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/web_package/signed_exchange_cert_fetcher_factory.h"
#include "content/browser/web_package/signed_exchange_certificate_chain.h"
#include "content/browser/web_package/signed_exchange_devtools_proxy.h"
#include "content/browser/web_package/signed_exchange_envelope.h"
#include "content/browser/web_package/signed_exchange_prologue.h"
#include "content/browser/web_package/signed_exchange_signature_verifier.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_loader_throttle.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/system/string_data_pipe_producer.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/filter/source_stream.h"
#include "net/ssl/ssl_info.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace content {

namespace {

constexpr char kDigestHeader[] = "Digest";

network::mojom::NetworkContext* g_network_context_for_testing = nullptr;

base::Optional<base::Time> g_verification_time_for_testing;

base::Time GetVerificationTime() {
  if (g_verification_time_for_testing)
    return *g_verification_time_for_testing;
  return base::Time::Now();
}

bool IsSupportedSignedExchangeVersion(
    const base::Optional<SignedExchangeVersion>& version) {
  return version == SignedExchangeVersion::kB2;
}

using VerifyCallback = base::OnceCallback<void(int32_t,
                                               const net::CertVerifyResult&,
                                               const net::ct::CTVerifyResult&)>;

void OnVerifyCertUI(VerifyCallback callback,
                    int32_t error_code,
                    const net::CertVerifyResult& cv_result,
                    const net::ct::CTVerifyResult& ct_result) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(std::move(callback), error_code, cv_result, ct_result));
}

void VerifyCert(const scoped_refptr<net::X509Certificate>& certificate,
                const GURL& url,
                const std::string& ocsp_result,
                const std::string& sct_list,
                base::RepeatingCallback<int(void)> frame_tree_node_id_getter,
                VerifyCallback callback) {
  VerifyCallback wrapped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindOnce(OnVerifyCertUI, std::move(callback)), net::ERR_FAILED,
      net::CertVerifyResult(), net::ct::CTVerifyResult());

  network::mojom::NetworkContext* network_context =
      g_network_context_for_testing;
  if (!network_context) {
    auto* frame =
        FrameTreeNode::GloballyFindByID(frame_tree_node_id_getter.Run());
    if (!frame)
      return;

    network_context = frame->current_frame_host()
                          ->GetProcess()
                          ->GetStoragePartition()
                          ->GetNetworkContext();
  }

  network_context->VerifyCertForSignedExchange(
      certificate, url, ocsp_result, sct_list, std::move(wrapped_callback));
}

}  // namespace

// static
void SignedExchangeHandler::SetNetworkContextForTesting(
    network::mojom::NetworkContext* network_context) {
  g_network_context_for_testing = network_context;
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
    int load_flags,
    std::unique_ptr<SignedExchangeDevToolsProxy> devtools_proxy,
    base::RepeatingCallback<int(void)> frame_tree_node_id_getter)
    : headers_callback_(std::move(headers_callback)),
      source_(std::move(body)),
      cert_fetcher_factory_(std::move(cert_fetcher_factory)),
      load_flags_(load_flags),
      devtools_proxy_(std::move(devtools_proxy)),
      frame_tree_node_id_getter_(frame_tree_node_id_getter),
      weak_factory_(this) {
  DCHECK(signed_exchange_utils::IsSignedExchangeHandlingEnabled());
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeHandler::SignedExchangeHandler");

  if (!SignedExchangeSignatureHeaderField::GetVersionParamFromContentType(
          content_type, &version_) ||
      !IsSupportedSignedExchangeVersion(version_)) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_.get(),
        base::StringPrintf("Unsupported version of the content type. Currentry "
                           "content type must be "
                           "\"application/signed-exchange;v=b2\". But the "
                           "response content type was \"%s\"",
                           content_type.c_str()));
    // Proceed to extract and redirect to the fallback URL.
  }

  // Triggering the read (asynchronously) for the prologue bytes.
  SetupBuffers(
      signed_exchange_prologue::BeforeFallbackUrl::kEncodedSizeInBytes);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&SignedExchangeHandler::DoHeaderLoop,
                                weak_factory_.GetWeakPtr()));
}

SignedExchangeHandler::~SignedExchangeHandler() = default;

SignedExchangeHandler::SignedExchangeHandler()
    : load_flags_(net::LOAD_NORMAL), weak_factory_(this) {}

const GURL& SignedExchangeHandler::GetFallbackUrl() const {
  return prologue_fallback_url_and_after_.fallback_url();
}

void SignedExchangeHandler::SetupBuffers(size_t size) {
  header_buf_ = base::MakeRefCounted<net::IOBuffer>(size);
  header_read_buf_ =
      base::MakeRefCounted<net::DrainableIOBuffer>(header_buf_.get(), size);
}

void SignedExchangeHandler::DoHeaderLoop() {
  DCHECK(state_ == State::kReadingPrologueBeforeFallbackUrl ||
         state_ == State::kReadingPrologueFallbackUrlAndAfter ||
         state_ == State::kReadingHeaders);
  int rv = source_->Read(
      header_read_buf_.get(), header_read_buf_->BytesRemaining(),
      base::BindRepeating(&SignedExchangeHandler::DidReadHeader,
                          base::Unretained(this), false /* sync */));
  if (rv != net::ERR_IO_PENDING)
    DidReadHeader(true /* sync */, rv);
}

void SignedExchangeHandler::DidReadHeader(bool completed_syncly, int result) {
  DCHECK(state_ == State::kReadingPrologueBeforeFallbackUrl ||
         state_ == State::kReadingPrologueFallbackUrlAndAfter ||
         state_ == State::kReadingHeaders);

  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeHandler::DidReadHeader");
  if (result < 0) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_.get(),
        base::StringPrintf("Error reading body stream. result: %d", result));
    RunErrorCallback(static_cast<net::Error>(result));
    return;
  }

  if (result == 0) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_.get(),
        "Stream ended while reading signed exchange header.");
    RunErrorCallback(net::ERR_INVALID_SIGNED_EXCHANGE);
    return;
  }

  header_read_buf_->DidConsume(result);
  if (header_read_buf_->BytesRemaining() == 0) {
    switch (state_) {
      case State::kReadingPrologueBeforeFallbackUrl:
        if (!ParsePrologueBeforeFallbackUrl()) {
          RunErrorCallback(net::ERR_INVALID_SIGNED_EXCHANGE);
          return;
        }
        break;
      case State::kReadingPrologueFallbackUrlAndAfter:
        if (!ParsePrologueFallbackUrlAndAfter()) {
          RunErrorCallback(net::ERR_INVALID_SIGNED_EXCHANGE);
          return;
        }
        break;
      case State::kReadingHeaders:
        if (!ParseHeadersAndFetchCertificate()) {
          RunErrorCallback(net::ERR_INVALID_SIGNED_EXCHANGE);
          return;
        }
        break;
      default:
        NOTREACHED();
    }
  }

  // We have finished reading headers, so return without queueing the next read.
  if (state_ == State::kFetchingCertificate)
    return;

  // Trigger the next read.
  DCHECK(state_ == State::kReadingPrologueBeforeFallbackUrl ||
         state_ == State::kReadingPrologueFallbackUrlAndAfter ||
         state_ == State::kReadingHeaders);
  if (completed_syncly) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&SignedExchangeHandler::DoHeaderLoop,
                                  weak_factory_.GetWeakPtr()));
  } else {
    DoHeaderLoop();
  }
}

bool SignedExchangeHandler::ParsePrologueBeforeFallbackUrl() {
  DCHECK_EQ(state_, State::kReadingPrologueBeforeFallbackUrl);

  prologue_before_fallback_url_ =
      signed_exchange_prologue::BeforeFallbackUrl::Parse(
          base::make_span(
              reinterpret_cast<uint8_t*>(header_buf_->data()),
              signed_exchange_prologue::BeforeFallbackUrl::kEncodedSizeInBytes),
          devtools_proxy_.get());

  // Note: We will proceed even if |!prologue_before_fallback_url_.is_valid()|
  //       to attempt reading `fallbackUrl`.

  // Set up a new buffer for reading
  // |signed_exchange_prologue::FallbackUrlAndAfter|.
  SetupBuffers(
      prologue_before_fallback_url_.ComputeFallbackUrlAndAfterLength());
  state_ = State::kReadingPrologueFallbackUrlAndAfter;
  return true;
}

bool SignedExchangeHandler::ParsePrologueFallbackUrlAndAfter() {
  DCHECK_EQ(state_, State::kReadingPrologueFallbackUrlAndAfter);

  prologue_fallback_url_and_after_ =
      signed_exchange_prologue::FallbackUrlAndAfter::Parse(
          base::make_span(
              reinterpret_cast<uint8_t*>(header_buf_->data()),
              prologue_before_fallback_url_.ComputeFallbackUrlAndAfterLength()),
          prologue_before_fallback_url_, devtools_proxy_.get());
  if (!prologue_fallback_url_and_after_.is_valid()) {
    return false;
  }

  // If the signed exchange version from content-type is unsupported, abort
  // parsing and redirect to the fallback URL.
  if (!IsSupportedSignedExchangeVersion(version_))
    return false;

  // Set up a new buffer for reading the Signature header field and CBOR-encoded
  // headers.
  SetupBuffers(
      prologue_fallback_url_and_after_.ComputeFollowingSectionsLength());
  state_ = State::kReadingHeaders;
  return true;
}

bool SignedExchangeHandler::ParseHeadersAndFetchCertificate() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeHandler::ParseHeadersAndFetchCertificate");
  DCHECK_EQ(state_, State::kReadingHeaders);

  base::StringPiece data(header_buf_->data(), header_read_buf_->size());
  base::StringPiece signature_header_field = data.substr(
      0, prologue_fallback_url_and_after_.signature_header_field_length());
  base::span<const uint8_t> cbor_header =
      base::as_bytes(base::make_span(data.substr(
          prologue_fallback_url_and_after_.signature_header_field_length(),
          prologue_fallback_url_and_after_.cbor_header_length())));
  envelope_ =
      SignedExchangeEnvelope::Parse(GetFallbackUrl(), signature_header_field,
                                    cbor_header, devtools_proxy_.get());
  header_read_buf_ = nullptr;
  header_buf_ = nullptr;
  if (!envelope_) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_.get(), "Failed to parse SignedExchange header.");
    return false;
  }

  const GURL cert_url = envelope_->signature().cert_url;
  // TODO(https://crbug.com/819467): When we will support ed25519Key, |cert_url|
  // may be empty.
  DCHECK(cert_url.is_valid());
  DCHECK(version_.has_value());

  DCHECK(cert_fetcher_factory_);

  const bool force_fetch = load_flags_ & net::LOAD_BYPASS_CACHE;

  cert_fetcher_ = std::move(cert_fetcher_factory_)
                      ->CreateFetcherAndStart(
                          cert_url, force_fetch, *version_,
                          base::BindOnce(&SignedExchangeHandler::OnCertReceived,
                                         base::Unretained(this)),
                          devtools_proxy_.get());

  state_ = State::kFetchingCertificate;
  return true;
}

void SignedExchangeHandler::RunErrorCallback(net::Error error) {
  DCHECK_NE(state_, State::kHeadersCallbackCalled);
  if (devtools_proxy_) {
    devtools_proxy_->OnSignedExchangeReceived(
        envelope_,
        unverified_cert_chain_ ? unverified_cert_chain_->cert()
                               : scoped_refptr<net::X509Certificate>(),
        nullptr);
  }
  std::move(headers_callback_)
      .Run(error, GetFallbackUrl(), std::string(),
           network::ResourceResponseHead(), nullptr);
  state_ = State::kHeadersCallbackCalled;
}

void SignedExchangeHandler::OnCertReceived(
    std::unique_ptr<SignedExchangeCertificateChain> cert_chain) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeHandler::OnCertReceived");
  DCHECK_EQ(state_, State::kFetchingCertificate);
  if (!cert_chain) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_.get(), "Failed to fetch the certificate.",
        std::make_pair(0 /* signature_index */,
                       SignedExchangeError::Field::kSignatureCertUrl));
    RunErrorCallback(net::ERR_INVALID_SIGNED_EXCHANGE);
    return;
  }

  unverified_cert_chain_ = std::move(cert_chain);

  const SignedExchangeSignatureVerifier::Result verify_result =
      SignedExchangeSignatureVerifier::Verify(
          *envelope_, unverified_cert_chain_->cert(), GetVerificationTime(),
          devtools_proxy_.get());
  if (verify_result != SignedExchangeSignatureVerifier::Result::kSuccess) {
    base::Optional<SignedExchangeError::Field> error_field =
        SignedExchangeError::GetFieldFromSignatureVerifierResult(verify_result);
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_.get(), "Failed to verify the signed exchange header.",
        error_field ? base::make_optional(
                          std::make_pair(0 /* signature_index */, *error_field))
                    : base::nullopt);
    RunErrorCallback(net::ERR_INVALID_SIGNED_EXCHANGE);
    return;
  }

  auto certificate = unverified_cert_chain_->cert();
  auto url = envelope_->request_url();

  // https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#cross-origin-trust
  // Step 6.4 Validate that valid SCTs from trusted logs are available from any
  // of:
  // - The SignedCertificateTimestampList in main-certificate’s sct property
  //   (Section 3.3),
  const std::string& sct_list_from_cert_cbor = unverified_cert_chain_->sct();
  // - An OCSP extension in the OCSP response in main-certificate’s ocsp
  //   property, or
  const std::string& stapled_ocsp_response = unverified_cert_chain_->ocsp();

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&VerifyCert, certificate, url, stapled_ocsp_response,
                     sct_list_from_cert_cbor, frame_tree_node_id_getter_,
                     base::BindOnce(&SignedExchangeHandler::OnVerifyCert,
                                    weak_factory_.GetWeakPtr())));
}

bool SignedExchangeHandler::CheckCertExtension(
    const net::X509Certificate* verified_cert) {
  if (base::FeatureList::IsEnabled(
          features::kAllowSignedHTTPExchangeCertsWithoutExtension))
    return true;

  // https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#cross-origin-trust
  // Step 6.2. Validate that main-certificate has the CanSignHttpExchanges
  // extension (Section 4.2). [spec text]
  return net::asn1::HasCanSignHttpExchangesDraftExtension(
      net::x509_util::CryptoBufferAsStringPiece(verified_cert->cert_buffer()));
}

bool SignedExchangeHandler::CheckOCSPStatus(
    const net::OCSPVerifyResult& ocsp_result) {
  // https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#cross-origin-trust
  // Step 6.3 Validate that main-certificate has an ocsp property (Section 3.3)
  // with a valid OCSP response whose lifetime (nextUpdate - thisUpdate) is less
  // than 7 days ([RFC6960]). [spec text]
  //
  // OCSP verification is done in CertVerifier::Verify(), so we just check the
  // result here.

  if (ocsp_result.response_status != net::OCSPVerifyResult::PROVIDED ||
      ocsp_result.revocation_status != net::OCSPRevocationStatus::GOOD)
    return false;

  return true;
}

void SignedExchangeHandler::OnVerifyCert(
    int32_t error_code,
    const net::CertVerifyResult& cv_result,
    const net::ct::CTVerifyResult& ct_result) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeHandler::OnCertVerifyComplete");

  if (error_code != net::OK) {
    std::string error_message;
    if (error_code == net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED) {
      error_message = base::StringPrintf(
          "CT verification failed. result: %s, policy compliance: %d",
          net::ErrorToShortString(error_code).c_str(),
          ct_result.policy_compliance);
    } else {
      error_message =
          base::StringPrintf("Certificate verification error: %s",
                             net::ErrorToShortString(error_code).c_str());
    }
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_.get(), error_message,
        std::make_pair(0 /* signature_index */,
                       SignedExchangeError::Field::kSignatureCertUrl));
    RunErrorCallback(net::ERR_INVALID_SIGNED_EXCHANGE);
    return;
  }

  if (!CheckCertExtension(cv_result.verified_cert.get())) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_.get(),
        "Certificate must have CanSignHttpExchangesDraft extension. To ignore "
        "this error for testing, enable "
        "chrome://flags/#allow-sxg-certs-without-extension.",
        std::make_pair(0 /* signature_index */,
                       SignedExchangeError::Field::kSignatureCertUrl));
    RunErrorCallback(net::ERR_INVALID_SIGNED_EXCHANGE);
    return;
  }

  if (!CheckOCSPStatus(cv_result.ocsp_result)) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_.get(),
        base::StringPrintf(
            "OCSP check failed. response status: %d, revocation status: %d",
            cv_result.ocsp_result.response_status,
            cv_result.ocsp_result.revocation_status),
        std::make_pair(0 /* signature_index */,
                       SignedExchangeError::Field::kSignatureCertUrl));
    RunErrorCallback(net::ERR_INVALID_SIGNED_EXCHANGE);
    return;
  }

  network::ResourceResponseHead response_head;
  response_head.headers = envelope_->BuildHttpResponseHeaders();
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

  std::string digest_header_value;
  if (!response_head.headers->EnumerateHeader(nullptr, kDigestHeader,
                                              &digest_header_value)) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_.get(), "Signed exchange has no Digest: header");
    RunErrorCallback(net::ERR_INVALID_SIGNED_EXCHANGE);
    return;
  }
  auto mi_stream = std::make_unique<MerkleIntegritySourceStream>(
      digest_header_value, std::move(source_));

  net::SSLInfo ssl_info;
  ssl_info.cert = cv_result.verified_cert;
  ssl_info.unverified_cert = unverified_cert_chain_->cert();
  ssl_info.cert_status = cv_result.cert_status;
  ssl_info.is_issued_by_known_root = cv_result.is_issued_by_known_root;
  ssl_info.public_key_hashes = cv_result.public_key_hashes;
  ssl_info.ocsp_result = cv_result.ocsp_result;
  ssl_info.is_fatal_cert_error =
      net::IsCertStatusError(ssl_info.cert_status) &&
      !net::IsCertStatusMinorError(ssl_info.cert_status);
  ssl_info.UpdateCertificateTransparencyInfo(ct_result);

  if (devtools_proxy_) {
    devtools_proxy_->OnSignedExchangeReceived(
        envelope_, unverified_cert_chain_->cert(), &ssl_info);
  }

  response_head.ssl_info = std::move(ssl_info);
  std::move(headers_callback_)
      .Run(net::OK, envelope_->request_url(), envelope_->request_method(),
           response_head, std::move(mi_stream));
  state_ = State::kHeadersCallbackCalled;
}

}  // namespace content
