// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_proxying_url_loader_factory.h"

#include <utility>

#include "android_webview/browser/android_protocol_handler.h"
#include "android_webview/browser/aw_contents_client_bridge.h"
#include "android_webview/browser/aw_contents_io_thread_client.h"
#include "android_webview/browser/input_stream.h"
#include "android_webview/browser/net/aw_web_resource_response.h"
#include "android_webview/browser/net_helpers.h"
#include "android_webview/browser/net_network_service/android_stream_reader_url_loader.h"
#include "android_webview/browser/renderer_host/auto_login_parser.h"
#include "android_webview/common/url_constants.h"
#include "base/android/build_info.h"
#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_utils.h"
#include "net/base/load_flags.h"
#include "net/http/http_util.h"

namespace android_webview {

namespace {

const char kAutoLoginHeaderName[] = "X-Auto-Login";
const char kRequestedWithHeaderName[] = "X-Requested-With";

class InterceptResponseDelegate
    : public AndroidStreamReaderURLLoader::ResponseDelegate {
 public:
  explicit InterceptResponseDelegate(
      std::unique_ptr<AwWebResourceResponse> response)
      : response_(std::move(response)) {}

  std::unique_ptr<android_webview::InputStream> OpenInputStream(
      JNIEnv* env) override {
    return response_->GetInputStream(env);
  }

  void OnInputStreamOpenFailed(bool* restarted) override { *restarted = false; }

  bool GetMimeType(JNIEnv* env,
                   const GURL& url,
                   android_webview::InputStream* stream,
                   std::string* mime_type) override {
    return response_->GetMimeType(env, mime_type);
  }

  bool GetCharset(JNIEnv* env,
                  const GURL& url,
                  android_webview::InputStream* stream,
                  std::string* charset) override {
    return response_->GetCharset(env, charset);
  }

  void AppendResponseHeaders(JNIEnv* env,
                             net::HttpResponseHeaders* headers) override {
    int status_code;
    std::string reason_phrase;
    if (response_->GetStatusInfo(env, &status_code, &reason_phrase)) {
      std::string status_line("HTTP/1.1 ");
      status_line.append(base::NumberToString(status_code));
      status_line.append(" ");
      status_line.append(reason_phrase);
      headers->ReplaceStatusLine(status_line);
    }
    response_->GetResponseHeaders(env, headers);
  }

 private:
  std::unique_ptr<AwWebResourceResponse> response_;
};

// Handles intercepted, in-progress requests/responses, so that they can be
// controlled and modified accordingly.
class InterceptedRequest : public network::mojom::URLLoader,
                           public network::mojom::URLLoaderClient {
 public:
  InterceptedRequest(
      int process_id,
      uint64_t request_id,
      int32_t routing_id,
      uint32_t options,
      const network::ResourceRequest& request,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      network::mojom::URLLoaderRequest loader_request,
      network::mojom::URLLoaderClientPtr client,
      network::mojom::URLLoaderFactoryPtr target_factory);
  ~InterceptedRequest() override;

  void Restart();

  // network::mojom::URLLoaderClient
  void OnReceiveResponse(const network::ResourceResponseHead& head) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const network::ResourceResponseHead& head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  // network::mojom::URLLoader
  void FollowRedirect(const std::vector<std::string>& removed_headers,
                      const net::HttpRequestHeaders& modified_headers,
                      const base::Optional<GURL>& new_url) override;
  void ProceedWithResponse() override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  void ContinueAfterIntercept();
  void ContinueAfterInterceptWithOverride(
      std::unique_ptr<AwWebResourceResponse> response);

  void InterceptResponseReceived(
      std::unique_ptr<AwWebResourceResponse> response);

  void InputStreamFailed();

 private:
  std::unique_ptr<AwContentsIoThreadClient> GetIoThreadClient();
  void OnRequestError(const network::URLLoaderCompletionStatus& status);

  // TODO(timvolodine): consider factoring this out of this class.
  void OnReceivedErrorToCallback(int error_code);
  bool ShouldNotInterceptRequest();

  const int process_id_;
  const uint64_t request_id_;
  const int32_t routing_id_;
  const uint32_t options_;
  bool input_stream_previously_failed_ = false;
  bool request_was_redirected_ = false;

  network::ResourceRequest request_;
  const net::MutableNetworkTrafficAnnotationTag traffic_annotation_;

  mojo::Binding<network::mojom::URLLoader> proxied_loader_binding_;
  network::mojom::URLLoaderClientPtr target_client_;

  mojo::Binding<network::mojom::URLLoaderClient> proxied_client_binding_;
  network::mojom::URLLoaderPtr target_loader_;
  network::mojom::URLLoaderFactoryPtr target_factory_;

  base::WeakPtrFactory<InterceptedRequest> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InterceptedRequest);
};

class ProtocolResponseDelegate
    : public AndroidStreamReaderURLLoader::ResponseDelegate {
 public:
  ProtocolResponseDelegate(const GURL& url,
                           base::WeakPtr<InterceptedRequest> request)
      : url_(url), request_(request) {}

  std::unique_ptr<android_webview::InputStream> OpenInputStream(
      JNIEnv* env) override {
    return CreateInputStream(env, url_);
  }

  void OnInputStreamOpenFailed(bool* restarted) override {
    request_->InputStreamFailed();
    *restarted = true;
  }

  bool GetMimeType(JNIEnv* env,
                   const GURL& url,
                   android_webview::InputStream* stream,
                   std::string* mime_type) override {
    return GetInputStreamMimeType(env, url, stream, mime_type);
  }

  bool GetCharset(JNIEnv* env,
                  const GURL& url,
                  android_webview::InputStream* stream,
                  std::string* charset) override {
    // TODO: We should probably be getting this from the managed side.
    return false;
  }

  void AppendResponseHeaders(JNIEnv* env,
                             net::HttpResponseHeaders* headers) override {
    // no-op
  }

 private:
  GURL url_;
  base::WeakPtr<InterceptedRequest> request_;
};

InterceptedRequest::InterceptedRequest(
    int process_id,
    uint64_t request_id,
    int32_t routing_id,
    uint32_t options,
    const network::ResourceRequest& request,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    network::mojom::URLLoaderRequest loader_request,
    network::mojom::URLLoaderClientPtr client,
    network::mojom::URLLoaderFactoryPtr target_factory)
    : process_id_(process_id),
      request_id_(request_id),
      routing_id_(routing_id),
      options_(options),
      request_(request),
      traffic_annotation_(traffic_annotation),
      proxied_loader_binding_(this, std::move(loader_request)),
      target_client_(std::move(client)),
      proxied_client_binding_(this),
      target_factory_(std::move(target_factory)),
      weak_factory_(this) {
  // If there is a client error, clean up the request.
  target_client_.set_connection_error_handler(base::BindOnce(
      &InterceptedRequest::OnRequestError, weak_factory_.GetWeakPtr(),
      network::URLLoaderCompletionStatus(net::ERR_ABORTED)));
}

InterceptedRequest::~InterceptedRequest() {}

void InterceptedRequest::Restart() {
  // TODO(timvolodine): add async check shouldOverrideUrlLoading and
  // shouldInterceptRequest.

  std::unique_ptr<AwContentsIoThreadClient> io_thread_client =
      GetIoThreadClient();
  DCHECK(io_thread_client);

  if (ShouldBlockURL(request_.url, io_thread_client.get())) {
    OnRequestError(network::URLLoaderCompletionStatus(net::ERR_ACCESS_DENIED));
    return;
  }

  request_.load_flags =
      UpdateLoadFlags(request_.load_flags, io_thread_client.get());

  if (ShouldNotInterceptRequest()) {
    // equivalent to no interception
    InterceptResponseReceived(nullptr);
  } else {
    if (request_.referrer.is_valid()) {
      // intentionally override if referrer header already exists
      request_.headers.SetHeader(net::HttpRequestHeaders::kReferer,
                                 request_.referrer.spec());
    }

    // TODO: verify the case when WebContents::RenderFrameDeleted is called
    // before network request is intercepted (i.e. if that's possible and
    // whether it can result in any issues).
    io_thread_client->ShouldInterceptRequestAsync(
        AwWebResourceRequest(request_),
        base::BindOnce(&InterceptedRequest::InterceptResponseReceived,
                       weak_factory_.GetWeakPtr()));
  }
}

// logic for when not to invoke shouldInterceptRequest callback
bool InterceptedRequest::ShouldNotInterceptRequest() {
  if (request_was_redirected_)
    return true;

  // Do not call shouldInterceptRequest callback for special android urls,
  // unless they fail to load on first attempt. Special android urls are urls
  // such as "file:///android_asset/", "file:///android_res/" urls or
  // "content:" scheme urls.
  return !input_stream_previously_failed_ &&
         (request_.url.SchemeIs(url::kContentScheme) ||
          android_webview::IsAndroidSpecialFileUrl(request_.url));
}

void SetRequestedWithHeader(net::HttpRequestHeaders& headers) {
  // We send the application's package name in the X-Requested-With header for
  // compatibility with previous WebView versions. This should not be visible to
  // shouldInterceptRequest.
  headers.SetHeaderIfMissing(
      kRequestedWithHeaderName,
      base::android::BuildInfo::GetInstance()->host_package_name());
}

void InterceptedRequest::InterceptResponseReceived(
    std::unique_ptr<AwWebResourceResponse> response) {
  SetRequestedWithHeader(request_.headers);

  if (response) {
    // non-null response: make sure to use it as an override for the
    // normal network data.
    ContinueAfterInterceptWithOverride(std::move(response));
  } else {
    ContinueAfterIntercept();
  }
}

void InterceptedRequest::InputStreamFailed() {
  DCHECK(!input_stream_previously_failed_);
  input_stream_previously_failed_ = true;
  proxied_client_binding_.Unbind();
  Restart();
}

void InterceptedRequest::ContinueAfterIntercept() {
  // For WebViewClassic compatibility this job can only accept URLs that can be
  // opened. URLs that cannot be opened should be resolved by the next handler.
  //
  // If a request is initially handled here but the job fails due to it being
  // unable to open the InputStream for that request the request is marked as
  // previously failed and restarted.
  // Restarting a request involves creating a new job for that request. This
  // handler will ignore requests known to have previously failed to 1) prevent
  // an infinite loop, 2) ensure that the next handler in line gets the
  // opportunity to create a job for the request.
  if (!input_stream_previously_failed_ &&
      (request_.url.SchemeIs(url::kContentScheme) ||
       android_webview::IsAndroidSpecialFileUrl(request_.url))) {
    network::mojom::URLLoaderClientPtr proxied_client;
    proxied_client_binding_.Bind(mojo::MakeRequest(&proxied_client));
    AndroidStreamReaderURLLoader* loader = new AndroidStreamReaderURLLoader(
        request_, std::move(proxied_client), traffic_annotation_,
        std::make_unique<ProtocolResponseDelegate>(request_.url,
                                                   weak_factory_.GetWeakPtr()));
    loader->Start();
    return;
  }

  if (!target_loader_ && target_factory_) {
    network::mojom::URLLoaderClientPtr proxied_client;
    proxied_client_binding_.Bind(mojo::MakeRequest(&proxied_client));
    target_factory_->CreateLoaderAndStart(
        mojo::MakeRequest(&target_loader_), routing_id_, request_id_, options_,
        request_, std::move(proxied_client), traffic_annotation_);
  }
}

void InterceptedRequest::ContinueAfterInterceptWithOverride(
    std::unique_ptr<AwWebResourceResponse> response) {
  network::mojom::URLLoaderClientPtr proxied_client;
  proxied_client_binding_.Bind(mojo::MakeRequest(&proxied_client));
  AndroidStreamReaderURLLoader* loader = new AndroidStreamReaderURLLoader(
      request_, std::move(proxied_client), traffic_annotation_,
      std::make_unique<InterceptResponseDelegate>(std::move(response)));
  loader->Start();
}

namespace {
// TODO(timvolodine): consider factoring this out of this file.

AwContentsClientBridge* GetAwContentsClientBridgeFromID(int process_id,
                                                        int render_frame_id) {
  content::WebContents* wc =
      process_id
          ? content::WebContents::FromRenderFrameHost(
                content::RenderFrameHost::FromID(process_id, render_frame_id))
          : content::WebContents::FromFrameTreeNodeId(render_frame_id);
  return AwContentsClientBridge::FromWebContents(wc);
}

void OnReceivedHttpErrorOnUiThread(
    int process_id,
    int render_frame_id,
    const AwWebResourceRequest& request,
    std::unique_ptr<AwContentsClientBridge::HttpErrorInfo> http_error_info) {
  auto* client = GetAwContentsClientBridgeFromID(process_id, render_frame_id);
  if (!client) {
    DLOG(WARNING) << "client is null, onReceivedHttpError dropped for "
                  << request.url;
    return;
  }
  client->OnReceivedHttpError(request, std::move(http_error_info));
}

void OnReceivedErrorOnUiThread(int process_id,
                               int render_frame_id,
                               const AwWebResourceRequest& request,
                               int error_code) {
  auto* client = GetAwContentsClientBridgeFromID(process_id, render_frame_id);
  if (!client) {
    DLOG(WARNING) << "client is null, onReceivedError dropped for "
                  << request.url;
    return;
  }
  // TODO(timvolodine): properly handle safe_browsing_hit.
  client->OnReceivedError(request, error_code, false /*safebrowsing_hit*/);
}

void OnNewLoginRequestOnUiThread(int process_id,
                                 int render_frame_id,
                                 const std::string& realm,
                                 const std::string& account,
                                 const std::string& args) {
  auto* client = GetAwContentsClientBridgeFromID(process_id, render_frame_id);
  if (!client) {
    return;
  }
  client->NewLoginRequest(realm, account, args);
}

}  // namespace

// URLLoaderClient methods.

void InterceptedRequest::OnReceiveResponse(
    const network::ResourceResponseHead& head) {
  // intercept response headers here
  // pause/resume proxied_client_binding_ if necessary

  if (head.headers && head.headers->response_code() >= 400) {
    // In Android WebView the WebViewClient.onReceivedHttpError callback
    // is invoked for any resource (main page, iframe, image, etc.) with
    // status code >= 400.
    std::unique_ptr<AwContentsClientBridge::HttpErrorInfo> error_info =
        AwContentsClientBridge::ExtractHttpErrorInfo(head.headers.get());

    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&OnReceivedHttpErrorOnUiThread, process_id_,
                       request_.render_frame_id, AwWebResourceRequest(request_),
                       std::move(error_info)));
  }

  if (request_.resource_type == content::RESOURCE_TYPE_MAIN_FRAME) {
    // Check for x-auto-login-header
    HeaderData header_data;
    std::string header_string;
    if (head.headers && head.headers->GetNormalizedHeader(kAutoLoginHeaderName,
                                                          &header_string)) {
      if (ParseHeader(header_string, ALLOW_ANY_REALM, &header_data)) {
        // TODO(timvolodine): consider simplifying this and above callback
        // code, crbug.com/897149.
        base::PostTaskWithTraits(
            FROM_HERE, {content::BrowserThread::UI},
            base::BindOnce(&OnNewLoginRequestOnUiThread, process_id_,
                           request_.render_frame_id, header_data.realm,
                           header_data.account, header_data.args));
      }
    }
  }

  target_client_->OnReceiveResponse(head);
}

