// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_url_interceptor_request_job.h"

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/protocol/page.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_element_reader.h"
#include "net/cert/cert_status_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_context.h"

namespace content {

namespace {
const char* ResourceTypeToString(ResourceType resource_type) {
  switch (resource_type) {
    case RESOURCE_TYPE_MAIN_FRAME:
      return protocol::Page::ResourceTypeEnum::Document;
    case RESOURCE_TYPE_SUB_FRAME:
      return protocol::Page::ResourceTypeEnum::Document;
    case RESOURCE_TYPE_STYLESHEET:
      return protocol::Page::ResourceTypeEnum::Stylesheet;
    case RESOURCE_TYPE_SCRIPT:
      return protocol::Page::ResourceTypeEnum::Script;
    case RESOURCE_TYPE_IMAGE:
      return protocol::Page::ResourceTypeEnum::Image;
    case RESOURCE_TYPE_FONT_RESOURCE:
      return protocol::Page::ResourceTypeEnum::Font;
    case RESOURCE_TYPE_SUB_RESOURCE:
      return protocol::Page::ResourceTypeEnum::Other;
    case RESOURCE_TYPE_OBJECT:
      return protocol::Page::ResourceTypeEnum::Other;
    case RESOURCE_TYPE_MEDIA:
      return protocol::Page::ResourceTypeEnum::Media;
    case RESOURCE_TYPE_WORKER:
      return protocol::Page::ResourceTypeEnum::Other;
    case RESOURCE_TYPE_SHARED_WORKER:
      return protocol::Page::ResourceTypeEnum::Other;
    case RESOURCE_TYPE_PREFETCH:
      return protocol::Page::ResourceTypeEnum::Fetch;
    case RESOURCE_TYPE_FAVICON:
      return protocol::Page::ResourceTypeEnum::Other;
    case RESOURCE_TYPE_XHR:
      return protocol::Page::ResourceTypeEnum::XHR;
    case RESOURCE_TYPE_PING:
      return protocol::Page::ResourceTypeEnum::Other;
    case RESOURCE_TYPE_SERVICE_WORKER:
      return protocol::Page::ResourceTypeEnum::Other;
    case RESOURCE_TYPE_CSP_REPORT:
      return protocol::Page::ResourceTypeEnum::Other;
    case RESOURCE_TYPE_PLUGIN_RESOURCE:
      return protocol::Page::ResourceTypeEnum::Other;
    default:
      return protocol::Page::ResourceTypeEnum::Other;
  }
}

void UnregisterNavigationRequestOnUI(
    base::WeakPtr<protocol::NetworkHandler> network_handler,
    std::string interception_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!network_handler)
    return;
  network_handler->InterceptedNavigationRequestFinished(interception_id);
}

void SendRequestInterceptedEventOnUiThread(
    base::WeakPtr<protocol::NetworkHandler> network_handler,
    std::string interception_id,
    GlobalRequestID global_request_id,
    std::unique_ptr<protocol::Network::Request> network_request,
    ResourceType resource_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!network_handler)
    return;
  bool is_navigation_request = resource_type == RESOURCE_TYPE_MAIN_FRAME ||
                               resource_type == RESOURCE_TYPE_SUB_FRAME;
  if (is_navigation_request) {
    network_handler->InterceptedNavigationRequest(global_request_id,
                                                  interception_id);
  }
  network_handler->frontend()->RequestIntercepted(
      interception_id, std::move(network_request),
      ResourceTypeToString(resource_type), is_navigation_request);
}

void SendRedirectInterceptedEventOnUiThread(
    base::WeakPtr<protocol::NetworkHandler> network_handler,
    std::string interception_id,
    std::unique_ptr<protocol::Network::Request> network_request,
    ResourceType resource_type,
    std::unique_ptr<protocol::Object> headers_object,
    int http_status_code,
    std::string redirect_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!network_handler)
    return;
  bool is_navigation_request = resource_type == RESOURCE_TYPE_MAIN_FRAME ||
                               resource_type == RESOURCE_TYPE_SUB_FRAME;
  network_handler->frontend()->RequestIntercepted(
      interception_id, std::move(network_request),
      ResourceTypeToString(resource_type), is_navigation_request,
      std::move(headers_object), http_status_code, redirect_url);
}

