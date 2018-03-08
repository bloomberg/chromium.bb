// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/web_url_loader_impl.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/child/child_thread_impl.h"
#include "content/child/scoped_child_process_reference.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_features.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/referrer.h"
#include "content/public/renderer/fixed_received_data.h"
#include "content/public/renderer/request_peer.h"
#include "content/renderer/loader/ftp_directory_listing_response_delegate.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/loader/resource_dispatcher.h"
#include "content/renderer/loader/shared_memory_data_consumer_handle.h"
#include "content/renderer/loader/sync_load_response.h"
#include "content/renderer/loader/web_url_request_util.h"
#include "content/renderer/loader/weburlresponse_extradata_impl.h"
#include "net/base/data_url.h"
#include "net/base/filename_util.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/ct_sct_to_string.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/url_request/url_request_data_job.h"
#include "services/network/loader_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/request_context_frame_type.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/WebKit/public/common/mime_util/mime_util.h"
#include "third_party/WebKit/public/platform/FilePathConversion.h"
#include "third_party/WebKit/public/platform/WebHTTPLoadInfo.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebSecurityStyle.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLLoadTiming.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

using base::Time;
using base::TimeTicks;
using blink::WebData;
using blink::WebHTTPBody;
using blink::WebHTTPHeaderVisitor;
using blink::WebHTTPLoadInfo;
using blink::WebReferrerPolicy;
using blink::WebSecurityPolicy;
using blink::WebString;
using blink::WebURL;
using blink::WebURLError;
using blink::WebURLLoadTiming;
using blink::WebURLLoader;
using blink::WebURLLoaderClient;
using blink::WebURLRequest;
using blink::WebURLResponse;

namespace content {

// Utilities ------------------------------------------------------------------

namespace {

constexpr char kStylesheetAcceptHeader[] = "text/css,*/*;q=0.1";
constexpr char kImageAcceptHeader[] = "image/webp,image/apng,image/*,*/*;q=0.8";

using HeadersVector = network::HttpRawRequestResponseInfo::HeadersVector;

class KeepAliveHandleWithChildProcessReference {
 public:
  explicit KeepAliveHandleWithChildProcessReference(
      mojom::KeepAliveHandlePtr ptr)
      : keep_alive_handle_(std::move(ptr)) {}
  ~KeepAliveHandleWithChildProcessReference() {}

 private:
  mojom::KeepAliveHandlePtr keep_alive_handle_;
  ScopedChildProcessReference reference_;
};

// TODO(estark): Figure out a way for the embedder to provide the
// security style for a resource. Ideally, the logic for assigning
// per-resource security styles should live in the same place as the
// logic for assigning per-page security styles (which lives in the
// embedder). It would also be nice for the embedder to have the chance
// to control the per-resource security style beyond the simple logic
// here. (For example, the embedder might want to mark certain resources
// differently if they use SHA1 signatures.) https://crbug.com/648326
blink::WebSecurityStyle GetSecurityStyleForResource(
    const GURL& url,
    net::CertStatus cert_status) {
  if (!url.SchemeIsCryptographic())
    return blink::kWebSecurityStyleNeutral;

  // Minor errors don't lower the security style to
  // WebSecurityStyleAuthenticationBroken.
  if (net::IsCertStatusError(cert_status) &&
      !net::IsCertStatusMinorError(cert_status)) {
    return blink::kWebSecurityStyleInsecure;
  }

  return blink::kWebSecurityStyleSecure;
}

// Converts timing data from |load_timing| to the format used by WebKit.
void PopulateURLLoadTiming(const net::LoadTimingInfo& load_timing,
                           WebURLLoadTiming* url_timing) {
  DCHECK(!load_timing.request_start.is_null());

  const TimeTicks kNullTicks;
  url_timing->Initialize();
  url_timing->SetRequestTime(
      (load_timing.request_start - kNullTicks).InSecondsF());
  url_timing->SetProxyStart(
      (load_timing.proxy_resolve_start - kNullTicks).InSecondsF());
  url_timing->SetProxyEnd(
      (load_timing.proxy_resolve_end - kNullTicks).InSecondsF());
  url_timing->SetDNSStart(
      (load_timing.connect_timing.dns_start - kNullTicks).InSecondsF());
  url_timing->SetDNSEnd(
      (load_timing.connect_timing.dns_end - kNullTicks).InSecondsF());
  url_timing->SetConnectStart(
      (load_timing.connect_timing.connect_start - kNullTicks).InSecondsF());
  url_timing->SetConnectEnd(
      (load_timing.connect_timing.connect_end - kNullTicks).InSecondsF());
  url_timing->SetSSLStart(
      (load_timing.connect_timing.ssl_start - kNullTicks).InSecondsF());
  url_timing->SetSSLEnd(
      (load_timing.connect_timing.ssl_end - kNullTicks).InSecondsF());
  url_timing->SetSendStart((load_timing.send_start - kNullTicks).InSecondsF());
  url_timing->SetSendEnd((load_timing.send_end - kNullTicks).InSecondsF());
  url_timing->SetReceiveHeadersEnd(
      (load_timing.receive_headers_end - kNullTicks).InSecondsF());
  url_timing->SetPushStart((load_timing.push_start - kNullTicks).InSecondsF());
  url_timing->SetPushEnd((load_timing.push_end - kNullTicks).InSecondsF());
}

net::RequestPriority ConvertWebKitPriorityToNetPriority(
    const WebURLRequest::Priority& priority) {
  switch (priority) {
    case WebURLRequest::Priority::kVeryHigh:
      return net::HIGHEST;

    case WebURLRequest::Priority::kHigh:
      return net::MEDIUM;

    case WebURLRequest::Priority::kMedium:
      return net::LOW;

    case WebURLRequest::Priority::kLow:
      return net::LOWEST;

    case WebURLRequest::Priority::kVeryLow:
      return net::IDLE;

    case WebURLRequest::Priority::kUnresolved:
    default:
      NOTREACHED();
      return net::LOW;
  }
}

// Extracts info from a data scheme URL |url| into |info| and |data|. Returns
// net::OK if successful. Returns a net error code otherwise.
int GetInfoFromDataURL(const GURL& url,
                       network::ResourceResponseInfo* info,
                       std::string* data) {
  // Assure same time for all time fields of data: URLs.
  Time now = Time::Now();
  info->load_timing.request_start = TimeTicks::Now();
  info->load_timing.request_start_time = now;
  info->request_time = now;
  info->response_time = now;

  std::string mime_type;
  std::string charset;
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(std::string()));
  int result = net::URLRequestDataJob::BuildResponse(
      url, &mime_type, &charset, data, headers.get());
  if (result != net::OK)
    return result;

  info->headers = headers;
  info->mime_type.swap(mime_type);
  info->charset.swap(charset);
  info->content_length = data->length();
  info->encoded_data_length = 0;
  info->encoded_body_length = 0;
  info->previews_state = PREVIEWS_OFF;

