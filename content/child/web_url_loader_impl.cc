// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/web_url_loader_impl.h"

#include <algorithm>
#include <deque>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "content/child/child_thread_impl.h"
#include "content/child/ftp_directory_listing_response_delegate.h"
#include "content/child/multipart_response_delegate.h"
#include "content/child/request_extra_data.h"
#include "content/child/request_info.h"
#include "content/child/resource_dispatcher.h"
#include "content/child/sync_load_response.h"
#include "content/child/web_data_consumer_handle_impl.h"
#include "content/child/web_url_request_util.h"
#include "content/child/weburlresponse_extradata_impl.h"
#include "content/common/resource_messages.h"
#include "content/common/resource_request_body.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/child/request_peer.h"
#include "content/public/common/content_switches.h"
#include "net/base/data_url.h"
#include "net/base/filename_util.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request_data_job.h"
#include "third_party/WebKit/public/platform/WebHTTPLoadInfo.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLLoadTiming.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "third_party/mojo/src/mojo/public/cpp/system/data_pipe.h"

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

const char kThrottledErrorDescription[] =
    "Request throttled. Visit http://dev.chromium.org/throttling for more "
    "information.";
const size_t kBodyStreamPipeCapacity = 4 * 1024;

typedef ResourceDevToolsInfo::HeadersVector HeadersVector;

// Converts timing data from |load_timing| to the format used by WebKit.
void PopulateURLLoadTiming(const net::LoadTimingInfo& load_timing,
                           WebURLLoadTiming* url_timing) {
  DCHECK(!load_timing.request_start.is_null());

  const TimeTicks kNullTicks;
  url_timing->initialize();
  url_timing->setRequestTime(
      (load_timing.request_start - kNullTicks).InSecondsF());
  url_timing->setProxyStart(
      (load_timing.proxy_resolve_start - kNullTicks).InSecondsF());
  url_timing->setProxyEnd(
      (load_timing.proxy_resolve_end - kNullTicks).InSecondsF());
  url_timing->setDNSStart(
      (load_timing.connect_timing.dns_start - kNullTicks).InSecondsF());
  url_timing->setDNSEnd(
      (load_timing.connect_timing.dns_end - kNullTicks).InSecondsF());
  url_timing->setConnectStart(
      (load_timing.connect_timing.connect_start - kNullTicks).InSecondsF());
  url_timing->setConnectEnd(
      (load_timing.connect_timing.connect_end - kNullTicks).InSecondsF());
  url_timing->setSSLStart(
      (load_timing.connect_timing.ssl_start - kNullTicks).InSecondsF());
  url_timing->setSSLEnd(
      (load_timing.connect_timing.ssl_end - kNullTicks).InSecondsF());
  url_timing->setSendStart(
      (load_timing.send_start - kNullTicks).InSecondsF());
  url_timing->setSendEnd(
      (load_timing.send_end - kNullTicks).InSecondsF());
  url_timing->setReceiveHeadersEnd(
      (load_timing.receive_headers_end - kNullTicks).InSecondsF());
}

net::RequestPriority ConvertWebKitPriorityToNetPriority(
    const WebURLRequest::Priority& priority) {
  switch (priority) {
    case WebURLRequest::PriorityVeryHigh:
      return net::HIGHEST;

    case WebURLRequest::PriorityHigh:
      return net::MEDIUM;

    case WebURLRequest::PriorityMedium:
      return net::LOW;

    case WebURLRequest::PriorityLow:
      return net::LOWEST;

    case WebURLRequest::PriorityVeryLow:
      return net::IDLE;

    case WebURLRequest::PriorityUnresolved:
    default:
      NOTREACHED();
      return net::LOW;
  }
}

// Extracts info from a data scheme URL into |info| and |data|. Returns net::OK
// if successful. Returns a net error code otherwise. Exported only for testing.
int GetInfoFromDataURL(const GURL& url,
                       ResourceResponseInfo* info,
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
  info->security_info.clear();
  info->content_length = data->length();
  info->encoded_data_length = 0;

  return net::OK;
}

#define STATIC_ASSERT_MATCHING_ENUMS(content_name, blink_name)       \
  static_assert(                                                     \
      static_cast<int>(content_name) == static_cast<int>(blink_name), \
      "mismatching enums: " #content_name)

STATIC_ASSERT_MATCHING_ENUMS(FETCH_REQUEST_MODE_SAME_ORIGIN,
                             WebURLRequest::FetchRequestModeSameOrigin);
STATIC_ASSERT_MATCHING_ENUMS(FETCH_REQUEST_MODE_NO_CORS,
                             WebURLRequest::FetchRequestModeNoCORS);
STATIC_ASSERT_MATCHING_ENUMS(FETCH_REQUEST_MODE_CORS,
                             WebURLRequest::FetchRequestModeCORS);
STATIC_ASSERT_MATCHING_ENUMS(
    FETCH_REQUEST_MODE_CORS_WITH_FORCED_PREFLIGHT,
    WebURLRequest::FetchRequestModeCORSWithForcedPreflight);

FetchRequestMode GetFetchRequestMode(const WebURLRequest& request) {
  return static_cast<FetchRequestMode>(request.fetchRequestMode());
}

STATIC_ASSERT_MATCHING_ENUMS(FETCH_CREDENTIALS_MODE_OMIT,
                             WebURLRequest::FetchCredentialsModeOmit);
STATIC_ASSERT_MATCHING_ENUMS(FETCH_CREDENTIALS_MODE_SAME_ORIGIN,
                             WebURLRequest::FetchCredentialsModeSameOrigin);
STATIC_ASSERT_MATCHING_ENUMS(FETCH_CREDENTIALS_MODE_INCLUDE,
                             WebURLRequest::FetchCredentialsModeInclude);

FetchCredentialsMode GetFetchCredentialsMode(const WebURLRequest& request) {
  return static_cast<FetchCredentialsMode>(request.fetchCredentialsMode());
}

STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_FRAME_TYPE_AUXILIARY,
                             WebURLRequest::FrameTypeAuxiliary);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_FRAME_TYPE_NESTED,
                             WebURLRequest::FrameTypeNested);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_FRAME_TYPE_NONE,
                             WebURLRequest::FrameTypeNone);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_FRAME_TYPE_TOP_LEVEL,
                             WebURLRequest::FrameTypeTopLevel);