void SendAuthRequiredEventOnUiThread(
    base::WeakPtr<protocol::NetworkHandler> network_handler,
    std::string interception_id,
    std::unique_ptr<protocol::Network::Request> network_request,
    ResourceType resource_type,
    std::unique_ptr<protocol::Network::AuthChallenge> auth_challenge) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!network_handler)
    return;
  bool is_navigation_request = resource_type == RESOURCE_TYPE_MAIN_FRAME ||
                               resource_type == RESOURCE_TYPE_SUB_FRAME;
  network_handler->frontend()->RequestIntercepted(
      interception_id, std::move(network_request),
      ResourceTypeToString(resource_type), is_navigation_request,
      protocol::Maybe<protocol::Network::Headers>(), protocol::Maybe<int>(),
      protocol::Maybe<protocol::String>(), std::move(auth_challenge));
}

class ProxyUploadElementReader : public net::UploadElementReader {
 public:
  explicit ProxyUploadElementReader(net::UploadElementReader* reader)
      : reader_(reader) {}

  ~ProxyUploadElementReader() override {}

  // net::UploadElementReader overrides:
  int Init(const net::CompletionCallback& callback) override {
    return reader_->Init(callback);
  }

  uint64_t GetContentLength() const override {
    return reader_->GetContentLength();
  }

  uint64_t BytesRemaining() const override { return reader_->BytesRemaining(); }

  bool IsInMemory() const override { return reader_->IsInMemory(); }

  int Read(net::IOBuffer* buf,
           int buf_length,
           const net::CompletionCallback& callback) override {
    return reader_->Read(buf, buf_length, callback);
  }

 private:
  net::UploadElementReader* reader_;  // NOT OWNED

  DISALLOW_COPY_AND_ASSIGN(ProxyUploadElementReader);
};

std::unique_ptr<net::UploadDataStream> GetUploadData(net::URLRequest* request) {
  if (!request->has_upload())
    return nullptr;

  const net::UploadDataStream* stream = request->get_upload();
  auto* readers = stream->GetElementReaders();
  if (!readers || readers->empty())
    return nullptr;

  std::vector<std::unique_ptr<net::UploadElementReader>> proxy_readers;
  proxy_readers.reserve(readers->size());
  for (auto& reader : *readers) {
    proxy_readers.push_back(
        base::MakeUnique<ProxyUploadElementReader>(reader.get()));
  }

  return base::MakeUnique<net::ElementsUploadDataStream>(
      std::move(proxy_readers), 0);
}
}  // namespace

DevToolsURLInterceptorRequestJob::DevToolsURLInterceptorRequestJob(
    scoped_refptr<DevToolsURLRequestInterceptor::State>
        devtools_url_request_interceptor_state,
    const std::string& interception_id,
    net::URLRequest* original_request,
    net::NetworkDelegate* original_network_delegate,
    WebContents* web_contents,
    base::WeakPtr<protocol::NetworkHandler> network_handler,
    bool is_redirect,
    ResourceType resource_type)
    : net::URLRequestJob(original_request, original_network_delegate),
      devtools_url_request_interceptor_state_(
          devtools_url_request_interceptor_state),
      request_details_(original_request->url(),
                       original_request->method(),
                       GetUploadData(original_request),
                       original_request->extra_request_headers(),
                       original_request->priority(),
                       original_request->context()),
      waiting_for_user_response_(WaitingForUserResponse::NOT_WAITING),
      intercepting_requests_(true),
      killed_(false),
      interception_id_(interception_id),
      web_contents_(web_contents),
      network_handler_(network_handler),
      is_redirect_(is_redirect),
      resource_type_(resource_type),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (const ResourceRequestInfo* info =
          ResourceRequestInfo::ForRequest(original_request)) {
    global_request_id_ = info->GetGlobalRequestID();
  }
}