  return net::OK;
}

// Convert a net::SignedCertificateTimestampAndStatus object to a
// blink::WebURLResponse::SignedCertificateTimestamp object.
blink::WebURLResponse::SignedCertificateTimestamp NetSCTToBlinkSCT(
    const net::SignedCertificateTimestampAndStatus& sct_and_status) {
  return blink::WebURLResponse::SignedCertificateTimestamp(
      WebString::FromASCII(net::ct::StatusToString(sct_and_status.status)),
      WebString::FromASCII(net::ct::OriginToString(sct_and_status.sct->origin)),
      WebString::FromUTF8(sct_and_status.sct->log_description),
      WebString::FromASCII(
          base::HexEncode(sct_and_status.sct->log_id.c_str(),
                          sct_and_status.sct->log_id.length())),
      sct_and_status.sct->timestamp.ToJavaTime(),
      WebString::FromASCII(net::ct::HashAlgorithmToString(
          sct_and_status.sct->signature.hash_algorithm)),
      WebString::FromASCII(net::ct::SignatureAlgorithmToString(
          sct_and_status.sct->signature.signature_algorithm)),
      WebString::FromASCII(base::HexEncode(
          sct_and_status.sct->signature.signature_data.c_str(),
          sct_and_status.sct->signature.signature_data.length())));
}

void SetSecurityStyleAndDetails(const GURL& url,
                                const network::ResourceResponseInfo& info,
                                WebURLResponse* response,
                                bool report_security_info) {
  if (!report_security_info) {
    response->SetSecurityStyle(blink::kWebSecurityStyleUnknown);
    return;
  }
  if (!url.SchemeIsCryptographic()) {
    response->SetSecurityStyle(blink::kWebSecurityStyleNeutral);
    return;
  }

  // The resource loader does not provide a guarantee that requests always have
  // security info (such as a certificate) attached. Use WebSecurityStyleUnknown
  // in this case where there isn't enough information to be useful.
  if (info.certificate.empty()) {
    response->SetSecurityStyle(blink::kWebSecurityStyleUnknown);
    return;
  }

  int ssl_version =
      net::SSLConnectionStatusToVersion(info.ssl_connection_status);
  const char* protocol;
  net::SSLVersionToString(&protocol, ssl_version);

  const char* key_exchange;
  const char* cipher;
  const char* mac;
  bool is_aead;
  bool is_tls13;
  uint16_t cipher_suite =
      net::SSLConnectionStatusToCipherSuite(info.ssl_connection_status);
  net::SSLCipherSuiteToStrings(&key_exchange, &cipher, &mac, &is_aead,
                               &is_tls13, cipher_suite);
  if (key_exchange == nullptr) {
    DCHECK(is_tls13);
    key_exchange = "";
  }

  if (mac == nullptr) {
    DCHECK(is_aead);
    mac = "";
  }

  const char* key_exchange_group = "";
  if (info.ssl_key_exchange_group != 0) {
    // Historically the field was named 'curve' rather than 'group'.
    key_exchange_group = SSL_get_curve_name(info.ssl_key_exchange_group);
    if (!key_exchange_group) {
      NOTREACHED();
      key_exchange_group = "";
    }
  }

  response->SetSecurityStyle(
      GetSecurityStyleForResource(url, info.cert_status));

  blink::WebURLResponse::SignedCertificateTimestampList sct_list(
      info.signed_certificate_timestamps.size());

  for (size_t i = 0; i < sct_list.size(); ++i)
    sct_list[i] = NetSCTToBlinkSCT(info.signed_certificate_timestamps[i]);

  scoped_refptr<net::X509Certificate> cert(
      net::X509Certificate::CreateFromBytes(info.certificate[0].data(),
                                            info.certificate[0].size()));
  if (!cert) {
    NOTREACHED();
    response->SetSecurityStyle(blink::kWebSecurityStyleUnknown);
    return;
  }

  std::vector<std::string> san_dns;
  std::vector<std::string> san_ip;
  cert->GetSubjectAltName(&san_dns, &san_ip);
  blink::WebVector<blink::WebString> web_san(san_dns.size() + san_ip.size());
  std::transform(
      san_dns.begin(), san_dns.end(), web_san.begin(),
      [](const std::string& h) { return blink::WebString::FromLatin1(h); });
  std::transform(san_ip.begin(), san_ip.end(), web_san.begin() + san_dns.size(),
                 [](const std::string& h) {
                   net::IPAddress ip(reinterpret_cast<const uint8_t*>(h.data()),
                                     h.size());
                   return blink::WebString::FromLatin1(ip.ToString());
                 });

  blink::WebVector<blink::WebString> web_cert(info.certificate.size());
  std::transform(
      info.certificate.begin(), info.certificate.end(), web_cert.begin(),
      [](const std::string& h) { return blink::WebString::FromLatin1(h); });

  blink::WebURLResponse::WebSecurityDetails webSecurityDetails(
      WebString::FromASCII(protocol), WebString::FromASCII(key_exchange),
      WebString::FromASCII(key_exchange_group), WebString::FromASCII(cipher),
      WebString::FromASCII(mac),
      WebString::FromUTF8(cert->subject().common_name), web_san,
      WebString::FromUTF8(cert->issuer().common_name),
      cert->valid_start().ToDoubleT(), cert->valid_expiry().ToDoubleT(),
      web_cert, sct_list);

  response->SetSecurityDetails(webSecurityDetails);
}

}  // namespace

StreamOverrideParameters::StreamOverrideParameters() {}
StreamOverrideParameters::~StreamOverrideParameters() {
  if (on_delete)
    std::move(on_delete).Run(stream_url);
}

WebURLLoaderFactoryImpl::WebURLLoaderFactoryImpl(
    base::WeakPtr<ResourceDispatcher> resource_dispatcher,
    scoped_refptr<SharedURLLoaderFactory> loader_factory)
    : resource_dispatcher_(std::move(resource_dispatcher)),
      loader_factory_(std::move(loader_factory)) {}

WebURLLoaderFactoryImpl::~WebURLLoaderFactoryImpl() = default;

std::unique_ptr<WebURLLoaderFactoryImpl>
WebURLLoaderFactoryImpl::CreateTestOnlyFactory() {
  return std::make_unique<WebURLLoaderFactoryImpl>(nullptr, nullptr);
}

std::unique_ptr<blink::WebURLLoader> WebURLLoaderFactoryImpl::CreateURLLoader(
    const blink::WebURLRequest& request,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (!loader_factory_) {
    // In some tests like RenderViewTests loader_factory_ is not available.
    // These tests can still use data URLs to bypass the ResourceDispatcher.
    if (!task_runner)
      task_runner = base::ThreadTaskRunnerHandle::Get();
    return std::make_unique<WebURLLoaderImpl>(resource_dispatcher_.get(),
                                              std::move(task_runner),
                                              nullptr /* factory */);
  }

  DCHECK(task_runner);
  DCHECK(resource_dispatcher_);
  return std::make_unique<WebURLLoaderImpl>(
      resource_dispatcher_.get(), std::move(task_runner), loader_factory_);
}

// This inner class exists since the WebURLLoader may be deleted while inside a
// call to WebURLLoaderClient.  Refcounting is to keep the context from being
// deleted if it may have work to do after calling into the client.
class WebURLLoaderImpl::Context : public base::RefCounted<Context> {
 public:
  using ReceivedData = RequestPeer::ReceivedData;