RequestContextFrameType GetRequestContextFrameType(
    const WebURLRequest& request) {
  return static_cast<RequestContextFrameType>(request.frameType());
}

STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_UNSPECIFIED,
                             WebURLRequest::RequestContextUnspecified);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_AUDIO,
                             WebURLRequest::RequestContextAudio);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_BEACON,
                             WebURLRequest::RequestContextBeacon);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_CSP_REPORT,
                             WebURLRequest::RequestContextCSPReport);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_DOWNLOAD,
                             WebURLRequest::RequestContextDownload);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_EMBED,
                             WebURLRequest::RequestContextEmbed);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_EVENT_SOURCE,
                             WebURLRequest::RequestContextEventSource);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_FAVICON,
                             WebURLRequest::RequestContextFavicon);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_FETCH,
                             WebURLRequest::RequestContextFetch);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_FONT,
                             WebURLRequest::RequestContextFont);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_FORM,
                             WebURLRequest::RequestContextForm);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_FRAME,
                             WebURLRequest::RequestContextFrame);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_HYPERLINK,
                             WebURLRequest::RequestContextHyperlink);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_IFRAME,
                             WebURLRequest::RequestContextIframe);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_IMAGE,
                             WebURLRequest::RequestContextImage);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_IMAGE_SET,
                             WebURLRequest::RequestContextImageSet);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_IMPORT,
                             WebURLRequest::RequestContextImport);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_INTERNAL,
                             WebURLRequest::RequestContextInternal);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_LOCATION,
                             WebURLRequest::RequestContextLocation);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_MANIFEST,
                             WebURLRequest::RequestContextManifest);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_OBJECT,
                             WebURLRequest::RequestContextObject);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_PING,
                             WebURLRequest::RequestContextPing);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_PLUGIN,
                             WebURLRequest::RequestContextPlugin);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_PREFETCH,
                             WebURLRequest::RequestContextPrefetch);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_SCRIPT,
                             WebURLRequest::RequestContextScript);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_SERVICE_WORKER,
                             WebURLRequest::RequestContextServiceWorker);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_SHARED_WORKER,
                             WebURLRequest::RequestContextSharedWorker);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_SUBRESOURCE,
                             WebURLRequest::RequestContextSubresource);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_STYLE,
                             WebURLRequest::RequestContextStyle);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_TRACK,
                             WebURLRequest::RequestContextTrack);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_VIDEO,
                             WebURLRequest::RequestContextVideo);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_WORKER,
                             WebURLRequest::RequestContextWorker);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_XML_HTTP_REQUEST,
                             WebURLRequest::RequestContextXMLHttpRequest);
STATIC_ASSERT_MATCHING_ENUMS(REQUEST_CONTEXT_TYPE_XSLT,
                             WebURLRequest::RequestContextXSLT);

RequestContextType GetRequestContextType(const WebURLRequest& request) {
  return static_cast<RequestContextType>(request.requestContext());
}

}  // namespace

// WebURLLoaderImpl::Context --------------------------------------------------