DevToolsURLInterceptorRequestJob::~DevToolsURLInterceptorRequestJob() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  bool is_navigation_request = resource_type_ == RESOURCE_TYPE_MAIN_FRAME ||
                               resource_type_ == RESOURCE_TYPE_SUB_FRAME;
  if (is_navigation_request) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(UnregisterNavigationRequestOnUI,
                                       network_handler_, interception_id_));
  }
  devtools_url_request_interceptor_state_->JobFinished(interception_id_);
}

// net::URLRequestJob implementation:
void DevToolsURLInterceptorRequestJob::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  request_details_.extra_request_headers = headers;
}

void DevToolsURLInterceptorRequestJob::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (is_redirect_ || !intercepting_requests_) {
    // If this is a fetch in response to a redirect, we have already sent the
    // Network.requestIntercepted event and the user opted to allow it so
    // there's no need to send another. We can just start the SubRequest.
    // If we're not intercepting results we can also just start the SubRequest.
    sub_request_.reset(new SubRequest(request_details_, this,
                                      devtools_url_request_interceptor_state_));
  } else {
    waiting_for_user_response_ =
        WaitingForUserResponse::WAITING_FOR_INTERCEPTION_RESPONSE;
    std::unique_ptr<protocol::Network::Request> network_request =
        protocol::NetworkHandler::CreateRequestFromURLRequest(request());
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(SendRequestInterceptedEventOnUiThread, network_handler_,
                   interception_id_, global_request_id_,
                   base::Passed(&network_request), resource_type_));
  }
}

void DevToolsURLInterceptorRequestJob::Kill() {
  killed_ = true;
  if (sub_request_)
    sub_request_->Cancel();

  URLRequestJob::Kill();
}

int DevToolsURLInterceptorRequestJob::ReadRawData(net::IOBuffer* buf,
                                                  int buf_size) {
  if (sub_request_) {
    int size = sub_request_->request()->Read(buf, buf_size);
    return size;
  } else {
    CHECK(mock_response_details_);
    return mock_response_details_->ReadRawData(buf, buf_size);
  }
}

int DevToolsURLInterceptorRequestJob::GetResponseCode() const {
  if (sub_request_) {
    return sub_request_->request()->GetResponseCode();
  } else {
    CHECK(mock_response_details_);
    return mock_response_details_->response_headers()->response_code();
  }
}

void DevToolsURLInterceptorRequestJob::GetResponseInfo(
    net::HttpResponseInfo* info) {
  // NOTE this can get called during URLRequestJob::NotifyStartError in which
  // case we might not have either a sub request or a mock response.
  if (sub_request_) {
    *info = sub_request_->request()->response_info();
  } else if (mock_response_details_) {
    info->headers = mock_response_details_->response_headers();
  }
}

const net::HttpResponseHeaders*
DevToolsURLInterceptorRequestJob::GetHttpResponseHeaders() const {
  if (sub_request_) {
    net::URLRequest* request = sub_request_->request();
    return request->response_info().headers.get();
  }
  CHECK(mock_response_details_);
  return mock_response_details_->response_headers().get();
}

bool DevToolsURLInterceptorRequestJob::GetMimeType(
    std::string* mime_type) const {
  const net::HttpResponseHeaders* response_headers = GetHttpResponseHeaders();
  if (!response_headers)
    return false;
  return response_headers->GetMimeType(mime_type);
}

bool DevToolsURLInterceptorRequestJob::GetCharset(std::string* charset) {
  const net::HttpResponseHeaders* response_headers = GetHttpResponseHeaders();
  if (!response_headers)
    return false;
  return response_headers->GetCharset(charset);
}