  Context(WebURLLoaderImpl* loader,
          ResourceDispatcher* resource_dispatcher,
          scoped_refptr<base::SingleThreadTaskRunner> task_runner,
          scoped_refptr<SharedURLLoaderFactory> factory,
          mojom::KeepAliveHandlePtr keep_alive_handle);

  ResourceDispatcher* resource_dispatcher() { return resource_dispatcher_; }
  int request_id() const { return request_id_; }
  WebURLLoaderClient* client() const { return client_; }
  void set_client(WebURLLoaderClient* client) { client_ = client; }
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() {
    return task_runner_;
  }

  void Cancel();
  void SetDefersLoading(bool value);
  void DidChangePriority(WebURLRequest::Priority new_priority,
                         int intra_priority_value);
  void Start(const WebURLRequest& request,
             SyncLoadResponse* sync_load_response);

  void OnUploadProgress(uint64_t position, uint64_t size);
  bool OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                          const network::ResourceResponseInfo& info);
  void OnReceivedResponse(const network::ResourceResponseInfo& info);
  void OnDownloadedData(int len, int encoded_data_length);
  void OnReceivedData(std::unique_ptr<ReceivedData> data);
  void OnTransferSizeUpdated(int transfer_size_diff);
  void OnReceivedCachedMetadata(const char* data, int len);
  void OnCompletedRequest(const network::URLLoaderCompletionStatus& status);

 private:
  friend class base::RefCounted<Context>;
  ~Context();

  // Called when the body data stream is detached from the reader side.
  void CancelBodyStreaming();
  // We can optimize the handling of data URLs in most cases.
  bool CanHandleDataURLRequestLocally(const WebURLRequest& request) const;
  void HandleDataURL();

  static net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag(
      const blink::WebURLRequest& request);

  WebURLLoaderImpl* loader_;

  WebURL url_;
  bool use_stream_on_response_;
  // Controls SetSecurityStyleAndDetails() in PopulateURLResponse(). Initially
  // set to WebURLRequest::ReportRawHeaders() in Start() and gets updated in
  // WillFollowRedirect() (by the InspectorNetworkAgent) while the new
  // ReportRawHeaders() value won't be propagated to the browser process.
  //
  // TODO(tyoshino): Investigate whether it's worth propagating the new value.
  bool report_raw_headers_;

  WebURLLoaderClient* client_;
  ResourceDispatcher* resource_dispatcher_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<FtpDirectoryListingResponseDelegate> ftp_listing_delegate_;
  std::unique_ptr<StreamOverrideParameters> stream_override_;
  std::unique_ptr<SharedMemoryDataConsumerHandle::Writer> body_stream_writer_;
  std::unique_ptr<KeepAliveHandleWithChildProcessReference> keep_alive_handle_;
  enum DeferState {NOT_DEFERRING, SHOULD_DEFER, DEFERRED_DATA};
  DeferState defers_loading_;
  int request_id_;

  scoped_refptr<SharedURLLoaderFactory> url_loader_factory_;
};

// A thin wrapper class for Context to ensure its lifetime while it is
// handling IPC messages coming from ResourceDispatcher. Owns one ref to
// Context and held by ResourceDispatcher.
class WebURLLoaderImpl::RequestPeerImpl : public RequestPeer {
 public:
  // If |discard_body| is false this doesn't propagate the received data
  // to the context.
  explicit RequestPeerImpl(Context* context, bool discard_body = false);

  // RequestPeer methods:
  void OnUploadProgress(uint64_t position, uint64_t size) override;
  bool OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                          const network::ResourceResponseInfo& info) override;
  void OnReceivedResponse(const network::ResourceResponseInfo& info) override;
  void OnDownloadedData(int len, int encoded_data_length) override;
  void OnReceivedData(std::unique_ptr<ReceivedData> data) override;
  void OnTransferSizeUpdated(int transfer_size_diff) override;
  void OnReceivedCachedMetadata(const char* data, int len) override;
  void OnCompletedRequest(
      const network::URLLoaderCompletionStatus& status) override;

 private:
  scoped_refptr<Context> context_;
  const bool discard_body_;
  DISALLOW_COPY_AND_ASSIGN(RequestPeerImpl);
};

// A sink peer that doesn't forward the data.
class WebURLLoaderImpl::SinkPeer : public RequestPeer {
 public:
  explicit SinkPeer(Context* context) : context_(context) {}

  // RequestPeer implementation:
  void OnUploadProgress(uint64_t position, uint64_t size) override {}
  bool OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                          const network::ResourceResponseInfo& info) override {
    return true;
  }
  void OnReceivedResponse(const network::ResourceResponseInfo& info) override {}
  void OnDownloadedData(int len, int encoded_data_length) override {}
  void OnReceivedData(std::unique_ptr<ReceivedData> data) override {}
  void OnTransferSizeUpdated(int transfer_size_diff) override {}
  void OnReceivedCachedMetadata(const char* data, int len) override {}
  void OnCompletedRequest(
      const network::URLLoaderCompletionStatus& status) override {
    context_->resource_dispatcher()->Cancel(context_->request_id(),
                                            context_->task_runner());
  }

 private:
  scoped_refptr<Context> context_;
  DISALLOW_COPY_AND_ASSIGN(SinkPeer);
};

// WebURLLoaderImpl::Context --------------------------------------------------

WebURLLoaderImpl::Context::Context(
    WebURLLoaderImpl* loader,
    ResourceDispatcher* resource_dispatcher,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_refptr<SharedURLLoaderFactory> url_loader_factory,
    mojom::KeepAliveHandlePtr keep_alive_handle_ptr)
    : loader_(loader),
      use_stream_on_response_(false),
      report_raw_headers_(false),
      client_(nullptr),
      resource_dispatcher_(resource_dispatcher),
      task_runner_(std::move(task_runner)),
      keep_alive_handle_(
          keep_alive_handle_ptr
              ? std::make_unique<KeepAliveHandleWithChildProcessReference>(
                    std::move(keep_alive_handle_ptr))
              : nullptr),
      defers_loading_(NOT_DEFERRING),
      request_id_(-1),
      url_loader_factory_(std::move(url_loader_factory)) {
  DCHECK(url_loader_factory_ || !resource_dispatcher);
}

void WebURLLoaderImpl::Context::Cancel() {
  TRACE_EVENT_WITH_FLOW0("loading", "WebURLLoaderImpl::Context::Cancel", this,
                         TRACE_EVENT_FLAG_FLOW_IN);
  if (resource_dispatcher_ && // NULL in unittest.
      request_id_ != -1) {
    resource_dispatcher_->Cancel(request_id_, task_runner_);
    request_id_ = -1;
  }

  if (body_stream_writer_)
    body_stream_writer_->Fail();

  // Ensure that we do not notify the delegate anymore as it has
  // its own pointer to the client.
  if (ftp_listing_delegate_)
    ftp_listing_delegate_->Cancel();

  // Do not make any further calls to the client.
  client_ = nullptr;
  loader_ = nullptr;
}