void InterceptedRequest::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& head) {
  // TODO(timvolodine): handle redirect override.
  // TODO(timvolodine): handle unsafe redirect case.
  request_was_redirected_ = true;
  target_client_->OnReceiveRedirect(redirect_info, head);
  request_.url = redirect_info.new_url;
  request_.method = redirect_info.new_method;
  request_.site_for_cookies = redirect_info.new_site_for_cookies;
  request_.referrer = GURL(redirect_info.new_referrer);
  request_.referrer_policy = redirect_info.new_referrer_policy;
}

void InterceptedRequest::OnUploadProgress(int64_t current_position,
                                          int64_t total_size,
                                          OnUploadProgressCallback callback) {
  target_client_->OnUploadProgress(current_position, total_size,
                                   std::move(callback));
}

void InterceptedRequest::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {
  target_client_->OnReceiveCachedMetadata(data);
}

void InterceptedRequest::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  target_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void InterceptedRequest::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  target_client_->OnStartLoadingResponseBody(std::move(body));
}

void InterceptedRequest::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  if (status.error_code != net::OK) {
    OnRequestError(status);
    return;
  }

  target_client_->OnComplete(status);

  // Deletes |this|.
  // TODO(timvolodine): consider doing this via the factory.
  delete this;
}

// URLLoader methods.

