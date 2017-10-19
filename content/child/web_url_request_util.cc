// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/web_url_request_util.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/task_scheduler/post_task.h"
#include "content/child/child_thread_impl.h"
#include "content/child/request_extra_data.h"
#include "content/public/common/content_features.h"
#include "content/public/common/service_names.mojom.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "storage/public/interfaces/blobs.mojom.h"
#include "storage/public/interfaces/size_getter.mojom.h"
#include "third_party/WebKit/public/platform/FilePathConversion.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebCachePolicy.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebHTTPHeaderVisitor.h"
#include "third_party/WebKit/public/platform/WebMixedContent.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebThread.h"

using blink::WebCachePolicy;
using blink::WebData;
using blink::WebHTTPBody;
using blink::WebString;
using blink::WebURLRequest;

namespace content {

namespace {

std::string TrimLWSAndCRLF(const base::StringPiece& input) {
  base::StringPiece string = net::HttpUtil::TrimLWS(input);
  const char* begin = string.data();
  const char* end = string.data() + string.size();
  while (begin < end && (end[-1] == '\r' || end[-1] == '\n'))
    --end;
  return std::string(base::StringPiece(begin, end - begin));
}

class HttpRequestHeadersVisitor : public blink::WebHTTPHeaderVisitor {
 public:
  explicit HttpRequestHeadersVisitor(net::HttpRequestHeaders* headers)
      : headers_(headers) {}
  ~HttpRequestHeadersVisitor() override = default;

  void VisitHeader(const WebString& name, const WebString& value) override {
    std::string name_latin1 = name.Latin1();
    std::string value_latin1 = TrimLWSAndCRLF(value.Latin1());

    // Skip over referrer headers found in the header map because we already
    // pulled it out as a separate parameter.
    if (base::LowerCaseEqualsASCII(name_latin1, "referer"))
      return;

    DCHECK(net::HttpUtil::IsValidHeaderName(name_latin1)) << name_latin1;
    DCHECK(net::HttpUtil::IsValidHeaderValue(value_latin1)) << value_latin1;
    headers_->SetHeader(name_latin1, value_latin1);
  }

 private:
  net::HttpRequestHeaders* const headers_;
};

class HeaderFlattener : public blink::WebHTTPHeaderVisitor {
 public:
  HeaderFlattener() {}
  ~HeaderFlattener() override {}

  void VisitHeader(const WebString& name, const WebString& value) override {
    // Headers are latin1.
    const std::string& name_latin1 = name.Latin1();
    const std::string& value_latin1 = value.Latin1();

    // Skip over referrer headers found in the header map because we already
    // pulled it out as a separate parameter.
    if (base::LowerCaseEqualsASCII(name_latin1, "referer"))
      return;

    if (!buffer_.empty())
      buffer_.append("\r\n");
    buffer_.append(name_latin1 + ": " + value_latin1);
  }

  const std::string& GetBuffer() const {
    return buffer_;
  }