void WebURLLoaderImpl::Context::SetDefersLoading(bool value) {
  if (request_id_ != -1)
    resource_dispatcher_->SetDefersLoading(request_id_, value);
  if (value && defers_loading_ == NOT_DEFERRING) {
    defers_loading_ = SHOULD_DEFER;
  } else if (!value && defers_loading_ != NOT_DEFERRING) {
    if (defers_loading_ == DEFERRED_DATA) {
      task_runner_->PostTask(FROM_HERE,
                             base::BindOnce(&Context::HandleDataURL, this));
    }
    defers_loading_ = NOT_DEFERRING;
  }
}

void WebURLLoaderImpl::Context::DidChangePriority(
    WebURLRequest::Priority new_priority, int intra_priority_value) {
  if (request_id_ != -1) {
    resource_dispatcher_->DidChangePriority(
        request_id_,
        ConvertWebKitPriorityToNetPriority(new_priority),
        intra_priority_value);
  }
}

void WebURLLoaderImpl::Context::Start(const WebURLRequest& request,
                                      SyncLoadResponse* sync_load_response) {
  DCHECK(request_id_ == -1);

  url_ = request.Url();
  use_stream_on_response_ = request.UseStreamOnResponse();
  report_raw_headers_ = request.ReportRawHeaders();

  if (CanHandleDataURLRequestLocally(request)) {
    if (sync_load_response) {
      // This is a sync load. Do the work now.
      sync_load_response->url = url_;
      sync_load_response->error_code =
          GetInfoFromDataURL(sync_load_response->url, sync_load_response,
                             &sync_load_response->data);
    } else {
      task_runner_->PostTask(FROM_HERE,
                             base::BindOnce(&Context::HandleDataURL, this));
    }
    return;
  }

  if (request.GetExtraData()) {
    RequestExtraData* extra_data =
        static_cast<RequestExtraData*>(request.GetExtraData());
    stream_override_ = extra_data->TakeStreamOverrideOwnership();
  }


  // PlzNavigate: outside of tests, the only navigation requests going through
  // the WebURLLoader are the ones created by CommitNavigation. Several browser
  // tests load HTML directly through a data url which will be handled by the
  // block above.
  DCHECK(!IsBrowserSideNavigationEnabled() || stream_override_ ||
         request.GetFrameType() ==
             network::mojom::RequestContextFrameType::kNone);

  GURL referrer_url(
      request.HttpHeaderField(WebString::FromASCII("Referer")).Latin1());
  const std::string& method = request.HttpMethod().Latin1();

  // TODO(brettw) this should take parameter encoding into account when
  // creating the GURLs.

  // TODO(horo): Check credentials flag is unset when credentials mode is omit.
  //             Check credentials flag is set when credentials mode is include.

  std::unique_ptr<network::ResourceRequest> resource_request(
      new network::ResourceRequest);

  resource_request->method = method;
  resource_request->url = url_;
  resource_request->site_for_cookies = request.SiteForCookies();
  resource_request->request_initiator =
      request.RequestorOrigin().IsNull()
          ? base::Optional<url::Origin>()
          : base::Optional<url::Origin>(request.RequestorOrigin());
  resource_request->referrer = referrer_url;

  resource_request->referrer_policy =
      Referrer::ReferrerPolicyForUrlRequest(request.GetReferrerPolicy());
  resource_request->resource_type = WebURLRequestToResourceType(request);

  resource_request->headers = GetWebURLRequestHeaders(request);
  if (resource_request->resource_type == RESOURCE_TYPE_STYLESHEET) {
    resource_request->headers.SetHeader(network::kAcceptHeader,
                                        kStylesheetAcceptHeader);
  } else if (resource_request->resource_type == RESOURCE_TYPE_FAVICON ||
             resource_request->resource_type == RESOURCE_TYPE_IMAGE) {
    resource_request->headers.SetHeader(network::kAcceptHeader,
                                        kImageAcceptHeader);
  } else {
    // Calling SetHeaderIfMissing() instead of SetHeader() because JS can
    // manually set an accept header on an XHR.
    resource_request->headers.SetHeaderIfMissing(network::kAcceptHeader,
                                                 network::kDefaultAcceptHeader);
  }

  resource_request->load_flags = GetLoadFlagsForWebURLRequest(request);

  // |plugin_child_id| only needs to be non-zero if the request originates
  // outside the render process, so we can use requestorProcessID even
  // for requests from in-process plugins.
  resource_request->plugin_child_id = request.GetPluginChildID();
  resource_request->priority =
      ConvertWebKitPriorityToNetPriority(request.GetPriority());
  resource_request->appcache_host_id = request.AppCacheHostID();
  resource_request->should_reset_appcache = request.ShouldResetAppCache();
  resource_request->is_external_request = request.IsExternalRequest();
  resource_request->cors_preflight_policy = request.GetCORSPreflightPolicy();
  resource_request->skip_service_worker = request.GetSkipServiceWorker();
  resource_request->fetch_request_mode = request.GetFetchRequestMode();
  resource_request->fetch_credentials_mode = request.GetFetchCredentialsMode();
  resource_request->fetch_redirect_mode = request.GetFetchRedirectMode();
  resource_request->fetch_integrity =
      GetFetchIntegrityForWebURLRequest(request);
  resource_request->fetch_request_context_type =
      GetRequestContextTypeForWebURLRequest(request);

  resource_request->fetch_frame_type = request.GetFrameType();
  resource_request->request_body =
      GetRequestBodyForWebURLRequest(request).get();
  resource_request->download_to_file = request.DownloadToFile();
  resource_request->keepalive = request.GetKeepalive();
  resource_request->has_user_gesture = request.HasUserGesture();
  resource_request->enable_load_timing = true;
  resource_request->enable_upload_progress = request.ReportUploadProgress();
  GURL gurl(url_);
  if (request.GetRequestContext() ==
          WebURLRequest::kRequestContextXMLHttpRequest &&
      (gurl.has_username() || gurl.has_password())) {
    resource_request->do_not_prompt_for_login = true;
  }
  resource_request->report_raw_headers = request.ReportRawHeaders();
  resource_request->previews_state =
      static_cast<int>(request.GetPreviewsState());

  // PlzNavigate: The network request has already been made by the browser.
  // The renderer should request a stream which contains the body of the
  // response. If the Network Service or NavigationMojoResponse is enabled, the
  // URLLoaderClientEndpoints is used instead to get the body.
  network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints;
  if (stream_override_) {
    CHECK(IsBrowserSideNavigationEnabled());
    DCHECK(!sync_load_response);
    DCHECK_NE(network::mojom::RequestContextFrameType::kNone,
              request.GetFrameType());
    if (stream_override_->url_loader_client_endpoints) {
      url_loader_client_endpoints =
          std::move(stream_override_->url_loader_client_endpoints);
    } else {
      resource_request->resource_body_stream_url = stream_override_->stream_url;
    }
  }

  RequestExtraData empty_extra_data;
  RequestExtraData* extra_data;
  if (request.GetExtraData())
    extra_data = static_cast<RequestExtraData*>(request.GetExtraData());
  else
    extra_data = &empty_extra_data;
  extra_data->CopyToResourceRequest(resource_request.get());

  if (sync_load_response) {
    DCHECK(defers_loading_ == NOT_DEFERRING);

    resource_dispatcher_->StartSync(
        std::move(resource_request), request.RequestorID(),
        extra_data->frame_origin(), GetTrafficAnnotationTag(request),
        sync_load_response, url_loader_factory_,
        extra_data->TakeURLLoaderThrottles());
    return;
  }

  std::unique_ptr<RequestPeer> peer;
  if (extra_data->download_to_network_cache_only()) {
    peer = std::make_unique<SinkPeer>(this);
  } else {
    const bool discard_body =
        (resource_request->resource_type == RESOURCE_TYPE_PREFETCH);
    peer =
        std::make_unique<WebURLLoaderImpl::RequestPeerImpl>(this, discard_body);
  }

  TRACE_EVENT_WITH_FLOW0("loading", "WebURLLoaderImpl::Context::Start", this,
                         TRACE_EVENT_FLAG_FLOW_OUT);
  base::OnceClosure continue_navigation_function;
  request_id_ = resource_dispatcher_->StartAsync(
      std::move(resource_request), request.RequestorID(), task_runner_,
      extra_data->frame_origin(), GetTrafficAnnotationTag(request),
      false /* is_sync */, std::move(peer), url_loader_factory_,
      extra_data->TakeURLLoaderThrottles(),
      std::move(url_loader_client_endpoints), &continue_navigation_function);
  extra_data->set_continue_navigation_function(
      std::move(continue_navigation_function));

  if (defers_loading_ != NOT_DEFERRING)
    resource_dispatcher_->SetDefersLoading(request_id_, true);
}