void InterceptedRequest::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const base::Optional<GURL>& new_url) {
  if (target_loader_)
    target_loader_->FollowRedirect(removed_headers, modified_headers, new_url);

  Restart();
}

void InterceptedRequest::ProceedWithResponse() {
  if (target_loader_)
    target_loader_->ProceedWithResponse();
}

void InterceptedRequest::SetPriority(net::RequestPriority priority,
                                     int32_t intra_priority_value) {
  if (target_loader_)
    target_loader_->SetPriority(priority, intra_priority_value);
}

void InterceptedRequest::PauseReadingBodyFromNet() {
  if (target_loader_)
    target_loader_->PauseReadingBodyFromNet();
}

void InterceptedRequest::ResumeReadingBodyFromNet() {
  if (target_loader_)
    target_loader_->ResumeReadingBodyFromNet();
}

std::unique_ptr<AwContentsIoThreadClient>
InterceptedRequest::GetIoThreadClient() {
  if (request_.originated_from_service_worker) {
    return AwContentsIoThreadClient::GetServiceWorkerIoThreadClient();
  }

  // |process_id_| == 0 indicates this is a navigation, and so we should use the
  // frame_tree_node_id API (with request_.render_frame_id).
  return process_id_
             ? AwContentsIoThreadClient::FromID(process_id_,
                                                request_.render_frame_id)
             : AwContentsIoThreadClient::FromID(request_.render_frame_id);
}

