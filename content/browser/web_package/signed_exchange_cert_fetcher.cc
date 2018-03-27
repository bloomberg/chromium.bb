// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_cert_fetcher.h"

#include "base/numerics/safe_conversions.h"
#include "base/strings/string_piece.h"
#include "base/trace_event/trace_event.h"
#include "content/common/throttling_url_loader.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/url_loader_throttle.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace content {

namespace {

// Limit certificate messages to 100k, matching BoringSSL's default limit.
const size_t kMaxCertSizeForSignedExchange = 100 * 1024;
static size_t g_max_cert_size_for_signed_exchange =
    kMaxCertSizeForSignedExchange;

void ResetMaxCertSizeForTest() {
  g_max_cert_size_for_signed_exchange = kMaxCertSizeForSignedExchange;
}

const net::NetworkTrafficAnnotationTag kCertFetcherTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("sigined_exchange_cert_fetcher", R"(
    semantics {
      sender: "Certificate Fetcher for Signed HTTP Exchanges"
      description:
        "Retrieves the X.509v3 certificates to verify the signed headers of "
        "Signed HTTP Exchanges."
      trigger:
        "Navigating Chrome (ex: clicking on a link) to an URL and the server "
        "returns a Signed HTTP Exchange."
      data: "Arbitrary site-controlled data can be included in the URL."
      destination: WEBSITE
    }
    policy {
      cookies_allowed: NO
      setting:
        "This feature cannot be disabled by settings. This feature is not "
        "enabled by default yet."
      policy_exception_justification: "Not implemented."
    }
    comments:
      "Chrome would be unable to handle Signed HTTP Exchanges without this "
      "type of request."
    )");

bool ConsumeByte(base::StringPiece* data, uint8_t* out) {
  if (data->empty())
    return false;
  *out = (*data)[0];
  data->remove_prefix(1);
  return true;
}

bool Consume2Bytes(base::StringPiece* data, uint16_t* out) {
  if (data->size() < 2)
    return false;
  *out = (static_cast<uint8_t>((*data)[0]) << 8) |
         static_cast<uint8_t>((*data)[1]);
  data->remove_prefix(2);
  return true;
}

bool Consume3Bytes(base::StringPiece* data, uint32_t* out) {
  if (data->size() < 3)
    return false;
  *out = (static_cast<uint8_t>((*data)[0]) << 16) |
         (static_cast<uint8_t>((*data)[1]) << 8) |
         static_cast<uint8_t>((*data)[2]);
  data->remove_prefix(3);
  return true;
}

}  // namespace

// static
std::unique_ptr<SignedExchangeCertFetcher>
SignedExchangeCertFetcher::CreateAndStart(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
    const GURL& cert_url,
    url::Origin request_initiator,
    bool force_fetch,
    CertificateCallback callback) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeCertFetcher::CreateAndStart");
  std::unique_ptr<SignedExchangeCertFetcher> cert_fetcher(
      new SignedExchangeCertFetcher(
          std::move(shared_url_loader_factory), std::move(throttles), cert_url,
          std::move(request_initiator), force_fetch, std::move(callback)));
  cert_fetcher->Start();
  return cert_fetcher;
}

// static
base::Optional<std::vector<base::StringPiece>>
SignedExchangeCertFetcher::GetCertChainFromMessage(base::StringPiece message) {
  uint8_t cert_request_context_size = 0;
  if (!ConsumeByte(&message, &cert_request_context_size)) {
    DVLOG(1) << "Can't read certificate request request context size.";
    return base::nullopt;
  }
  if (cert_request_context_size != 0) {
    DVLOG(1) << "Invalid certificate request context size: "
             << static_cast<int>(cert_request_context_size);
    return base::nullopt;
  }
  uint32_t cert_list_size = 0;
  if (!Consume3Bytes(&message, &cert_list_size)) {
    DVLOG(1) << "Can't read certificate list size.";
    return base::nullopt;
  }

  if (cert_list_size != message.length()) {
    DVLOG(1) << "Certificate list size error: cert_list_size=" << cert_list_size
             << " remaining=" << message.length();
    return base::nullopt;
  }

  std::vector<base::StringPiece> certs;
  while (!message.empty()) {
    uint32_t cert_data_size = 0;
    if (!Consume3Bytes(&message, &cert_data_size)) {
      DVLOG(1) << "Can't read certificate data size.";
      return base::nullopt;
    }
    if (message.length() < cert_data_size) {
      DVLOG(1) << "Certificate data size error: cert_data_size="
               << cert_data_size << " remaining=" << message.length();
      return base::nullopt;
    }
    certs.emplace_back(message.substr(0, cert_data_size));
    message.remove_prefix(cert_data_size);

    uint16_t extensions_size = 0;
    if (!Consume2Bytes(&message, &extensions_size)) {
      DVLOG(1) << "Can't read extensions size.";
      return base::nullopt;
    }
    if (message.length() < extensions_size) {
      DVLOG(1) << "Extensions size error: extensions_size=" << extensions_size
               << " remaining=" << message.length();
      return base::nullopt;
    }
    message.remove_prefix(extensions_size);
  }
  return certs;
}