void DevToolsURLInterceptorRequestJob::GetLoadTimingInfo(
    net::LoadTimingInfo* load_timing_info) const {
  if (sub_request_) {
    sub_request_->request()->GetLoadTimingInfo(load_timing_info);
  } else {
    CHECK(mock_response_details_);
    // Since this request is mocked most of the fields are irrelevant.
    load_timing_info->receive_headers_end =
        mock_response_details_->response_time();
  }
}

bool DevToolsURLInterceptorRequestJob::NeedsAuth() {
  return !!auth_info_;
}

void DevToolsURLInterceptorRequestJob::GetAuthChallengeInfo(
    scoped_refptr<net::AuthChallengeInfo>* auth_info) {
  *auth_info = auth_info_.get();
}

void DevToolsURLInterceptorRequestJob::SetAuth(
    const net::AuthCredentials& credentials) {
  sub_request_->request()->SetAuth(credentials);
  auth_info_ = nullptr;
}

void DevToolsURLInterceptorRequestJob::CancelAuth() {
  sub_request_->request()->CancelAuth();
  auth_info_ = nullptr;
}

void DevToolsURLInterceptorRequestJob::OnAuthRequired(
    net::URLRequest* request,
    net::AuthChallengeInfo* auth_info) {
  DCHECK(sub_request_);
  DCHECK_EQ(request, sub_request_->request());
  auth_info_ = auth_info;

  if (!intercepting_requests_) {
    // This should trigger default auth behavior.
    // See comment in ProcessAuthRespose.
    NotifyHeadersComplete();
    return;
  }

  // This notification came from the sub requests URLRequest::Delegate and
  // depending on what the protocol user wants us to do we must either cancel
  // the auth, provide the credentials or proxy it the original
  // URLRequest::Delegate.

  waiting_for_user_response_ =
      WaitingForUserResponse::WAITING_FOR_AUTH_RESPONSE;

  std::unique_ptr<protocol::Network::Request> network_request =
      protocol::NetworkHandler::CreateRequestFromURLRequest(this->request());
  std::unique_ptr<protocol::Network::AuthChallenge> auth_challenge =
      protocol::Network::AuthChallenge::Create()
          .SetSource(auth_info->is_proxy
                         ? protocol::Network::AuthChallenge::SourceEnum::Proxy
                         : protocol::Network::AuthChallenge::SourceEnum::Server)
          .SetOrigin(auth_info->challenger.Serialize())
          .SetScheme(auth_info->scheme)
          .SetRealm(auth_info->realm)
          .Build();

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(SendAuthRequiredEventOnUiThread, network_handler_,
                 interception_id_, base::Passed(&network_request),
                 resource_type_, base::Passed(&auth_challenge)));
}

void DevToolsURLInterceptorRequestJob::OnCertificateRequested(
    net::URLRequest* request,
    net::SSLCertRequestInfo* cert_request_info) {
  DCHECK(sub_request_);
  DCHECK_EQ(request, sub_request_->request());
  NotifyCertificateRequested(cert_request_info);
}

void DevToolsURLInterceptorRequestJob::OnSSLCertificateError(
    net::URLRequest* request,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  DCHECK(sub_request_);
  DCHECK_EQ(request, sub_request_->request());
  NotifySSLCertificateError(ssl_info, fatal);
}

void DevToolsURLInterceptorRequestJob::OnResponseStarted(
    net::URLRequest* request,
    int net_error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(sub_request_);
  DCHECK_EQ(request, sub_request_->request());
  DCHECK_NE(net::ERR_IO_PENDING, net_error);

  if (net_error != net::OK) {
    sub_request_->Cancel();

    NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                           static_cast<net::Error>(net_error)));
    return;
  }

  NotifyHeadersComplete();
}

void DevToolsURLInterceptorRequestJob::OnReadCompleted(net::URLRequest* request,
                                                       int num_bytes) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(sub_request_);
  DCHECK_EQ(request, sub_request_->request());

  // OnReadCompleted may get called while canceling the subrequest, in that
  // event theres no need to call ReadRawDataComplete.
  if (!killed_)
    ReadRawDataComplete(num_bytes);
}