// This inner class exists since the WebURLLoader may be deleted while inside a
// call to WebURLLoaderClient.  Refcounting is to keep the context from being
// deleted if it may have work to do after calling into the client.
class WebURLLoaderImpl::Context : public base::RefCounted<Context>,
                                  public RequestPeer {
 public:
  Context(WebURLLoaderImpl* loader,
          ResourceDispatcher* resource_dispatcher,
          scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  WebURLLoaderClient* client() const { return client_; }
  void set_client(WebURLLoaderClient* client) { client_ = client; }

  void Cancel();
  void SetDefersLoading(bool value);
  void DidChangePriority(WebURLRequest::Priority new_priority,
                         int intra_priority_value);
  bool AttachThreadedDataReceiver(
      blink::WebThreadedDataReceiver* threaded_data_receiver);
  void Start(const WebURLRequest& request,
             SyncLoadResponse* sync_load_response);

  // RequestPeer methods:
  void OnUploadProgress(uint64 position, uint64 size) override;
  bool OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                          const ResourceResponseInfo& info) override;
  void OnReceivedResponse(const ResourceResponseInfo& info) override;
  void OnDownloadedData(int len, int encoded_data_length) override;
  void OnReceivedData(const char* data,
                      int data_length,
                      int encoded_data_length) override;
  void OnReceivedCachedMetadata(const char* data, int len) override;
  void OnCompletedRequest(int error_code,
                          bool was_ignored_by_handler,
                          bool stale_copy_in_cache,
                          const std::string& security_info,
                          const base::TimeTicks& completion_time,
                          int64 total_transfer_size) override;

 private:
  friend class base::RefCounted<Context>;
  ~Context() override;

  // We can optimize the handling of data URLs in most cases.
  bool CanHandleDataURLRequestLocally() const;
  void HandleDataURL();
  MojoResult WriteDataOnBodyStream(const char* data, size_t size);
  void OnHandleGotWritable(MojoResult);

  WebURLLoaderImpl* loader_;
  WebURLRequest request_;
  WebURLLoaderClient* client_;
  ResourceDispatcher* resource_dispatcher_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  WebReferrerPolicy referrer_policy_;
  scoped_ptr<FtpDirectoryListingResponseDelegate> ftp_listing_delegate_;
  scoped_ptr<MultipartResponseDelegate> multipart_delegate_;
  scoped_ptr<StreamOverrideParameters> stream_override_;
  mojo::ScopedDataPipeProducerHandle body_stream_writer_;
  mojo::common::HandleWatcher body_stream_writer_watcher_;
  // TODO(yhirano): Delete this buffer after implementing the back-pressure
  // mechanism.
  std::deque<char> body_stream_buffer_;
  bool got_all_stream_body_data_;
  enum DeferState {NOT_DEFERRING, SHOULD_DEFER, DEFERRED_DATA};
  DeferState defers_loading_;
  int request_id_;
};