 private:
  std::string buffer_;
};

// A helper class which allows a holder of a data pipe for a blob to know how
// big the blob is. It will stay alive until either the caller gets the length
// or the size getter pipe is torn down.
class BlobSizeGetter : public storage::mojom::BlobReaderClient,
                       public storage::mojom::SizeGetter {
 public:
  BlobSizeGetter(
      storage::mojom::BlobReaderClientRequest blob_reader_client_request,
      storage::mojom::SizeGetterRequest size_getter_request)
      : blob_reader_client_binding_(this), size_getter_binding_(this) {
    // If a sync XHR is doing the upload, then the main thread will be blocked.
    // So we must bind these interfaces on a background thread, otherwise the
    // methods below will never be called and the processes will hang.
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        base::CreateSingleThreadTaskRunnerWithTraits(
            {base::TaskPriority::USER_VISIBLE,
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&BlobSizeGetter::BindInternal, base::Unretained(this),
                       std::move(blob_reader_client_request),
                       std::move(size_getter_request)));
  }

 private:
  ~BlobSizeGetter() override {}

  void BindInternal(
      storage::mojom::BlobReaderClientRequest blob_reader_client_request,
      storage::mojom::SizeGetterRequest size_getter_request) {
    blob_reader_client_binding_.Bind(std::move(blob_reader_client_request));
    size_getter_binding_.Bind(std::move(size_getter_request));
    size_getter_binding_.set_connection_error_handler(base::BindOnce(
        &BlobSizeGetter::OnSizeGetterConnectionError, base::Unretained(this)));
  }

  // storage::mojom::BlobReaderClient implementation:
  void OnCalculatedSize(uint64_t total_size,
                        uint64_t expected_content_size) override {
    size_ = total_size;
    calculated_size_ = true;
    if (!callback_.is_null()) {
      std::move(callback_).Run(total_size);
      delete this;
    } else if (!size_getter_binding_.is_bound()) {
      delete this;
    }
  }

  void OnComplete(int32_t status, uint64_t data_length) override {}

  // storage::mojom::SizeGetter implementation:
  void GetSize(GetSizeCallback callback) override {
    if (calculated_size_) {
      std::move(callback).Run(size_);
      delete this;
    } else {
      callback_ = std::move(callback);
    }
  }

  void OnSizeGetterConnectionError() {
    if (calculated_size_)
      delete this;
  }

  bool calculated_size_ = false;
  uint64_t size_ = 0;
  mojo::Binding<storage::mojom::BlobReaderClient> blob_reader_client_binding_;
  mojo::Binding<storage::mojom::SizeGetter> size_getter_binding_;
  GetSizeCallback callback_;
};

}  // namespace

ResourceType WebURLRequestContextToResourceType(
    WebURLRequest::RequestContext request_context) {
  switch (request_context) {
    // CSP report
    case WebURLRequest::kRequestContextCSPReport:
      return RESOURCE_TYPE_CSP_REPORT;

    // Favicon
    case WebURLRequest::kRequestContextFavicon:
      return RESOURCE_TYPE_FAVICON;

    // Font
    case WebURLRequest::kRequestContextFont:
      return RESOURCE_TYPE_FONT_RESOURCE;

    // Image
    case WebURLRequest::kRequestContextImage:
    case WebURLRequest::kRequestContextImageSet:
      return RESOURCE_TYPE_IMAGE;

    // Media
    case WebURLRequest::kRequestContextAudio:
    case WebURLRequest::kRequestContextVideo:
      return RESOURCE_TYPE_MEDIA;

    // Object
    case WebURLRequest::kRequestContextEmbed:
    case WebURLRequest::kRequestContextObject:
      return RESOURCE_TYPE_OBJECT;

    // Ping
    case WebURLRequest::kRequestContextBeacon:
    case WebURLRequest::kRequestContextPing:
      return RESOURCE_TYPE_PING;

    // Subresource of plugins
    case WebURLRequest::kRequestContextPlugin:
      return RESOURCE_TYPE_PLUGIN_RESOURCE;

    // Prefetch
    case WebURLRequest::kRequestContextPrefetch:
      return RESOURCE_TYPE_PREFETCH;

    // Script
    case WebURLRequest::kRequestContextImport:
    case WebURLRequest::kRequestContextScript:
      return RESOURCE_TYPE_SCRIPT;

    // Style
    case WebURLRequest::kRequestContextXSLT:
    case WebURLRequest::kRequestContextStyle:
      return RESOURCE_TYPE_STYLESHEET;

    // Subresource
    case WebURLRequest::kRequestContextDownload:
    case WebURLRequest::kRequestContextManifest:
    case WebURLRequest::kRequestContextSubresource:
      return RESOURCE_TYPE_SUB_RESOURCE;

    // TextTrack
    case WebURLRequest::kRequestContextTrack:
      return RESOURCE_TYPE_MEDIA;

    // Workers
    case WebURLRequest::kRequestContextServiceWorker:
      return RESOURCE_TYPE_SERVICE_WORKER;
    case WebURLRequest::kRequestContextSharedWorker:
      return RESOURCE_TYPE_SHARED_WORKER;
    case WebURLRequest::kRequestContextWorker:
      return RESOURCE_TYPE_WORKER;

    // Unspecified
    case WebURLRequest::kRequestContextInternal:
    case WebURLRequest::kRequestContextUnspecified:
      return RESOURCE_TYPE_SUB_RESOURCE;

    // XHR
    case WebURLRequest::kRequestContextEventSource:
    case WebURLRequest::kRequestContextFetch:
    case WebURLRequest::kRequestContextXMLHttpRequest:
      return RESOURCE_TYPE_XHR;

    // These should be handled by the FrameType checks at the top of the
    // function.
    case WebURLRequest::kRequestContextForm:
    case WebURLRequest::kRequestContextHyperlink:
    case WebURLRequest::kRequestContextLocation:
    case WebURLRequest::kRequestContextFrame:
    case WebURLRequest::kRequestContextIframe:
      NOTREACHED();
      return RESOURCE_TYPE_SUB_RESOURCE;

    default:
      NOTREACHED();
      return RESOURCE_TYPE_SUB_RESOURCE;
  }
}

ResourceType WebURLRequestToResourceType(const WebURLRequest& request) {
  WebURLRequest::RequestContext request_context = request.GetRequestContext();
  if (request.GetFrameType() != WebURLRequest::kFrameTypeNone) {
    DCHECK(request_context == WebURLRequest::kRequestContextForm ||
           request_context == WebURLRequest::kRequestContextFrame ||
           request_context == WebURLRequest::kRequestContextHyperlink ||
           request_context == WebURLRequest::kRequestContextIframe ||
           request_context == WebURLRequest::kRequestContextInternal ||
           request_context == WebURLRequest::kRequestContextLocation);
    if (request.GetFrameType() == WebURLRequest::kFrameTypeTopLevel ||
        request.GetFrameType() == WebURLRequest::kFrameTypeAuxiliary) {
      return RESOURCE_TYPE_MAIN_FRAME;
    }
    if (request.GetFrameType() == WebURLRequest::kFrameTypeNested)
      return RESOURCE_TYPE_SUB_FRAME;
    NOTREACHED();
    return RESOURCE_TYPE_SUB_RESOURCE;
  }
  return WebURLRequestContextToResourceType(request_context);
}

net::HttpRequestHeaders GetWebURLRequestHeaders(
    const blink::WebURLRequest& request) {
  net::HttpRequestHeaders headers;
  HttpRequestHeadersVisitor visitor(&headers);
  request.VisitHTTPHeaderFields(&visitor);
  return headers;
}

std::string GetWebURLRequestHeadersAsString(
    const blink::WebURLRequest& request) {
  HeaderFlattener flattener;
  request.VisitHTTPHeaderFields(&flattener);
  return flattener.GetBuffer();
}

int GetLoadFlagsForWebURLRequest(const WebURLRequest& request) {
  int load_flags = net::LOAD_NORMAL;
  GURL url = request.Url();
  switch (request.GetCachePolicy()) {
    case WebCachePolicy::kValidatingCacheData:
      load_flags |= net::LOAD_VALIDATE_CACHE;
      break;
    case WebCachePolicy::kBypassingCache:
      load_flags |= net::LOAD_BYPASS_CACHE;
      break;
    case WebCachePolicy::kReturnCacheDataElseLoad:
      load_flags |= net::LOAD_SKIP_CACHE_VALIDATION;
      break;
    case WebCachePolicy::kReturnCacheDataDontLoad:
      load_flags |= net::LOAD_ONLY_FROM_CACHE | net::LOAD_SKIP_CACHE_VALIDATION;
      break;
    case WebCachePolicy::kReturnCacheDataIfValid:
      load_flags |= net::LOAD_ONLY_FROM_CACHE;
      break;
    case WebCachePolicy::kUseProtocolCachePolicy:
      break;
    case WebCachePolicy::kBypassCacheLoadOnlyFromCache:
      load_flags |= net::LOAD_ONLY_FROM_CACHE | net::LOAD_BYPASS_CACHE;
      break;
  }

  if (!request.AllowStoredCredentials()) {
    load_flags |= net::LOAD_DO_NOT_SAVE_COOKIES;
    load_flags |= net::LOAD_DO_NOT_SEND_COOKIES;
    load_flags |= net::LOAD_DO_NOT_SEND_AUTH_DATA;
  }

  if (request.GetExtraData()) {
    RequestExtraData* extra_data =
        static_cast<RequestExtraData*>(request.GetExtraData());
    if (extra_data->is_prefetch())
      load_flags |= net::LOAD_PREFETCH;
  }

  return load_flags;
}

WebHTTPBody GetWebHTTPBodyForRequestBody(
    const scoped_refptr<ResourceRequestBody>& input) {
  WebHTTPBody http_body;
  http_body.Initialize();
  http_body.SetIdentifier(input->identifier());
  http_body.SetContainsPasswordData(input->contains_sensitive_info());
  for (const auto& element : *input->elements()) {
    switch (element.type()) {
      case ResourceRequestBody::Element::TYPE_BYTES:
        http_body.AppendData(WebData(element.bytes(), element.length()));
        break;
      case ResourceRequestBody::Element::TYPE_FILE:
        http_body.AppendFileRange(
            blink::FilePathToWebString(element.path()), element.offset(),
            (element.length() != std::numeric_limits<uint64_t>::max())
                ? element.length()
                : -1,
            element.expected_modification_time().ToDoubleT());
        break;
      case ResourceRequestBody::Element::TYPE_FILE_FILESYSTEM:
        http_body.AppendFileSystemURLRange(
            element.filesystem_url(), element.offset(),
            (element.length() != std::numeric_limits<uint64_t>::max())
                ? element.length()
                : -1,
            element.expected_modification_time().ToDoubleT());
        break;
      case ResourceRequestBody::Element::TYPE_BLOB:
        http_body.AppendBlob(WebString::FromASCII(element.blob_uuid()));
        break;
      case ResourceRequestBody::Element::TYPE_BYTES_DESCRIPTION:
      case ResourceRequestBody::Element::TYPE_DISK_CACHE_ENTRY:
      default:
        NOTREACHED();
        break;
    }
  }
  return http_body;
}

scoped_refptr<ResourceRequestBody> GetRequestBodyForWebURLRequest(
    const WebURLRequest& request) {
  scoped_refptr<ResourceRequestBody> request_body;

  if (request.HttpBody().IsNull()) {
    return request_body;
  }

  const std::string& method = request.HttpMethod().Latin1();
  // GET and HEAD requests shouldn't have http bodies.
  DCHECK(method != "GET" && method != "HEAD");

  return GetRequestBodyForWebHTTPBody(request.HttpBody());
}

void GetBlobRegistry(storage::mojom::BlobRegistryRequest request) {
  ChildThreadImpl::current()->GetConnector()->BindInterface(
      mojom::kBrowserServiceName, std::move(request));
}

scoped_refptr<ResourceRequestBody> GetRequestBodyForWebHTTPBody(
    const blink::WebHTTPBody& httpBody) {
  scoped_refptr<ResourceRequestBody> request_body = new ResourceRequestBody();
  size_t i = 0;
  WebHTTPBody::Element element;
  // TODO(jam): cache this somewhere so we don't request it each time?
  storage::mojom::BlobRegistryPtr blob_registry;
  while (httpBody.ElementAt(i++, element)) {
    switch (element.type) {
      case WebHTTPBody::Element::kTypeData:
        element.data.ForEachSegment([&request_body](const char* segment,
                                                    size_t segment_size,
                                                    size_t segment_offset) {
          request_body->AppendBytes(segment, static_cast<int>(segment_size));
          return true;
        });
        break;
      case WebHTTPBody::Element::kTypeFile:
        if (element.file_length == -1) {
          request_body->AppendFileRange(
              blink::WebStringToFilePath(element.file_path), 0,
              std::numeric_limits<uint64_t>::max(), base::Time());
        } else {
          request_body->AppendFileRange(
              blink::WebStringToFilePath(element.file_path),
              static_cast<uint64_t>(element.file_start),
              static_cast<uint64_t>(element.file_length),
              base::Time::FromDoubleT(element.modification_time));
        }
        break;
      case WebHTTPBody::Element::kTypeFileSystemURL: {
        GURL file_system_url = element.file_system_url;
        DCHECK(file_system_url.SchemeIsFileSystem());
        request_body->AppendFileSystemFileRange(
            file_system_url, static_cast<uint64_t>(element.file_start),
            static_cast<uint64_t>(element.file_length),
            base::Time::FromDoubleT(element.modification_time));
        break;
      }
      case WebHTTPBody::Element::kTypeBlob: {
        if (base::FeatureList::IsEnabled(features::kNetworkService)) {
          if (!blob_registry.is_bound()) {
            if (ChildThreadImpl::current()) {
              ChildThreadImpl::current()->GetConnector()->BindInterface(
                  mojom::kBrowserServiceName, MakeRequest(&blob_registry));
            } else {
              // TODO(sammc): We should use per-frame / per-worker
              // InterfaceProvider instead (crbug.com/734210).
              blink::Platform::Current()
                  ->MainThread()
                  ->GetSingleThreadTaskRunner()
                  ->PostTask(FROM_HERE,
                             base::BindOnce(&GetBlobRegistry,
                                            MakeRequest(&blob_registry)));
            }
          }
          storage::mojom::BlobPtr blob_ptr;
          blob_registry->GetBlobFromUUID(MakeRequest(&blob_ptr),
                                         element.blob_uuid.Utf8());

          storage::mojom::BlobReaderClientPtr blob_reader_client_ptr;
          storage::mojom::SizeGetterPtr size_getter_ptr;
          // Object deletes itself.
          new BlobSizeGetter(MakeRequest(&blob_reader_client_ptr),
                             MakeRequest(&size_getter_ptr));

          mojo::DataPipe data_pipe;
          request_body->AppendDataPipe(std::move(data_pipe.consumer_handle),
                                       std::move(size_getter_ptr));

          blob_ptr->ReadAll(std::move(data_pipe.producer_handle),
                            std::move(blob_reader_client_ptr));
        } else {
          request_body->AppendBlob(element.blob_uuid.Utf8());
        }
        break;
      }
      default:
        NOTREACHED();
    }
  }
  request_body->set_identifier(httpBody.Identifier());
  request_body->set_contains_sensitive_info(httpBody.ContainsPasswordData());
  return request_body;
}

#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enums: " #a)

STATIC_ASSERT_ENUM(FETCH_REQUEST_MODE_SAME_ORIGIN,
                   WebURLRequest::kFetchRequestModeSameOrigin);
STATIC_ASSERT_ENUM(FETCH_REQUEST_MODE_NO_CORS,
                   WebURLRequest::kFetchRequestModeNoCORS);
STATIC_ASSERT_ENUM(FETCH_REQUEST_MODE_CORS,
                   WebURLRequest::kFetchRequestModeCORS);
STATIC_ASSERT_ENUM(FETCH_REQUEST_MODE_CORS_WITH_FORCED_PREFLIGHT,
                   WebURLRequest::kFetchRequestModeCORSWithForcedPreflight);
STATIC_ASSERT_ENUM(FETCH_REQUEST_MODE_NAVIGATE,
                   WebURLRequest::kFetchRequestModeNavigate);

FetchRequestMode GetFetchRequestModeForWebURLRequest(
    const WebURLRequest& request) {
  return static_cast<FetchRequestMode>(request.GetFetchRequestMode());
}

STATIC_ASSERT_ENUM(FETCH_CREDENTIALS_MODE_OMIT,
                   WebURLRequest::kFetchCredentialsModeOmit);
STATIC_ASSERT_ENUM(FETCH_CREDENTIALS_MODE_SAME_ORIGIN,
                   WebURLRequest::kFetchCredentialsModeSameOrigin);
STATIC_ASSERT_ENUM(FETCH_CREDENTIALS_MODE_INCLUDE,
                   WebURLRequest::kFetchCredentialsModeInclude);
STATIC_ASSERT_ENUM(FETCH_CREDENTIALS_MODE_PASSWORD,
                   WebURLRequest::kFetchCredentialsModePassword);

FetchCredentialsMode GetFetchCredentialsModeForWebURLRequest(
    const WebURLRequest& request) {
  return static_cast<FetchCredentialsMode>(request.GetFetchCredentialsMode());
}

STATIC_ASSERT_ENUM(FetchRedirectMode::FOLLOW_MODE,
                   WebURLRequest::kFetchRedirectModeFollow);
STATIC_ASSERT_ENUM(FetchRedirectMode::ERROR_MODE,
                   WebURLRequest::kFetchRedirectModeError);
STATIC_ASSERT_ENUM(FetchRedirectMode::MANUAL_MODE,
                   WebURLRequest::kFetchRedirectModeManual);

FetchRedirectMode GetFetchRedirectModeForWebURLRequest(
    const WebURLRequest& request) {
  return static_cast<FetchRedirectMode>(request.GetFetchRedirectMode());
}

std::string GetFetchIntegrityForWebURLRequest(const WebURLRequest& request) {
  return request.GetFetchIntegrity().Utf8();
}

STATIC_ASSERT_ENUM(REQUEST_CONTEXT_FRAME_TYPE_AUXILIARY,
                   WebURLRequest::kFrameTypeAuxiliary);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_FRAME_TYPE_NESTED,
                   WebURLRequest::kFrameTypeNested);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_FRAME_TYPE_NONE,
                   WebURLRequest::kFrameTypeNone);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_FRAME_TYPE_TOP_LEVEL,
                   WebURLRequest::kFrameTypeTopLevel);