void DevToolsURLInterceptorRequestJob::OnReceivedRedirect(
    net::URLRequest* request,
    const net::RedirectInfo& redirectinfo,
    bool* defer_redirect) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(sub_request_);
  DCHECK_EQ(request, sub_request_->request());
  // If we're not intercepting results then just exit.
  if (!intercepting_requests_) {
    *defer_redirect = false;
    return;
  }

  // Otherwise we will need to ask what to do via DevTools protocol.
  *defer_redirect = true;

  size_t iter = 0;
  std::string header_name;
  std::string header_value;
  std::unique_ptr<protocol::DictionaryValue> headers_dict(
      protocol::DictionaryValue::create());
  while (request->response_headers()->EnumerateHeaderLines(&iter, &header_name,
                                                           &header_value)) {
    headers_dict->setString(header_name, header_value);
  }

  redirect_.reset(new net::RedirectInfo(redirectinfo));
  sub_request_->Cancel();
  sub_request_.reset();

  waiting_for_user_response_ =
      WaitingForUserResponse::WAITING_FOR_INTERCEPTION_RESPONSE;

  std::unique_ptr<protocol::Network::Request> network_request =
      protocol::NetworkHandler::CreateRequestFromURLRequest(this->request());
  std::unique_ptr<protocol::Object> headers_object =
      protocol::Object::fromValue(headers_dict.get(), nullptr);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(SendRedirectInterceptedEventOnUiThread, network_handler_,
                 interception_id_, base::Passed(&network_request),
                 resource_type_, base::Passed(&headers_object),
                 redirectinfo.status_code, redirectinfo.new_url.spec()));
}

void DevToolsURLInterceptorRequestJob::StopIntercepting() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  intercepting_requests_ = false;

  // Allow the request to continue if we're waiting for user input.
  switch (waiting_for_user_response_) {
    case WaitingForUserResponse::NOT_WAITING:
      return;

    case WaitingForUserResponse::WAITING_FOR_INTERCEPTION_RESPONSE:
      ProcessInterceptionRespose(
          base::MakeUnique<DevToolsURLRequestInterceptor::Modifications>(
              base::nullopt, base::nullopt, protocol::Maybe<std::string>(),
              protocol::Maybe<std::string>(), protocol::Maybe<std::string>(),
              protocol::Maybe<protocol::Network::Headers>(),
              protocol::Maybe<protocol::Network::AuthChallengeResponse>()));
      return;

    case WaitingForUserResponse::WAITING_FOR_AUTH_RESPONSE: {
      std::unique_ptr<protocol::Network::AuthChallengeResponse> auth_response =
          protocol::Network::AuthChallengeResponse::Create()
              .SetResponse(protocol::Network::AuthChallengeResponse::
                               ResponseEnum::Default)
              .Build();
      ProcessAuthRespose(
          base::MakeUnique<DevToolsURLRequestInterceptor::Modifications>(
              base::nullopt, base::nullopt, protocol::Maybe<std::string>(),
              protocol::Maybe<std::string>(), protocol::Maybe<std::string>(),
              protocol::Maybe<protocol::Network::Headers>(),
              std::move(auth_response)));
      return;
    }

    default:
      NOTREACHED();
      return;
  }
}