WebURLLoaderImpl::Context::Context(
    WebURLLoaderImpl* loader,
    ResourceDispatcher* resource_dispatcher,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : loader_(loader),
      client_(NULL),
      resource_dispatcher_(resource_dispatcher),
      task_runner_(task_runner),
      referrer_policy_(blink::WebReferrerPolicyDefault),
      got_all_stream_body_data_(false),
      defers_loading_(NOT_DEFERRING),
      request_id_(-1) {
}

void WebURLLoaderImpl::Context::Cancel() {
  if (resource_dispatcher_ && // NULL in unittest.
      request_id_ != -1) {
    resource_dispatcher_->Cancel(request_id_);
    request_id_ = -1;
  }

  // Ensure that we do not notify the multipart delegate anymore as it has
  // its own pointer to the client.
  if (multipart_delegate_)
    multipart_delegate_->Cancel();
  // Ditto for the ftp delegate.
  if (ftp_listing_delegate_)
    ftp_listing_delegate_->Cancel();

  // Do not make any further calls to the client.
  client_ = NULL;
  loader_ = NULL;
}

void WebURLLoaderImpl::Context::SetDefersLoading(bool value) {
  if (request_id_ != -1)
    resource_dispatcher_->SetDefersLoading(request_id_, value);
  if (value && defers_loading_ == NOT_DEFERRING) {
    defers_loading_ = SHOULD_DEFER;
  } else if (!value && defers_loading_ != NOT_DEFERRING) {
    if (defers_loading_ == DEFERRED_DATA) {
      task_runner_->PostTask(FROM_HERE,
                             base::Bind(&Context::HandleDataURL, this));
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

bool WebURLLoaderImpl::Context::AttachThreadedDataReceiver(
    blink::WebThreadedDataReceiver* threaded_data_receiver) {
  if (request_id_ != -1) {
    return resource_dispatcher_->AttachThreadedDataReceiver(
        request_id_, threaded_data_receiver);
  }

  return false;
}

void WebURLLoaderImpl::Context::Start(const WebURLRequest& request,
                                      SyncLoadResponse* sync_load_response) {
  DCHECK(request_id_ == -1);
  request_ = request;  // Save the request.
  if (request.extraData()) {
    RequestExtraData* extra_data =
        static_cast<RequestExtraData*>(request.extraData());
    stream_override_ = extra_data->TakeStreamOverrideOwnership();
  }

  GURL url = request.url();

  // PlzNavigate: during navigation, the renderer should request a stream which
  // contains the body of the response. The request has already been made by the
  // browser.
  if (stream_override_.get()) {
    CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableBrowserSideNavigation));
    DCHECK(!sync_load_response);
    DCHECK_NE(WebURLRequest::FrameTypeNone, request.frameType());
    DCHECK_EQ("GET", request.httpMethod().latin1());
    url = stream_override_->stream_url;
  }

  // PlzNavigate: the only navigation requests going through the WebURLLoader
  // are the ones created by CommitNavigation.
  DCHECK(!base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kEnableBrowserSideNavigation) ||
         stream_override_.get() ||
         request.frameType() == WebURLRequest::FrameTypeNone);

  if (CanHandleDataURLRequestLocally()) {
    if (sync_load_response) {
      // This is a sync load. Do the work now.
      sync_load_response->url = url;
      sync_load_response->error_code =
          GetInfoFromDataURL(sync_load_response->url, sync_load_response,
                             &sync_load_response->data);
    } else {
      task_runner_->PostTask(FROM_HERE,
                             base::Bind(&Context::HandleDataURL, this));
    }
    return;
  }

  GURL referrer_url(
      request.httpHeaderField(WebString::fromUTF8("Referer")).latin1());
  const std::string& method = request.httpMethod().latin1();

  // TODO(brettw) this should take parameter encoding into account when
  // creating the GURLs.

  // TODO(horo): Check credentials flag is unset when credentials mode is omit.
  //             Check credentials flag is set when credentials mode is include.

  RequestInfo request_info;
  request_info.method = method;
  request_info.url = url;
  request_info.first_party_for_cookies = request.firstPartyForCookies();
  referrer_policy_ = request.referrerPolicy();
  request_info.referrer = Referrer(referrer_url, referrer_policy_);
  request_info.headers = GetWebURLRequestHeaders(request);
  request_info.load_flags = GetLoadFlagsForWebURLRequest(request);
  request_info.enable_load_timing = true;
  request_info.enable_upload_progress = request.reportUploadProgress();
  if (request.requestContext() == WebURLRequest::RequestContextXMLHttpRequest &&
      (url.has_username() || url.has_password())) {
    request_info.do_not_prompt_for_login = true;
  }
  // requestor_pid only needs to be non-zero if the request originates outside
  // the render process, so we can use requestorProcessID even for requests
  // from in-process plugins.
  request_info.requestor_pid = request.requestorProcessID();
  request_info.request_type = WebURLRequestToResourceType(request);
  request_info.priority =
      ConvertWebKitPriorityToNetPriority(request.priority());
  request_info.appcache_host_id = request.appCacheHostID();
  request_info.routing_id = request.requestorID();
  request_info.download_to_file = request.downloadToFile();
  request_info.has_user_gesture = request.hasUserGesture();
  request_info.skip_service_worker = request.skipServiceWorker();
  request_info.should_reset_appcache = request.shouldResetAppCache();
  request_info.fetch_request_mode = GetFetchRequestMode(request);
  request_info.fetch_credentials_mode = GetFetchCredentialsMode(request);
  request_info.fetch_request_context_type = GetRequestContextType(request);
  request_info.fetch_frame_type = GetRequestContextFrameType(request);
  request_info.extra_data = request.extraData();

  scoped_refptr<ResourceRequestBody> request_body =
      GetRequestBodyForWebURLRequest(request).get();

  if (sync_load_response) {
    resource_dispatcher_->StartSync(
        request_info, request_body.get(), sync_load_response);
    return;
  }

  request_id_ = resource_dispatcher_->StartAsync(
      request_info, request_body.get(), this);
}

void WebURLLoaderImpl::Context::OnUploadProgress(uint64 position, uint64 size) {
  if (client_)
    client_->didSendData(loader_, position, size);
}

bool WebURLLoaderImpl::Context::OnReceivedRedirect(
    const net::RedirectInfo& redirect_info,
    const ResourceResponseInfo& info) {
  if (!client_)
    return false;

  WebURLResponse response;
  response.initialize();
  PopulateURLResponse(request_.url(), info, &response);

  // TODO(darin): We lack sufficient information to construct the actual
  // request that resulted from the redirect.
  WebURLRequest new_request(redirect_info.new_url);
  new_request.setFirstPartyForCookies(
      redirect_info.new_first_party_for_cookies);
  new_request.setDownloadToFile(request_.downloadToFile());
  new_request.setRequestContext(request_.requestContext());
  new_request.setFrameType(request_.frameType());
  new_request.setSkipServiceWorker(request_.skipServiceWorker());
  new_request.setShouldResetAppCache(request_.shouldResetAppCache());
  new_request.setFetchRequestMode(request_.fetchRequestMode());
  new_request.setFetchCredentialsMode(request_.fetchCredentialsMode());

  new_request.setHTTPReferrer(WebString::fromUTF8(redirect_info.new_referrer),
                              referrer_policy_);

  std::string old_method = request_.httpMethod().utf8();
  new_request.setHTTPMethod(WebString::fromUTF8(redirect_info.new_method));
  if (redirect_info.new_method == old_method)
    new_request.setHTTPBody(request_.httpBody());

  // Protect from deletion during call to willSendRequest.
  scoped_refptr<Context> protect(this);

  client_->willSendRequest(loader_, new_request, response);
  request_ = new_request;

  // Only follow the redirect if WebKit left the URL unmodified.
  if (redirect_info.new_url == GURL(new_request.url())) {
    // First-party cookie logic moved from DocumentLoader in Blink to
    // net::URLRequest in the browser. Assert that Blink didn't try to change it
    // to something else.
    DCHECK_EQ(redirect_info.new_first_party_for_cookies.spec(),
              request_.firstPartyForCookies().string().utf8());
    return true;
  }

  // We assume that WebKit only changes the URL to suppress a redirect, and we
  // assume that it does so by setting it to be invalid.
  DCHECK(!new_request.url().isValid());
  return false;
}

void WebURLLoaderImpl::Context::OnReceivedResponse(
    const ResourceResponseInfo& initial_info) {
  if (!client_)
    return;

  ResourceResponseInfo info = initial_info;

  // PlzNavigate: during navigations, the ResourceResponse has already been
  // received on the browser side, and has been passed down to the renderer.
  if (stream_override_.get()) {
    CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableBrowserSideNavigation));
    info = stream_override_->response;
  }

  WebURLResponse response;
  response.initialize();
  PopulateURLResponse(request_.url(), info, &response);

  bool show_raw_listing = (GURL(request_.url()).query() == "raw");

  if (info.mime_type == "text/vnd.chromium.ftp-dir") {
    if (show_raw_listing) {
      // Set the MIME type to plain text to prevent any active content.
      response.setMIMEType("text/plain");
    } else {
      // We're going to produce a parsed listing in HTML.
      response.setMIMEType("text/html");
    }
  }

  // Prevent |this| from being destroyed if the client destroys the loader,
  // ether in didReceiveResponse, or when the multipart/ftp delegate calls into
  // it.
  scoped_refptr<Context> protect(this);

  if (request_.useStreamOnResponse()) {
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = kBodyStreamPipeCapacity;

    mojo::ScopedDataPipeConsumerHandle consumer;
    MojoResult result = mojo::CreateDataPipe(&options,
                                             &body_stream_writer_,
                                             &consumer);
    if (result != MOJO_RESULT_OK) {
      // TODO(yhirano): Handle the error.
      return;
    }
    client_->didReceiveResponse(
        loader_, response, new WebDataConsumerHandleImpl(consumer.Pass()));
  } else {
    client_->didReceiveResponse(loader_, response);
  }

  // We may have been cancelled after didReceiveResponse, which would leave us
  // without a client and therefore without much need to do further handling.
  if (!client_)
    return;

  DCHECK(!ftp_listing_delegate_.get());
  DCHECK(!multipart_delegate_.get());
  if (info.headers.get() && info.mime_type == "multipart/x-mixed-replace") {
    std::string content_type;
    info.headers->EnumerateHeader(NULL, "content-type", &content_type);

    std::string mime_type;
    std::string charset;
    bool had_charset = false;
    std::string boundary;
    net::HttpUtil::ParseContentType(content_type, &mime_type, &charset,
                                    &had_charset, &boundary);
    base::TrimString(boundary, " \"", &boundary);

    // If there's no boundary, just handle the request normally.  In the gecko
    // code, nsMultiMixedConv::OnStartRequest throws an exception.
    if (!boundary.empty()) {
      multipart_delegate_.reset(
          new MultipartResponseDelegate(client_, loader_, response, boundary));
    }
  } else if (info.mime_type == "text/vnd.chromium.ftp-dir" &&
             !show_raw_listing) {
    ftp_listing_delegate_.reset(
        new FtpDirectoryListingResponseDelegate(client_, loader_, response));
  }
}