SignedExchangeCertFetcher::SignedExchangeCertFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
    const GURL& cert_url,
    url::Origin request_initiator,
    bool force_fetch,
    CertificateCallback callback)
    : shared_url_loader_factory_(std::move(shared_url_loader_factory)),
      throttles_(std::move(throttles)),
      resource_request_(std::make_unique<network::ResourceRequest>()),
      callback_(std::move(callback)) {
  // TODO(https://crbug.com/803774): Revisit more ResourceRequest flags.
  resource_request_->url = cert_url;
  resource_request_->request_initiator = std::move(request_initiator);
  resource_request_->resource_type = RESOURCE_TYPE_SUB_RESOURCE;
  // Cert requests should not send credential informartion, because the default
  // credentials mode of Fetch is "omit".
  resource_request_->load_flags = net::LOAD_DO_NOT_SEND_AUTH_DATA |
                                  net::LOAD_DO_NOT_SAVE_COOKIES |
                                  net::LOAD_DO_NOT_SEND_COOKIES;
  if (force_fetch) {
    resource_request_->load_flags |=
        net::LOAD_DISABLE_CACHE | net::LOAD_BYPASS_CACHE;
  }
  resource_request_->render_frame_id = MSG_ROUTING_NONE;
}

SignedExchangeCertFetcher::~SignedExchangeCertFetcher() = default;

void SignedExchangeCertFetcher::Start() {
  url_loader_ = ThrottlingURLLoader::CreateLoaderAndStart(
      std::move(shared_url_loader_factory_), std::move(throttles_),
      0 /* routing_id */, 0 /* request_id */,
      network::mojom::kURLLoadOptionNone, resource_request_.get(), this,
      kCertFetcherTrafficAnnotation, base::ThreadTaskRunnerHandle::Get());
}

void SignedExchangeCertFetcher::Abort() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeCertFetcher::Abort");
  DCHECK(callback_);
  url_loader_ = nullptr;
  body_.reset();
  handle_watcher_ = nullptr;
  body_string_.clear();
  std::move(callback_).Run(scoped_refptr<net::X509Certificate>());
}

void SignedExchangeCertFetcher::OnHandleReady(MojoResult result) {
  TRACE_EVENT_BEGIN0(TRACE_DISABLED_BY_DEFAULT("loading"),
                     "SignedExchangeCertFetcher::OnHandleReady");
  const void* buffer = nullptr;
  uint32_t num_bytes = 0;
  MojoResult rv =
      body_->BeginReadData(&buffer, &num_bytes, MOJO_READ_DATA_FLAG_NONE);
  if (rv == MOJO_RESULT_OK) {
    if (body_string_.size() + num_bytes > g_max_cert_size_for_signed_exchange) {
      body_->EndReadData(num_bytes);
      Abort();
      TRACE_EVENT_END1(TRACE_DISABLED_BY_DEFAULT("loading"),
                       "SignedExchangeCertFetcher::OnHandleReady", "error",
                       "The response body size exceeds the limit.");
      return;
    }
    body_string_.append(static_cast<const char*>(buffer), num_bytes);
    body_->EndReadData(num_bytes);
  } else if (rv == MOJO_RESULT_FAILED_PRECONDITION) {
    OnDataComplete();
  } else {
    DCHECK_EQ(MOJO_RESULT_SHOULD_WAIT, rv);
  }
  TRACE_EVENT_END0(TRACE_DISABLED_BY_DEFAULT("loading"),
                   "SignedExchangeCertFetcher::OnHandleReady");
}