void DevToolsURLInterceptorRequestJob::ContinueInterceptedRequest(
    std::unique_ptr<DevToolsURLRequestInterceptor::Modifications> modifications,
    std::unique_ptr<ContinueInterceptedRequestCallback> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  switch (waiting_for_user_response_) {
    case WaitingForUserResponse::NOT_WAITING:
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::BindOnce(&ContinueInterceptedRequestCallback::sendFailure,
                         base::Passed(std::move(callback)),
                         protocol::Response::InvalidParams(
                             "Response already processed.")));
      break;

    case WaitingForUserResponse::WAITING_FOR_INTERCEPTION_RESPONSE:
      if (modifications->auth_challenge_response.isJust()) {
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            base::BindOnce(&ContinueInterceptedRequestCallback::sendFailure,
                           base::Passed(std::move(callback)),
                           protocol::Response::InvalidParams(
                               "authChallengeResponse not expected.")));
        break;
      }
      ProcessInterceptionRespose(std::move(modifications));
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::BindOnce(&ContinueInterceptedRequestCallback::sendSuccess,
                         base::Passed(std::move(callback))));
      break;

    case WaitingForUserResponse::WAITING_FOR_AUTH_RESPONSE:
      if (!modifications->auth_challenge_response.isJust()) {
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            base::BindOnce(&ContinueInterceptedRequestCallback::sendFailure,
                           base::Passed(std::move(callback)),
                           protocol::Response::InvalidParams(
                               "authChallengeResponse required.")));
        break;
      }
      if (ProcessAuthRespose(std::move(modifications))) {
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            base::BindOnce(&ContinueInterceptedRequestCallback::sendSuccess,
                           base::Passed(std::move(callback))));
      } else {
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            base::BindOnce(&ContinueInterceptedRequestCallback::sendFailure,
                           base::Passed(std::move(callback)),
                           protocol::Response::InvalidParams(
                               "Unrecognized authChallengeResponse.")));
      }
      break;

    default:
      NOTREACHED();
      break;
  }
}

void DevToolsURLInterceptorRequestJob::ProcessInterceptionRespose(
    std::unique_ptr<DevToolsURLRequestInterceptor::Modifications>
        modifications) {
  waiting_for_user_response_ = WaitingForUserResponse::NOT_WAITING;

  if (modifications->error_reason) {
    NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                           *modifications->error_reason));
    return;
  }

  if (modifications->raw_response) {
    mock_response_details_.reset(new MockResponseDetails(
        std::move(*modifications->raw_response), base::TimeTicks::Now()));

    std::string value;
    if (mock_response_details_->response_headers()->IsRedirect(&value)) {
      devtools_url_request_interceptor_state_->ExpectRequestAfterRedirect(
          request(), interception_id_);
    }
    NotifyHeadersComplete();
    return;
  }

  if (redirect_) {
    // NOTE we don't append the text form of the status code because
    // net::HttpResponseHeaders doesn't need that.
    std::string raw_headers =
        base::StringPrintf("HTTP/1.1 %d", redirect_->status_code);
    raw_headers.append(1, '\0');
    raw_headers.append("Location: ");
    raw_headers.append(
        modifications->modified_url.fromMaybe(redirect_->new_url.spec()));
    raw_headers.append(2, '\0');
    mock_response_details_.reset(new MockResponseDetails(
        make_scoped_refptr(new net::HttpResponseHeaders(raw_headers)), "", 0,
        base::TimeTicks::Now()));
    redirect_.reset();

    devtools_url_request_interceptor_state_->ExpectRequestAfterRedirect(
        request(), interception_id_);
    NotifyHeadersComplete();
  } else {
    // Note this redirect is not visible to the caller by design. If they want a
    // visible redirect they can mock a response with a 302.
    if (modifications->modified_url.isJust())
      request_details_.url = GURL(modifications->modified_url.fromJust());

    if (modifications->modified_method.isJust())
      request_details_.method = modifications->modified_method.fromJust();

    if (modifications->modified_post_data.isJust()) {
      const std::string& post_data =
          modifications->modified_post_data.fromJust();
      std::vector<char> data(post_data.begin(), post_data.end());
      request_details_.post_data =
          net::ElementsUploadDataStream::CreateWithReader(
              base::MakeUnique<net::UploadOwnedBytesElementReader>(&data), 0);
    }

    if (modifications->modified_headers.isJust()) {
      request_details_.extra_request_headers.Clear();
      std::unique_ptr<protocol::DictionaryValue> headers =
          modifications->modified_headers.fromJust()->toValue();
      for (size_t i = 0; i < headers->size(); i++) {
        std::string value;
        if (headers->at(i).second->asString(&value)) {
          request_details_.extra_request_headers.SetHeader(headers->at(i).first,
                                                           value);
        }
      }
    }

    // The reason we start a sub request is because we are in full control of it
    // and can choose to ignore it if, for example, the fetch encounters a
    // redirect that the user chooses to replace with a mock response.
    sub_request_.reset(new SubRequest(request_details_, this,
                                      devtools_url_request_interceptor_state_));
  }
}