void WebURLLoaderImpl::Context::OnDownloadedData(int len,
                                                 int encoded_data_length) {
  if (client_)
    client_->didDownloadData(loader_, len, encoded_data_length);
}

void WebURLLoaderImpl::Context::OnReceivedData(const char* data,
                                               int data_length,
                                               int encoded_data_length) {
  if (!client_)
    return;

  if (request_.useStreamOnResponse()) {
    // We don't support ftp_listening_delegate_ and multipart_delegate_ for now.
    // TODO(yhirano): Support ftp listening and multipart.
    MojoResult rv = WriteDataOnBodyStream(data, data_length);
    if (rv != MOJO_RESULT_OK && client_) {
      client_->didFail(loader_,
                       loader_->CreateError(request_.url(),
                                            false,
                                            net::ERR_FAILED));
    }
  } else if (ftp_listing_delegate_) {
    // The FTP listing delegate will make the appropriate calls to
    // client_->didReceiveData and client_->didReceiveResponse.  Since the
    // delegate may want to do work after sending data to the delegate, keep
    // |this| and the delegate alive until it's finished handling the data.
    scoped_refptr<Context> protect(this);
    ftp_listing_delegate_->OnReceivedData(data, data_length);
  } else if (multipart_delegate_) {
    // The multipart delegate will make the appropriate calls to
    // client_->didReceiveData and client_->didReceiveResponse.  Since the
    // delegate may want to do work after sending data to the delegate, keep
    // |this| and the delegate alive until it's finished handling the data.
    scoped_refptr<Context> protect(this);
    multipart_delegate_->OnReceivedData(data, data_length, encoded_data_length);
  } else {
    client_->didReceiveData(loader_, data, data_length, encoded_data_length);
  }
}

void WebURLLoaderImpl::Context::OnReceivedCachedMetadata(
    const char* data, int len) {
  if (client_)
    client_->didReceiveCachedMetadata(loader_, data, len);
}