void SignedExchangeCertFetcher::OnDataComplete() {
  TRACE_EVENT_BEGIN0(TRACE_DISABLED_BY_DEFAULT("loading"),
                     "SignedExchangeCertFetcher::OnDataComplete");
  DCHECK(callback_);
  url_loader_ = nullptr;
  body_.reset();
  handle_watcher_ = nullptr;
  base::Optional<std::vector<base::StringPiece>> der_certs =
      GetCertChainFromMessage(body_string_);
  if (!der_certs) {
    body_string_.clear();
    std::move(callback_).Run(scoped_refptr<net::X509Certificate>());
    TRACE_EVENT_END1(TRACE_DISABLED_BY_DEFAULT("loading"),
                     "SignedExchangeCertFetcher::OnDataComplete", "error",
                     "Failed to get certificate chain from message.");
    return;
  }
  scoped_refptr<net::X509Certificate> cert =
      net::X509Certificate::CreateFromDERCertChain(*der_certs);
  body_string_.clear();
  std::move(callback_).Run(std::move(cert));
  TRACE_EVENT_END0(TRACE_DISABLED_BY_DEFAULT("loading"),
                   "SignedExchangeCertFetcher::OnDataComplete");
}

// network::mojom::URLLoaderClient
void SignedExchangeCertFetcher::OnReceiveResponse(
    const network::ResourceResponseHead& head,
    network::mojom::DownloadedTempFilePtr downloaded_file) {
  TRACE_EVENT_BEGIN0(TRACE_DISABLED_BY_DEFAULT("loading"),
                     "SignedExchangeCertFetcher::OnReceiveResponse");
  if (head.headers->response_code() != net::HTTP_OK) {
    Abort();
    TRACE_EVENT_END2(TRACE_DISABLED_BY_DEFAULT("loading"),
                     "SignedExchangeCertFetcher::OnReceiveResponse", "error",
                     "Invalid reponse code.", "code",
                     head.headers->response_code());
    return;
  }
  if (head.content_length > 0) {
    if (base::checked_cast<size_t>(head.content_length) >
        g_max_cert_size_for_signed_exchange) {
      Abort();
      TRACE_EVENT_END2(TRACE_DISABLED_BY_DEFAULT("loading"),
                       "SignedExchangeCertFetcher::OnReceiveResponse", "error",
                       "Invalid content length.", "content_length",
                       head.content_length);
      return;
    }
    body_string_.reserve(head.content_length);
  }
  TRACE_EVENT_END0(TRACE_DISABLED_BY_DEFAULT("loading"),
                   "SignedExchangeCertFetcher::OnReceiveResponse");
}

void SignedExchangeCertFetcher::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& head) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeCertFetcher::OnReceiveRedirect");
  // Currently the cert fetcher doesn't allow any redirects.
  Abort();
}

void SignedExchangeCertFetcher::OnDataDownloaded(int64_t data_length,
                                                 int64_t encoded_length) {
  // Cert fetching is not a downloading job.
  NOTREACHED();
}

void SignedExchangeCertFetcher::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback callback) {
  // Cert fetching doesn't have request body.
  NOTREACHED();
}

void SignedExchangeCertFetcher::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {
  // Cert fetching doesn't use cached metadata.
  NOTREACHED();
}

void SignedExchangeCertFetcher::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  // Do nothing.
}

void SignedExchangeCertFetcher::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeCertFetcher::OnStartLoadingResponseBody");
  body_ = std::move(body);
  handle_watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC,
      base::SequencedTaskRunnerHandle::Get());
  handle_watcher_->Watch(
      body_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      base::BindRepeating(&SignedExchangeCertFetcher::OnHandleReady,
                          base::Unretained(this)));
}

void SignedExchangeCertFetcher::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeCertFetcher::OnComplete");
  if (!handle_watcher_)
    Abort();
}

// static
base::ScopedClosureRunner SignedExchangeCertFetcher::SetMaxCertSizeForTest(
    size_t max_cert_size) {
  g_max_cert_size_for_signed_exchange = max_cert_size;
  return base::ScopedClosureRunner(base::BindOnce(&ResetMaxCertSizeForTest));
}

}  // namespace content