bool DevToolsURLInterceptorRequestJob::ProcessAuthRespose(
    std::unique_ptr<DevToolsURLRequestInterceptor::Modifications>
        modifications) {
  waiting_for_user_response_ = WaitingForUserResponse::NOT_WAITING;

  protocol::Network::AuthChallengeResponse* auth_challenge_response =
      modifications->auth_challenge_response.fromJust();

  if (auth_challenge_response->GetResponse() ==
      protocol::Network::AuthChallengeResponse::ResponseEnum::Default) {
    // The user wants the default behavior, we must proxy the auth request to
    // the original URLRequest::Delegate.  We can't do that directly but by
    // implementing NeedsAuth and calling NotifyHeadersComplete we trigger it.
    // To close the loop we also need to implement GetAuthChallengeInfo, SetAuth
    // and CancelAuth.
    NotifyHeadersComplete();
    return true;
  }

  if (auth_challenge_response->GetResponse() ==
      protocol::Network::AuthChallengeResponse::ResponseEnum::CancelAuth) {
    CancelAuth();
    return true;
  }

  if (auth_challenge_response->GetResponse() ==
      protocol::Network::AuthChallengeResponse::ResponseEnum::
          ProvideCredentials) {
    SetAuth(net::AuthCredentials(
        base::UTF8ToUTF16(auth_challenge_response->GetUsername("").c_str()),
        base::UTF8ToUTF16(auth_challenge_response->GetPassword("").c_str())));
    return true;
  }

  return false;
}

DevToolsURLInterceptorRequestJob::RequestDetails::RequestDetails(
    const GURL& url,
    const std::string& method,
    std::unique_ptr<net::UploadDataStream> post_data,
    const net::HttpRequestHeaders& extra_request_headers,
    const net::RequestPriority& priority,
    const net::URLRequestContext* url_request_context)
    : url(url),
      method(method),
      post_data(std::move(post_data)),
      extra_request_headers(extra_request_headers),
      priority(priority),
      url_request_context(url_request_context) {}

DevToolsURLInterceptorRequestJob::RequestDetails::~RequestDetails() {}