void WebURLLoaderImpl::Context::OnCompletedRequest(
    int error_code,
    bool was_ignored_by_handler,
    bool stale_copy_in_cache,
    const std::string& security_info,
    const base::TimeTicks& completion_time,
    int64 total_transfer_size) {
  // The WebURLLoaderImpl may be deleted in any of the calls to the client or
  // the delegates below (As they also may call in to the client).  Keep |this|
  // alive in that case, to avoid a crash.  If that happens, the request will be
  // cancelled and |client_| will be set to NULL.
  scoped_refptr<Context> protect(this);

  if (ftp_listing_delegate_) {
    ftp_listing_delegate_->OnCompletedRequest();
    ftp_listing_delegate_.reset(NULL);
  } else if (multipart_delegate_) {
    multipart_delegate_->OnCompletedRequest();
    multipart_delegate_.reset(NULL);
  }

  if (client_) {
    if (error_code != net::OK) {
      client_->didFail(loader_, CreateError(request_.url(),
                                            stale_copy_in_cache,
                                            error_code));
    } else {
      if (request_.useStreamOnResponse()) {
        got_all_stream_body_data_ = true;
        if (body_stream_buffer_.empty()) {
          // Close the handle to notify the end of data.
          body_stream_writer_.reset();
          client_->didFinishLoading(
              loader_, (completion_time - TimeTicks()).InSecondsF(),
              total_transfer_size);
        }
      } else {
        client_->didFinishLoading(
            loader_, (completion_time - TimeTicks()).InSecondsF(),
            total_transfer_size);
      }
    }
  }
}

WebURLLoaderImpl::Context::~Context() {
  if (request_id_ >= 0) {
    resource_dispatcher_->RemovePendingRequest(request_id_);
  }
}

bool WebURLLoaderImpl::Context::CanHandleDataURLRequestLocally() const {
  GURL url = request_.url();
  if (!url.SchemeIs(url::kDataScheme))
    return false;

  // The fast paths for data URL, Start() and HandleDataURL(), don't support
  // the downloadToFile option.
  if (request_.downloadToFile())
    return false;

  // Optimize for the case where we can handle a data URL locally.  We must
  // skip this for data URLs targetted at frames since those could trigger a
  // download.
  //
  // NOTE: We special case MIME types we can render both for performance
  // reasons as well as to support unit tests.

#if defined(OS_ANDROID)
  // For compatibility reasons on Android we need to expose top-level data://
  // to the browser.
  if (request_.frameType() == WebURLRequest::FrameTypeTopLevel)
    return false;
#endif

  if (request_.frameType() != WebURLRequest::FrameTypeTopLevel &&
      request_.frameType() != WebURLRequest::FrameTypeNested)
    return true;

  std::string mime_type, unused_charset;
  if (net::DataURL::Parse(request_.url(), &mime_type, &unused_charset, NULL) &&
      net::IsSupportedMimeType(mime_type))
    return true;

  return false;
}

void WebURLLoaderImpl::Context::HandleDataURL() {
  DCHECK_NE(defers_loading_, DEFERRED_DATA);
  if (defers_loading_ == SHOULD_DEFER) {
      defers_loading_ = DEFERRED_DATA;
      return;
  }

  ResourceResponseInfo info;
  std::string data;

  int error_code = GetInfoFromDataURL(request_.url(), &info, &data);

  if (error_code == net::OK) {
    OnReceivedResponse(info);
    if (!data.empty())
      OnReceivedData(data.data(), data.size(), 0);
  }

  OnCompletedRequest(error_code, false, false, info.security_info,
                     base::TimeTicks::Now(), 0);
}

MojoResult WebURLLoaderImpl::Context::WriteDataOnBodyStream(const char* data,
                                                            size_t size) {
  if (body_stream_buffer_.empty() && size == 0) {
    // Nothing to do.
    return MOJO_RESULT_OK;
  }

  if (!body_stream_writer_.is_valid()) {
    // The handle is already cleared.
    return MOJO_RESULT_OK;
  }

  char* buffer = nullptr;
  uint32_t num_bytes_writable = 0;
  MojoResult rv = mojo::BeginWriteDataRaw(body_stream_writer_.get(),
                                          reinterpret_cast<void**>(&buffer),
                                          &num_bytes_writable,
                                          MOJO_WRITE_DATA_FLAG_NONE);
  if (rv == MOJO_RESULT_SHOULD_WAIT) {
    body_stream_buffer_.insert(body_stream_buffer_.end(), data, data + size);
    body_stream_writer_watcher_.Start(
        body_stream_writer_.get(),
        MOJO_HANDLE_SIGNAL_WRITABLE,
        MOJO_DEADLINE_INDEFINITE,
        base::Bind(&WebURLLoaderImpl::Context::OnHandleGotWritable,
                   base::Unretained(this)));
    return MOJO_RESULT_OK;
  }

  if (rv != MOJO_RESULT_OK)
    return rv;

  uint32_t num_bytes_to_write = 0;
  if (num_bytes_writable < body_stream_buffer_.size()) {
    auto begin = body_stream_buffer_.begin();
    auto end = body_stream_buffer_.begin() + num_bytes_writable;

    std::copy(begin, end, buffer);
    num_bytes_to_write = num_bytes_writable;
    body_stream_buffer_.erase(begin, end);
    body_stream_buffer_.insert(body_stream_buffer_.end(), data, data + size);
  } else {
    std::copy(body_stream_buffer_.begin(), body_stream_buffer_.end(), buffer);
    num_bytes_writable -= body_stream_buffer_.size();
    num_bytes_to_write += body_stream_buffer_.size();
    buffer += body_stream_buffer_.size();
    body_stream_buffer_.clear();

    size_t num_newbytes_to_write =
        std::min(size, static_cast<size_t>(num_bytes_writable));
    std::copy(data, data + num_newbytes_to_write, buffer);
    num_bytes_to_write += num_newbytes_to_write;
    body_stream_buffer_.insert(body_stream_buffer_.end(),
                               data + num_newbytes_to_write,
                               data + size);
  }

  rv = mojo::EndWriteDataRaw(body_stream_writer_.get(), num_bytes_to_write);
  if (rv == MOJO_RESULT_OK && !body_stream_buffer_.empty()) {
    body_stream_writer_watcher_.Start(
        body_stream_writer_.get(),
        MOJO_HANDLE_SIGNAL_WRITABLE,
        MOJO_DEADLINE_INDEFINITE,
        base::Bind(&WebURLLoaderImpl::Context::OnHandleGotWritable,
                   base::Unretained(this)));
  }
  return rv;
}