RequestContextFrameType GetRequestContextFrameTypeForWebURLRequest(
    const WebURLRequest& request) {
  return static_cast<RequestContextFrameType>(request.GetFrameType());
}

STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_UNSPECIFIED,
                   WebURLRequest::kRequestContextUnspecified);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_AUDIO,
                   WebURLRequest::kRequestContextAudio);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_BEACON,
                   WebURLRequest::kRequestContextBeacon);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_CSP_REPORT,
                   WebURLRequest::kRequestContextCSPReport);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_DOWNLOAD,
                   WebURLRequest::kRequestContextDownload);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_EMBED,
                   WebURLRequest::kRequestContextEmbed);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_EVENT_SOURCE,
                   WebURLRequest::kRequestContextEventSource);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_FAVICON,
                   WebURLRequest::kRequestContextFavicon);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_FETCH,
                   WebURLRequest::kRequestContextFetch);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_FONT,
                   WebURLRequest::kRequestContextFont);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_FORM,
                   WebURLRequest::kRequestContextForm);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_FRAME,
                   WebURLRequest::kRequestContextFrame);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_HYPERLINK,
                   WebURLRequest::kRequestContextHyperlink);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_IFRAME,
                   WebURLRequest::kRequestContextIframe);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_IMAGE,
                   WebURLRequest::kRequestContextImage);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_IMAGE_SET,
                   WebURLRequest::kRequestContextImageSet);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_IMPORT,
                   WebURLRequest::kRequestContextImport);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_INTERNAL,
                   WebURLRequest::kRequestContextInternal);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_LOCATION,
                   WebURLRequest::kRequestContextLocation);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_MANIFEST,
                   WebURLRequest::kRequestContextManifest);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_OBJECT,
                   WebURLRequest::kRequestContextObject);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_PING,
                   WebURLRequest::kRequestContextPing);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_PLUGIN,
                   WebURLRequest::kRequestContextPlugin);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_PREFETCH,
                   WebURLRequest::kRequestContextPrefetch);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_SCRIPT,
                   WebURLRequest::kRequestContextScript);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_SERVICE_WORKER,
                   WebURLRequest::kRequestContextServiceWorker);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_SHARED_WORKER,
                   WebURLRequest::kRequestContextSharedWorker);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_SUBRESOURCE,
                   WebURLRequest::kRequestContextSubresource);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_STYLE,
                   WebURLRequest::kRequestContextStyle);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_TRACK,
                   WebURLRequest::kRequestContextTrack);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_VIDEO,
                   WebURLRequest::kRequestContextVideo);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_WORKER,
                   WebURLRequest::kRequestContextWorker);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_XML_HTTP_REQUEST,
                   WebURLRequest::kRequestContextXMLHttpRequest);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_XSLT,
                   WebURLRequest::kRequestContextXSLT);