void InterceptedRequest::OnRequestError(
    const network::URLLoaderCompletionStatus& status) {
  target_client_->OnComplete(status);

  OnReceivedErrorToCallback(status.error_code);

  // Deletes |this|.
  // TODO(timvolodine): consider handling this through a data structure.
  delete this;
}

void InterceptedRequest::OnReceivedErrorToCallback(int error_code) {
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&OnReceivedErrorOnUiThread, process_id_,
                     request_.render_frame_id, AwWebResourceRequest(request_),
                     error_code));
}

}  // namespace

//============================
// AwProxyingURLLoaderFactory
//============================

AwProxyingURLLoaderFactory::AwProxyingURLLoaderFactory(
    int process_id,
    network::mojom::URLLoaderFactoryRequest loader_request,
    network::mojom::URLLoaderFactoryPtrInfo target_factory_info,
    std::unique_ptr<AwInterceptedRequestHandler> request_handler)
    : process_id_(process_id),
      request_handler_(std::move(request_handler)),
      weak_factory_(this) {
  // actual creation of the factory
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  target_factory_.Bind(std::move(target_factory_info));
  target_factory_.set_connection_error_handler(
      base::BindOnce(&AwProxyingURLLoaderFactory::OnTargetFactoryError,
                     base::Unretained(this)));
  proxy_bindings_.AddBinding(this, std::move(loader_request));
  proxy_bindings_.set_connection_error_handler(
      base::BindRepeating(&AwProxyingURLLoaderFactory::OnProxyBindingError,
                          base::Unretained(this)));
}