void WebURLLoaderImpl::Context::OnHandleGotWritable(MojoResult result) {
  if (result != MOJO_RESULT_OK) {
    if (client_) {
      client_->didFail(loader_,
                       loader_->CreateError(request_.url(),
                                            false,
                                            net::ERR_FAILED));
      // |this| can be deleted here.
    }
    return;
  }

  if (body_stream_buffer_.empty())
    return;

  MojoResult rv = WriteDataOnBodyStream(nullptr, 0);
  if (rv == MOJO_RESULT_OK) {
    if (got_all_stream_body_data_ && body_stream_buffer_.empty()) {
      // Close the handle to notify the end of data.
      body_stream_writer_.reset();
      if (client_) {
        // TODO(yhirano): Pass appropriate arguments.
        client_->didFinishLoading(loader_, 0, 0);
        // |this| can be deleted here.
      }
    }
  } else {
    if (client_) {
      client_->didFail(loader_, loader_->CreateError(request_.url(),
                                                     false,
                                                     net::ERR_FAILED));
      // |this| can be deleted here.
    }
  }
}

// WebURLLoaderImpl -----------------------------------------------------------

WebURLLoaderImpl::WebURLLoaderImpl(
    ResourceDispatcher* resource_dispatcher,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : context_(new Context(this, resource_dispatcher, task_runner)) {
}

WebURLLoaderImpl::~WebURLLoaderImpl() {
  cancel();
}

WebURLError WebURLLoaderImpl::CreateError(const WebURL& unreachable_url,
                                          bool stale_copy_in_cache,
                                          int reason) {
  WebURLError error;
  error.domain = WebString::fromUTF8(net::kErrorDomain);
  error.reason = reason;
  error.unreachableURL = unreachable_url;
  error.staleCopyInCache = stale_copy_in_cache;
  if (reason == net::ERR_ABORTED) {
    error.isCancellation = true;
  } else if (reason == net::ERR_TEMPORARILY_THROTTLED) {
    error.localizedDescription = WebString::fromUTF8(
        kThrottledErrorDescription);
  } else {
    error.localizedDescription = WebString::fromUTF8(
        net::ErrorToString(reason));
  }
  return error;
}

void WebURLLoaderImpl::PopulateURLResponse(const GURL& url,
                                           const ResourceResponseInfo& info,
                                           WebURLResponse* response) {
  response->setURL(url);
  response->setResponseTime(info.response_time.ToInternalValue());
  response->setMIMEType(WebString::fromUTF8(info.mime_type));
  response->setTextEncodingName(WebString::fromUTF8(info.charset));
  response->setExpectedContentLength(info.content_length);
  response->setSecurityInfo(info.security_info);
  response->setAppCacheID(info.appcache_id);
  response->setAppCacheManifestURL(info.appcache_manifest_url);
  response->setWasCached(!info.load_timing.request_start_time.is_null() &&
      info.response_time < info.load_timing.request_start_time);
  response->setRemoteIPAddress(
      WebString::fromUTF8(info.socket_address.host()));
  response->setRemotePort(info.socket_address.port());
  response->setConnectionID(info.load_timing.socket_log_id);
  response->setConnectionReused(info.load_timing.socket_reused);
  response->setDownloadFilePath(info.download_file_path.AsUTF16Unsafe());
  response->setWasFetchedViaSPDY(info.was_fetched_via_spdy);
  response->setWasFetchedViaServiceWorker(info.was_fetched_via_service_worker);
  response->setWasFallbackRequiredByServiceWorker(
      info.was_fallback_required_by_service_worker);
  response->setServiceWorkerResponseType(info.response_type_via_service_worker);
  response->setOriginalURLViaServiceWorker(
      info.original_url_via_service_worker);

  WebURLResponseExtraDataImpl* extra_data =
      new WebURLResponseExtraDataImpl(info.npn_negotiated_protocol);
  response->setExtraData(extra_data);
  extra_data->set_was_fetched_via_spdy(info.was_fetched_via_spdy);
  extra_data->set_was_npn_negotiated(info.was_npn_negotiated);
  extra_data->set_was_alternate_protocol_available(
      info.was_alternate_protocol_available);
  extra_data->set_connection_info(info.connection_info);
  extra_data->set_was_fetched_via_proxy(info.was_fetched_via_proxy);
  extra_data->set_proxy_server(info.proxy_server);

  // If there's no received headers end time, don't set load timing.  This is
  // the case for non-HTTP requests, requests that don't go over the wire, and
  // certain error cases.
  if (!info.load_timing.receive_headers_end.is_null()) {
    WebURLLoadTiming timing;
    PopulateURLLoadTiming(info.load_timing, &timing);
    const TimeTicks kNullTicks;
    timing.setServiceWorkerFetchStart(
        (info.service_worker_fetch_start - kNullTicks).InSecondsF());
    timing.setServiceWorkerFetchReady(
        (info.service_worker_fetch_ready - kNullTicks).InSecondsF());
    timing.setServiceWorkerFetchEnd(
        (info.service_worker_fetch_end - kNullTicks).InSecondsF());
    response->setLoadTiming(timing);
  }

  if (info.devtools_info.get()) {
    WebHTTPLoadInfo load_info;

    load_info.setHTTPStatusCode(info.devtools_info->http_status_code);
    load_info.setHTTPStatusText(WebString::fromLatin1(
        info.devtools_info->http_status_text));
    load_info.setEncodedDataLength(info.encoded_data_length);

    load_info.setRequestHeadersText(WebString::fromLatin1(
        info.devtools_info->request_headers_text));
    load_info.setResponseHeadersText(WebString::fromLatin1(
        info.devtools_info->response_headers_text));
    const HeadersVector& request_headers = info.devtools_info->request_headers;
    for (HeadersVector::const_iterator it = request_headers.begin();
         it != request_headers.end(); ++it) {
      load_info.addRequestHeader(WebString::fromLatin1(it->first),
          WebString::fromLatin1(it->second));
    }
    const HeadersVector& response_headers =
        info.devtools_info->response_headers;
    for (HeadersVector::const_iterator it = response_headers.begin();
         it != response_headers.end(); ++it) {
      load_info.addResponseHeader(WebString::fromLatin1(it->first),
          WebString::fromLatin1(it->second));
    }
    load_info.setNPNNegotiatedProtocol(WebString::fromLatin1(
        info.npn_negotiated_protocol));
    response->setHTTPLoadInfo(load_info);
  }

  const net::HttpResponseHeaders* headers = info.headers.get();
  if (!headers)
    return;

  WebURLResponse::HTTPVersion version = WebURLResponse::Unknown;
  if (headers->GetHttpVersion() == net::HttpVersion(0, 9))
    version = WebURLResponse::HTTP_0_9;
  else if (headers->GetHttpVersion() == net::HttpVersion(1, 0))
    version = WebURLResponse::HTTP_1_0;
  else if (headers->GetHttpVersion() == net::HttpVersion(1, 1))
    version = WebURLResponse::HTTP_1_1;
  response->setHTTPVersion(version);
  response->setHTTPStatusCode(headers->response_code());
  response->setHTTPStatusText(WebString::fromLatin1(headers->GetStatusText()));

  // TODO(darin): We should leverage HttpResponseHeaders for this, and this
  // should be using the same code as ResourceDispatcherHost.
  // TODO(jungshik): Figure out the actual value of the referrer charset and
  // pass it to GetSuggestedFilename.
  std::string value;
  headers->EnumerateHeader(NULL, "content-disposition", &value);
  response->setSuggestedFileName(
      net::GetSuggestedFilename(url,
                                value,
                                std::string(),  // referrer_charset
                                std::string(),  // suggested_name
                                std::string(),  // mime_type
                                std::string()));  // default_name

  Time time_val;
  if (headers->GetLastModifiedValue(&time_val))
    response->setLastModifiedDate(time_val.ToDoubleT());

  // Build up the header map.
  void* iter = NULL;
  std::string name;
  while (headers->EnumerateHeaderLines(&iter, &name, &value)) {
    response->addHTTPHeaderField(WebString::fromLatin1(name),
                                 WebString::fromLatin1(value));
  }
}

void WebURLLoaderImpl::loadSynchronously(const WebURLRequest& request,
                                         WebURLResponse& response,
                                         WebURLError& error,
                                         WebData& data) {
  SyncLoadResponse sync_load_response;
  context_->Start(request, &sync_load_response);

  const GURL& final_url = sync_load_response.url;

  // TODO(tc): For file loads, we may want to include a more descriptive
  // status code or status text.
  int error_code = sync_load_response.error_code;
  if (error_code != net::OK) {
    response.setURL(final_url);
    error.domain = WebString::fromUTF8(net::kErrorDomain);
    error.reason = error_code;
    error.unreachableURL = final_url;
    return;
  }

  PopulateURLResponse(final_url, sync_load_response, &response);

  data.assign(sync_load_response.data.data(),
              sync_load_response.data.size());
}

void WebURLLoaderImpl::loadAsynchronously(const WebURLRequest& request,
                                          WebURLLoaderClient* client) {
  DCHECK(!context_->client());

  context_->set_client(client);
  context_->Start(request, NULL);
}

void WebURLLoaderImpl::cancel() {
  context_->Cancel();
}

void WebURLLoaderImpl::setDefersLoading(bool value) {
  context_->SetDefersLoading(value);
}

void WebURLLoaderImpl::didChangePriority(WebURLRequest::Priority new_priority,
                                         int intra_priority_value) {
  context_->DidChangePriority(new_priority, intra_priority_value);
}

bool WebURLLoaderImpl::attachThreadedDataReceiver(
    blink::WebThreadedDataReceiver* threaded_data_receiver) {
  return context_->AttachThreadedDataReceiver(threaded_data_receiver);
}

}  // namespace content