DevToolsURLInterceptorRequestJob::SubRequest::SubRequest(
    DevToolsURLInterceptorRequestJob::RequestDetails& request_details,
    DevToolsURLInterceptorRequestJob* devtools_interceptor_request_job,
    scoped_refptr<DevToolsURLRequestInterceptor::State>
        devtools_url_request_interceptor_state)
    : devtools_interceptor_request_job_(devtools_interceptor_request_job),
      devtools_url_request_interceptor_state_(
          devtools_url_request_interceptor_state),
      fetch_in_progress_(true) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("devtools_interceptor", R"(
        semantics {
          sender: "Developer Tools"
          description:
            "When user is debugging a page, all actions resulting in a network "
            "request are intercepted to enrich the debugging experience."
          trigger:
            "User triggers an action that requires network request (like "
            "navigation, download, etc.) while debugging the page."
          data:
            "Any data that user action sends."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "This feature cannot be disabled in settings, however it happens "
            "only when user is debugging a page."
          chrome_policy {
            DeveloperToolsDisabled {
              DeveloperToolsDisabled: true
            }
          }
        })");
  request_ = request_details.url_request_context->CreateRequest(
      request_details.url, request_details.priority,
      devtools_interceptor_request_job_, traffic_annotation),
  request_->set_method(request_details.method);
  request_->SetExtraRequestHeaders(request_details.extra_request_headers);

  // Mimic the ResourceRequestInfoImpl of the original request.
  const ResourceRequestInfoImpl* resource_request_info =
      static_cast<const ResourceRequestInfoImpl*>(
          ResourceRequestInfo::ForRequest(
              devtools_interceptor_request_job->request()));
  ResourceRequestInfoImpl* extra_data = new ResourceRequestInfoImpl(
      resource_request_info->requester_info(),
      resource_request_info->GetRouteID(),
      resource_request_info->GetFrameTreeNodeId(),
      resource_request_info->GetOriginPID(),
      resource_request_info->GetRequestID(),
      resource_request_info->GetRenderFrameID(),
      resource_request_info->IsMainFrame(),
      resource_request_info->ParentIsMainFrame(),
      resource_request_info->GetResourceType(),
      resource_request_info->GetPageTransition(),
      resource_request_info->should_replace_current_entry(),
      resource_request_info->IsDownload(), resource_request_info->is_stream(),
      resource_request_info->allow_download(),
      resource_request_info->HasUserGesture(),
      resource_request_info->is_load_timing_enabled(),
      resource_request_info->is_upload_progress_enabled(),
      resource_request_info->do_not_prompt_for_login(),
      resource_request_info->GetReferrerPolicy(),
      resource_request_info->GetVisibilityState(),
      resource_request_info->GetContext(),
      resource_request_info->ShouldReportRawHeaders(),
      resource_request_info->IsAsync(),
      resource_request_info->GetPreviewsState(), resource_request_info->body(),
      resource_request_info->initiated_in_secure_context());
  extra_data->AssociateWithRequest(request_.get());

  if (request_details.post_data)
    request_->set_upload(std::move(request_details.post_data));

  devtools_url_request_interceptor_state_->RegisterSubRequest(request_.get());
  request_->Start();
}

DevToolsURLInterceptorRequestJob::SubRequest::~SubRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  devtools_url_request_interceptor_state_->UnregisterSubRequest(request_.get());
}

void DevToolsURLInterceptorRequestJob::SubRequest::Cancel() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!fetch_in_progress_)
    return;

  fetch_in_progress_ = false;
  request_->Cancel();
}

DevToolsURLInterceptorRequestJob::MockResponseDetails::MockResponseDetails(
    std::string response_bytes,
    base::TimeTicks response_time)
    : response_bytes_(std::move(response_bytes)),
      read_offset_(0),
      response_time_(response_time) {
  int header_size = net::HttpUtil::LocateEndOfHeaders(response_bytes_.c_str(),
                                                      response_bytes_.size());
  if (header_size == -1) {
    LOG(WARNING) << "Can't find headers in result";
    response_headers_ = new net::HttpResponseHeaders("");
  } else {
    response_headers_ =
        new net::HttpResponseHeaders(net::HttpUtil::AssembleRawHeaders(
            response_bytes_.c_str(), header_size));
    read_offset_ = header_size;
  }

  CHECK_LE(read_offset_, response_bytes_.size());
}

DevToolsURLInterceptorRequestJob::MockResponseDetails::MockResponseDetails(
    const scoped_refptr<net::HttpResponseHeaders>& response_headers,
    std::string response_bytes,
    size_t read_offset,
    base::TimeTicks response_time)
    : response_headers_(response_headers),
      response_bytes_(std::move(response_bytes)),
      read_offset_(read_offset),
      response_time_(response_time) {}

DevToolsURLInterceptorRequestJob::MockResponseDetails::~MockResponseDetails() {}

int DevToolsURLInterceptorRequestJob::MockResponseDetails::ReadRawData(
    net::IOBuffer* buf,
    int buf_size) {
  size_t bytes_available = response_bytes_.size() - read_offset_;
  size_t bytes_to_copy =
      std::min(static_cast<size_t>(buf_size), bytes_available);
  if (bytes_to_copy > 0) {
    std::memcpy(buf->data(), &response_bytes_.data()[read_offset_],
                bytes_to_copy);
    read_offset_ += bytes_to_copy;
  }
  return bytes_to_copy;
}

}  // namespace content