AwProxyingURLLoaderFactory::~AwProxyingURLLoaderFactory() {}

// static
void AwProxyingURLLoaderFactory::CreateProxy(
    int process_id,
    network::mojom::URLLoaderFactoryRequest loader_request,
    network::mojom::URLLoaderFactoryPtrInfo target_factory_info,
    std::unique_ptr<AwInterceptedRequestHandler> request_handler) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // will manage its own lifetime
  new AwProxyingURLLoaderFactory(process_id, std::move(loader_request),
                                 std::move(target_factory_info),
                                 std::move(request_handler));
}

void AwProxyingURLLoaderFactory::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest loader,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  bool pass_through = false;
  if (pass_through) {
    // this is the so-called pass-through, no-op option.
    target_factory_->CreateLoaderAndStart(
        std::move(loader), routing_id, request_id, options, request,
        std::move(client), traffic_annotation);
    return;
  }

  // TODO(timvolodine): handle interception, modification (headers for
  // webview), blocking, callbacks etc..

  network::mojom::URLLoaderFactoryPtr target_factory_clone;
  target_factory_->Clone(MakeRequest(&target_factory_clone));

  // manages its own lifecycle
  // TODO(timvolodine): consider keeping track of requests.
  InterceptedRequest* req = new InterceptedRequest(
      process_id_, request_id, routing_id, options, request, traffic_annotation,
      std::move(loader), std::move(client), std::move(target_factory_clone));
  req->Restart();
}

void AwProxyingURLLoaderFactory::OnTargetFactoryError() {
  delete this;
}

void AwProxyingURLLoaderFactory::OnProxyBindingError() {
  if (proxy_bindings_.empty())
    delete this;
}

void AwProxyingURLLoaderFactory::Clone(
    network::mojom::URLLoaderFactoryRequest loader_request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  proxy_bindings_.AddBinding(this, std::move(loader_request));
}

}  // namespace android_webview