void WebURLLoaderImpl::Context::OnUploadProgress(uint64_t position,
                                                 uint64_t size) {
  if (client_)
    client_->DidSendData(position, size);
}

bool WebURLLoaderImpl::Context::OnReceivedRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseInfo& info) {
  if (!client_)
    return false;

  TRACE_EVENT_WITH_FLOW0(
      "loading", "WebURLLoaderImpl::Context::OnReceivedRedirect",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  WebURLResponse response;
  PopulateURLResponse(url_, info, &response, report_raw_headers_);

  url_ = WebURL(redirect_info.new_url);
  return client_->WillFollowRedirect(
      url_, redirect_info.new_site_for_cookies,
      WebString::FromUTF8(redirect_info.new_referrer),
      Referrer::NetReferrerPolicyToBlinkReferrerPolicy(
          redirect_info.new_referrer_policy),
      WebString::FromUTF8(redirect_info.new_method), response,
      report_raw_headers_);
}

void WebURLLoaderImpl::Context::OnReceivedResponse(
    const network::ResourceResponseInfo& initial_info) {
  if (!client_)
    return;

  TRACE_EVENT_WITH_FLOW0(
      "loading", "WebURLLoaderImpl::Context::OnReceivedResponse",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  network::ResourceResponseInfo info = initial_info;

  // PlzNavigate: during navigations, the ResourceResponse has already been
  // received on the browser side, and has been passed down to the renderer.
  if (stream_override_) {
    CHECK(IsBrowserSideNavigationEnabled());
    info = stream_override_->response;

    // Replay the redirects that happened during navigation.
    DCHECK_EQ(stream_override_->redirect_responses.size(),
              stream_override_->redirect_infos.size());
    for (size_t i = 0; i < stream_override_->redirect_responses.size(); ++i) {
      bool result = OnReceivedRedirect(stream_override_->redirect_infos[i],
                                       stream_override_->redirect_responses[i]);
      if (!result)
        return;
    }
  }

  WebURLResponse response;
  PopulateURLResponse(url_, info, &response, report_raw_headers_);

  bool show_raw_listing = false;
  if (info.mime_type == "text/vnd.chromium.ftp-dir") {
    if (GURL(url_).query_piece() == "raw") {
      // Set the MIME type to plain text to prevent any active content.
      response.SetMIMEType("text/plain");
      show_raw_listing = true;
    } else {
      // We're going to produce a parsed listing in HTML.
      response.SetMIMEType("text/html");
    }
  }
  if (info.headers.get() && info.mime_type == "multipart/x-mixed-replace") {
    std::string content_type;
    info.headers->EnumerateHeader(nullptr, "content-type", &content_type);

    std::string mime_type;
    std::string charset;
    bool had_charset = false;
    std::string boundary;
    net::HttpUtil::ParseContentType(content_type, &mime_type, &charset,
                                    &had_charset, &boundary);
    base::TrimString(boundary, " \"", &boundary);
    response.SetMultipartBoundary(boundary.data(), boundary.size());
  }

  if (use_stream_on_response_) {
    SharedMemoryDataConsumerHandle::BackpressureMode mode =
        SharedMemoryDataConsumerHandle::kDoNotApplyBackpressure;
    if (info.headers &&
        info.headers->HasHeaderValue("Cache-Control", "no-store")) {
      mode = SharedMemoryDataConsumerHandle::kApplyBackpressure;
    }

    auto read_handle = std::make_unique<SharedMemoryDataConsumerHandle>(
        mode, base::Bind(&Context::CancelBodyStreaming, this),
        &body_stream_writer_);

    // Here |body_stream_writer_| has an indirect reference to |this| and that
    // creates a reference cycle, but it is not a problem because the cycle
    // will break if one of the following happens:
    //  1) The body data transfer is done (with or without an error).
    //  2) |read_handle| (and its reader) is detached.
    client_->DidReceiveResponse(response, std::move(read_handle));
    // TODO(yhirano): Support ftp listening and multipart
    return;
  }

  client_->DidReceiveResponse(response);

  // DidReceiveResponse() may have triggered a cancel, causing the |client_| to
  // go away.
  if (!client_)
    return;

  DCHECK(!ftp_listing_delegate_);
  if (info.mime_type == "text/vnd.chromium.ftp-dir" && !show_raw_listing) {
    ftp_listing_delegate_ =
        std::make_unique<FtpDirectoryListingResponseDelegate>(client_, loader_,
                                                              response);
  }
}

void WebURLLoaderImpl::Context::OnDownloadedData(int len,
                                                 int encoded_data_length) {
  if (client_)
    client_->DidDownloadData(len, encoded_data_length);
}

void WebURLLoaderImpl::Context::OnReceivedData(
    std::unique_ptr<ReceivedData> data) {
  const char* payload = data->payload();
  int data_length = data->length();
  if (!client_)
    return;

  TRACE_EVENT_WITH_FLOW0(
      "loading", "WebURLLoaderImpl::Context::OnReceivedData",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  if (ftp_listing_delegate_) {
    // The FTP listing delegate will make the appropriate calls to
    // client_->didReceiveData and client_->didReceiveResponse.
    ftp_listing_delegate_->OnReceivedData(payload, data_length);
    return;
  }

  // We dispatch the data even when |useStreamOnResponse()| is set, in order
  // to make Devtools work.
  client_->DidReceiveData(payload, data_length);

  if (use_stream_on_response_) {
    // We don't support |ftp_listing_delegate_| for now.
    // TODO(yhirano): Support ftp listening.
    body_stream_writer_->AddData(std::move(data));
  }
}

void WebURLLoaderImpl::Context::OnTransferSizeUpdated(int transfer_size_diff) {
  client_->DidReceiveTransferSizeUpdate(transfer_size_diff);
}

void WebURLLoaderImpl::Context::OnReceivedCachedMetadata(
    const char* data, int len) {
  if (!client_)
    return;
  TRACE_EVENT_WITH_FLOW0(
      "loading", "WebURLLoaderImpl::Context::OnReceivedCachedMetadata",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  client_->DidReceiveCachedMetadata(data, len);
}

void WebURLLoaderImpl::Context::OnCompletedRequest(
    const network::URLLoaderCompletionStatus& status) {
  int64_t total_transfer_size = status.encoded_data_length;
  int64_t encoded_body_size = status.encoded_body_length;

  if (ftp_listing_delegate_) {
    ftp_listing_delegate_->OnCompletedRequest();
    ftp_listing_delegate_.reset(nullptr);
  }

  if (body_stream_writer_ && status.error_code != net::OK)
    body_stream_writer_->Fail();
  body_stream_writer_.reset();

  if (client_) {
    TRACE_EVENT_WITH_FLOW0(
        "loading", "WebURLLoaderImpl::Context::OnCompletedRequest",
        this, TRACE_EVENT_FLAG_FLOW_IN);

    if (status.error_code != net::OK) {
      const WebURLError::HasCopyInCache has_copy_in_cache =
          status.exists_in_cache ? WebURLError::HasCopyInCache::kTrue
                                 : WebURLError::HasCopyInCache::kFalse;
      client_->DidFail(
          status.cors_error_status
              ? WebURLError(*status.cors_error_status, has_copy_in_cache, url_)
              : WebURLError(status.error_code, has_copy_in_cache,
                            WebURLError::IsWebSecurityViolation::kFalse, url_),
          total_transfer_size, encoded_body_size, status.decoded_body_length);
    } else {
      client_->DidFinishLoading(
          (status.completion_time - TimeTicks()).InSecondsF(),
          total_transfer_size, encoded_body_size, status.decoded_body_length,
          status.blocked_cross_site_document);
    }
  }
}

WebURLLoaderImpl::Context::~Context() {
  // We must be already cancelled at this point.
  DCHECK_LT(request_id_, 0);
}

void WebURLLoaderImpl::Context::CancelBodyStreaming() {
  scoped_refptr<Context> protect(this);

  // Notify renderer clients that the request is canceled.
  if (ftp_listing_delegate_) {
    ftp_listing_delegate_->OnCompletedRequest();
    ftp_listing_delegate_.reset(nullptr);
  }

  if (body_stream_writer_) {
    body_stream_writer_->Fail();
    body_stream_writer_.reset();
  }
  if (client_) {
    // TODO(yhirano): Set |stale_copy_in_cache| appropriately if possible.
    client_->DidFail(WebURLError(net::ERR_ABORTED, url_),
                     WebURLLoaderClient::kUnknownEncodedDataLength, 0, 0);
  }

  // Notify the browser process that the request is canceled.
  Cancel();
}

bool WebURLLoaderImpl::Context::CanHandleDataURLRequestLocally(
    const WebURLRequest& request) const {
  if (!request.Url().ProtocolIs(url::kDataScheme))
    return false;

  // The fast paths for data URL, Start() and HandleDataURL(), don't support
  // the downloadToFile option.
  if (request.DownloadToFile())
    return false;

  // Data url requests from object tags may need to be intercepted as streams
  // and so need to be sent to the browser.
  if (request.GetRequestContext() == WebURLRequest::kRequestContextObject)
    return false;

  // Optimize for the case where we can handle a data URL locally.  We must
  // skip this for data URLs targetted at frames since those could trigger a
  // download.
  //
  // NOTE: We special case MIME types we can render both for performance
  // reasons as well as to support unit tests.

#if defined(OS_ANDROID)
  // For compatibility reasons on Android we need to expose top-level data://
  // to the browser. In tests resource_dispatcher_ can be null, and test pages
  // need to be loaded locally.
  // For PlzNavigate, navigation requests were already checked in the browser.
  if (resource_dispatcher_ &&
      request.GetFrameType() ==
          network::mojom::RequestContextFrameType::kTopLevel) {
    if (!IsBrowserSideNavigationEnabled())
      return false;
  }
#endif

  if (request.GetFrameType() !=
          network::mojom::RequestContextFrameType::kTopLevel &&
      request.GetFrameType() !=
          network::mojom::RequestContextFrameType::kNested)
    return true;

  std::string mime_type, unused_charset;
  if (net::DataURL::Parse(request.Url(), &mime_type, &unused_charset,
                          nullptr) &&
      blink::IsSupportedMimeType(mime_type))
    return true;

  return false;
}

void WebURLLoaderImpl::Context::HandleDataURL() {
  DCHECK_NE(defers_loading_, DEFERRED_DATA);
  if (defers_loading_ == SHOULD_DEFER) {
      defers_loading_ = DEFERRED_DATA;
      return;
  }

  network::ResourceResponseInfo info;
  std::string data;

  int error_code = GetInfoFromDataURL(url_, &info, &data);

  if (error_code == net::OK) {
    OnReceivedResponse(info);
    auto size = data.size();
    if (size != 0)
      OnReceivedData(std::make_unique<FixedReceivedData>(data.data(), size));
  }

  network::URLLoaderCompletionStatus status(error_code);
  status.encoded_body_length = data.size();
  status.decoded_body_length = data.size();
  OnCompletedRequest(status);
}

// WebURLLoaderImpl::RequestPeerImpl ------------------------------------------

WebURLLoaderImpl::RequestPeerImpl::RequestPeerImpl(Context* context,
                                                   bool discard_body)
    : context_(context), discard_body_(discard_body) {}

void WebURLLoaderImpl::RequestPeerImpl::OnUploadProgress(uint64_t position,
                                                         uint64_t size) {
  context_->OnUploadProgress(position, size);
}

bool WebURLLoaderImpl::RequestPeerImpl::OnReceivedRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseInfo& info) {
  return context_->OnReceivedRedirect(redirect_info, info);
}

void WebURLLoaderImpl::RequestPeerImpl::OnReceivedResponse(
    const network::ResourceResponseInfo& info) {
  context_->OnReceivedResponse(info);
}

void WebURLLoaderImpl::RequestPeerImpl::OnDownloadedData(
    int len,
    int encoded_data_length) {
  context_->OnDownloadedData(len, encoded_data_length);
}

void WebURLLoaderImpl::RequestPeerImpl::OnReceivedData(
    std::unique_ptr<ReceivedData> data) {
  if (discard_body_)
    return;
  context_->OnReceivedData(std::move(data));
}

void WebURLLoaderImpl::RequestPeerImpl::OnTransferSizeUpdated(
    int transfer_size_diff) {
  context_->OnTransferSizeUpdated(transfer_size_diff);
}

void WebURLLoaderImpl::RequestPeerImpl::OnReceivedCachedMetadata(
    const char* data,
    int len) {
  if (discard_body_)
    return;
  context_->OnReceivedCachedMetadata(data, len);
}

void WebURLLoaderImpl::RequestPeerImpl::OnCompletedRequest(
    const network::URLLoaderCompletionStatus& status) {
  context_->OnCompletedRequest(status);
}

// WebURLLoaderImpl -----------------------------------------------------------

WebURLLoaderImpl::WebURLLoaderImpl(
    ResourceDispatcher* resource_dispatcher,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_refptr<SharedURLLoaderFactory> url_loader_factory)
    : WebURLLoaderImpl(resource_dispatcher,
                       std::move(task_runner),
                       std::move(url_loader_factory),
                       nullptr) {}

WebURLLoaderImpl::WebURLLoaderImpl(
    ResourceDispatcher* resource_dispatcher,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_refptr<SharedURLLoaderFactory> url_loader_factory,
    mojom::KeepAliveHandlePtr keep_alive_handle)
    : context_(new Context(this,
                           resource_dispatcher,
                           std::move(task_runner),
                           std::move(url_loader_factory),
                           std::move(keep_alive_handle))) {}

WebURLLoaderImpl::~WebURLLoaderImpl() {
  Cancel();
}

void WebURLLoaderImpl::PopulateURLResponse(
    const WebURL& url,
    const network::ResourceResponseInfo& info,
    WebURLResponse* response,
    bool report_security_info) {
  response->SetURL(url);
  response->SetResponseTime(info.response_time);
  response->SetMIMEType(WebString::FromUTF8(info.mime_type));
  response->SetTextEncodingName(WebString::FromUTF8(info.charset));
  response->SetExpectedContentLength(info.content_length);
  response->SetHasMajorCertificateErrors(
      net::IsCertStatusError(info.cert_status) &&
      !net::IsCertStatusMinorError(info.cert_status));
  response->SetCTPolicyCompliance(info.ct_policy_compliance);
  response->SetIsLegacySymantecCert(info.is_legacy_symantec_cert);
  response->SetAppCacheID(info.appcache_id);
  response->SetAppCacheManifestURL(info.appcache_manifest_url);
  response->SetWasCached(!info.load_timing.request_start_time.is_null() &&
                         info.response_time <
                             info.load_timing.request_start_time);
  response->SetRemoteIPAddress(
      WebString::FromUTF8(info.socket_address.HostForURL()));
  response->SetRemotePort(info.socket_address.port());
  response->SetConnectionID(info.load_timing.socket_log_id);
  response->SetConnectionReused(info.load_timing.socket_reused);
  response->SetDownloadFilePath(
      blink::FilePathToWebString(info.download_file_path));
  response->SetWasFetchedViaSPDY(info.was_fetched_via_spdy);
  response->SetWasFetchedViaServiceWorker(info.was_fetched_via_service_worker);
  response->SetWasFallbackRequiredByServiceWorker(
      info.was_fallback_required_by_service_worker);
  response->SetResponseTypeViaServiceWorker(
      info.response_type_via_service_worker);
  response->SetURLListViaServiceWorker(info.url_list_via_service_worker);
  response->SetCacheStorageCacheName(
      info.is_in_cache_storage
          ? blink::WebString::FromUTF8(info.cache_storage_cache_name)
          : blink::WebString());
  blink::WebVector<blink::WebString> cors_exposed_header_names(
      info.cors_exposed_header_names.size());
  std::transform(
      info.cors_exposed_header_names.begin(),
      info.cors_exposed_header_names.end(), cors_exposed_header_names.begin(),
      [](const std::string& h) { return blink::WebString::FromLatin1(h); });
  response->SetCorsExposedHeaderNames(cors_exposed_header_names);
  response->SetDidServiceWorkerNavigationPreload(
      info.did_service_worker_navigation_preload);
  response->SetEncodedDataLength(info.encoded_data_length);
  response->SetAlpnNegotiatedProtocol(
      WebString::FromUTF8(info.alpn_negotiated_protocol));
  response->SetConnectionInfo(info.connection_info);

  SetSecurityStyleAndDetails(url, info, response, report_security_info);

  WebURLResponseExtraDataImpl* extra_data = new WebURLResponseExtraDataImpl();
  response->SetExtraData(extra_data);
  extra_data->set_was_fetched_via_spdy(info.was_fetched_via_spdy);
  extra_data->set_was_alpn_negotiated(info.was_alpn_negotiated);
  extra_data->set_was_alternate_protocol_available(
      info.was_alternate_protocol_available);
  extra_data->set_previews_state(
      static_cast<PreviewsState>(info.previews_state));
  extra_data->set_effective_connection_type(info.effective_connection_type);

  // If there's no received headers end time, don't set load timing.  This is
  // the case for non-HTTP requests, requests that don't go over the wire, and
  // certain error cases.
  if (!info.load_timing.receive_headers_end.is_null()) {
    WebURLLoadTiming timing;
    PopulateURLLoadTiming(info.load_timing, &timing);
    const TimeTicks kNullTicks;
    timing.SetWorkerStart(
        (info.service_worker_start_time - kNullTicks).InSecondsF());
    timing.SetWorkerReady(
        (info.service_worker_ready_time - kNullTicks).InSecondsF());
    response->SetLoadTiming(timing);
  }

  if (info.raw_request_response_info.get()) {
    WebHTTPLoadInfo load_info;

    load_info.SetHTTPStatusCode(
        info.raw_request_response_info->http_status_code);
    load_info.SetHTTPStatusText(WebString::FromLatin1(
        info.raw_request_response_info->http_status_text));

    load_info.SetRequestHeadersText(WebString::FromLatin1(
        info.raw_request_response_info->request_headers_text));
    load_info.SetResponseHeadersText(WebString::FromLatin1(
        info.raw_request_response_info->response_headers_text));
    const HeadersVector& request_headers =
        info.raw_request_response_info->request_headers;
    for (HeadersVector::const_iterator it = request_headers.begin();
         it != request_headers.end(); ++it) {
      load_info.AddRequestHeader(WebString::FromLatin1(it->first),
                                 WebString::FromLatin1(it->second));
    }
    const HeadersVector& response_headers =
        info.raw_request_response_info->response_headers;
    for (HeadersVector::const_iterator it = response_headers.begin();
         it != response_headers.end(); ++it) {
      load_info.AddResponseHeader(WebString::FromLatin1(it->first),
                                  WebString::FromLatin1(it->second));
    }
    load_info.SetNPNNegotiatedProtocol(
        WebString::FromLatin1(info.alpn_negotiated_protocol));
    response->SetHTTPLoadInfo(load_info);
  }

  const net::HttpResponseHeaders* headers = info.headers.get();
  if (!headers)
    return;

  WebURLResponse::HTTPVersion version = WebURLResponse::kHTTPVersionUnknown;
  if (headers->GetHttpVersion() == net::HttpVersion(0, 9))
    version = WebURLResponse::kHTTPVersion_0_9;
  else if (headers->GetHttpVersion() == net::HttpVersion(1, 0))
    version = WebURLResponse::kHTTPVersion_1_0;
  else if (headers->GetHttpVersion() == net::HttpVersion(1, 1))
    version = WebURLResponse::kHTTPVersion_1_1;
  else if (headers->GetHttpVersion() == net::HttpVersion(2, 0))
    version = WebURLResponse::kHTTPVersion_2_0;
  response->SetHTTPVersion(version);
  response->SetHTTPStatusCode(headers->response_code());
  response->SetHTTPStatusText(WebString::FromLatin1(headers->GetStatusText()));

  // Build up the header map.
  size_t iter = 0;
  std::string name;
  std::string value;
  while (headers->EnumerateHeaderLines(&iter, &name, &value)) {
    response->AddHTTPHeaderField(WebString::FromLatin1(name),
                                 WebString::FromLatin1(value));
  }
}

void WebURLLoaderImpl::LoadSynchronously(const WebURLRequest& request,
                                         WebURLResponse& response,
                                         base::Optional<WebURLError>& error,
                                         WebData& data,
                                         int64_t& encoded_data_length,
                                         int64_t& encoded_body_length) {
  TRACE_EVENT0("loading", "WebURLLoaderImpl::loadSynchronously");
  SyncLoadResponse sync_load_response;
  context_->Start(request, &sync_load_response);

  const GURL& final_url = sync_load_response.url;

  // TODO(tc): For file loads, we may want to include a more descriptive
  // status code or status text.
  const int error_code = sync_load_response.error_code;
  if (error_code != net::OK) {
    if (sync_load_response.cors_error) {
      // TODO(toyoshim): Pass CORS error related headers here.
      error =
          WebURLError(network::CORSErrorStatus(*sync_load_response.cors_error),
                      WebURLError::HasCopyInCache::kFalse, final_url);
    } else {
      // SyncResourceHandler returns ERR_ABORTED for CORS redirect errors,
      // so we treat the error as a web security violation.
      const WebURLError::IsWebSecurityViolation is_web_security_violation =
          error_code == net::ERR_ABORTED
              ? WebURLError::IsWebSecurityViolation::kTrue
              : WebURLError::IsWebSecurityViolation::kFalse;
      error = WebURLError(error_code, WebURLError::HasCopyInCache::kFalse,
                          is_web_security_violation, final_url);
    }
    return;
  }

  PopulateURLResponse(final_url, sync_load_response, &response,
                      request.ReportRawHeaders());
  encoded_data_length = sync_load_response.encoded_data_length;
  encoded_body_length = sync_load_response.encoded_body_length;

  data.Assign(sync_load_response.data.data(), sync_load_response.data.size());
}

void WebURLLoaderImpl::LoadAsynchronously(const WebURLRequest& request,
                                          WebURLLoaderClient* client) {
  TRACE_EVENT_WITH_FLOW0("loading", "WebURLLoaderImpl::loadAsynchronously",
                         this, TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(!context_->client());

  context_->set_client(client);
  context_->Start(request, nullptr);
}

void WebURLLoaderImpl::Cancel() {
  context_->Cancel();
}

void WebURLLoaderImpl::SetDefersLoading(bool value) {
  context_->SetDefersLoading(value);
}

void WebURLLoaderImpl::DidChangePriority(WebURLRequest::Priority new_priority,
                                         int intra_priority_value) {
  context_->DidChangePriority(new_priority, intra_priority_value);
}

// static
// We have this function at the bottom of this file because it confuses
// syntax highliting.
net::NetworkTrafficAnnotationTag
WebURLLoaderImpl::Context::GetTrafficAnnotationTag(
    const blink::WebURLRequest& request) {
  switch (request.GetRequestContext()) {
    case WebURLRequest::kRequestContextUnspecified:
    case WebURLRequest::kRequestContextAudio:
    case WebURLRequest::kRequestContextBeacon:
    case WebURLRequest::kRequestContextCSPReport:
    case WebURLRequest::kRequestContextDownload:
    case WebURLRequest::kRequestContextEventSource:
    case WebURLRequest::kRequestContextFetch:
    case WebURLRequest::kRequestContextFont:
    case WebURLRequest::kRequestContextForm:
    case WebURLRequest::kRequestContextFrame:
    case WebURLRequest::kRequestContextHyperlink:
    case WebURLRequest::kRequestContextIframe:
    case WebURLRequest::kRequestContextImage:
    case WebURLRequest::kRequestContextImageSet:
    case WebURLRequest::kRequestContextImport:
    case WebURLRequest::kRequestContextInternal:
    case WebURLRequest::kRequestContextLocation:
    case WebURLRequest::kRequestContextManifest:
    case WebURLRequest::kRequestContextPing:
    case WebURLRequest::kRequestContextPrefetch:
    case WebURLRequest::kRequestContextScript:
    case WebURLRequest::kRequestContextServiceWorker:
    case WebURLRequest::kRequestContextSharedWorker:
    case WebURLRequest::kRequestContextSubresource:
    case WebURLRequest::kRequestContextStyle:
    case WebURLRequest::kRequestContextTrack:
    case WebURLRequest::kRequestContextVideo:
    case WebURLRequest::kRequestContextWorker:
    case WebURLRequest::kRequestContextXMLHttpRequest:
    case WebURLRequest::kRequestContextXSLT:
      return net::DefineNetworkTrafficAnnotation("blink_resource_loader", R"(
      semantics {
        sender: "Blink Resource Loader"
        description:
          "Blink-initiated request, which includes all resources for "
          "normal page loads, chrome URLs, and downloads."
        trigger:
          "The user navigates to a URL or downloads a file. Also when a "
          "webpage, ServiceWorker, or chrome:// uses any network communication."
        data: "Anything the initiator wants to send."
        destination: OTHER
      }
      policy {
        cookies_allowed: YES
        cookies_store: "user"
        setting: "These requests cannot be disabled in settings."
        policy_exception_justification:
          "Not implemented. Without these requests, Chrome will be unable "
          "to load any webpage."
      })");

    case WebURLRequest::kRequestContextEmbed:
    case WebURLRequest::kRequestContextObject:
    case WebURLRequest::kRequestContextPlugin:
      return net::DefineNetworkTrafficAnnotation(
          "blink_extension_resource_loader", R"(
        semantics {
          sender: "Blink Resource Loader"
          description:
            "Blink-initiated request for resources required for NaCl instances "
            "tagged with <embed> or <object>, or installed extensions."
          trigger:
            "An extension or NaCl instance may initiate a request at any time, "
            "even in the background."
          data: "Anything the initiator wants to send."
          destination: OTHER
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "These requests cannot be disabled in settings, but they are "
            "sent only if user installs extensions."
          chrome_policy {
            ExtensionInstallBlacklist {
              ExtensionInstallBlacklist: {
                entries: '*'
              }
            }
          }
        })");

    case WebURLRequest::kRequestContextFavicon:
      return net::DefineNetworkTrafficAnnotation("favicon_loader", R"(
        semantics {
          sender: "Blink Resource Loader"
          description:
            "Chrome sends a request to download favicon for a URL."
          trigger:
            "Navigating to a URL."
          data: "None."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting: "These requests cannot be disabled in settings."
          policy_exception_justification:
            "Not implemented."
        })");
  }

  return net::NetworkTrafficAnnotationTag::NotReached();
}

}  // namespace content