RequestContextType GetRequestContextTypeForWebURLRequest(
    const WebURLRequest& request) {
  return static_cast<RequestContextType>(request.GetRequestContext());
}

blink::WebMixedContentContextType GetMixedContentContextTypeForWebURLRequest(
    const WebURLRequest& request) {
  bool block_mixed_plugin_content = false;
  if (request.GetExtraData()) {
    RequestExtraData* extra_data =
        static_cast<RequestExtraData*>(request.GetExtraData());
    block_mixed_plugin_content = extra_data->block_mixed_plugin_content();
  }

  return blink::WebMixedContent::ContextTypeFromRequestContext(
      request.GetRequestContext(), block_mixed_plugin_content);
}

STATIC_ASSERT_ENUM(ServiceWorkerMode::NONE,
                   WebURLRequest::ServiceWorkerMode::kNone);
STATIC_ASSERT_ENUM(ServiceWorkerMode::FOREIGN,
                   WebURLRequest::ServiceWorkerMode::kForeign);
STATIC_ASSERT_ENUM(ServiceWorkerMode::ALL,
                   WebURLRequest::ServiceWorkerMode::kAll);

ServiceWorkerMode GetServiceWorkerModeForWebURLRequest(
    const WebURLRequest& request) {
  return static_cast<ServiceWorkerMode>(request.GetServiceWorkerMode());
}

}  // namespace content
