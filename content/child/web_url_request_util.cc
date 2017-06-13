// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/web_url_request_util.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "content/child/request_extra_data.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/public/platform/FilePathConversion.h"
#include "third_party/WebKit/public/platform/WebCachePolicy.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebHTTPHeaderVisitor.h"
#include "third_party/WebKit/public/platform/WebMixedContent.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"

using blink::WebCachePolicy;
using blink::WebData;
using blink::WebHTTPBody;
using blink::WebString;
using blink::WebURLRequest;

namespace content {

namespace {

const char kThrottledErrorDescription[] =
    "Request throttled. Visit http://dev.chromium.org/throttling for more "
    "information.";

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

}  // namespace

ResourceType WebURLRequestToResourceType(const WebURLRequest& request) {
  WebURLRequest::RequestContext requestContext = request.GetRequestContext();
  if (request.GetFrameType() != WebURLRequest::kFrameTypeNone) {
    DCHECK(requestContext == WebURLRequest::kRequestContextForm ||
           requestContext == WebURLRequest::kRequestContextFrame ||
           requestContext == WebURLRequest::kRequestContextHyperlink ||
           requestContext == WebURLRequest::kRequestContextIframe ||
           requestContext == WebURLRequest::kRequestContextInternal ||
           requestContext == WebURLRequest::kRequestContextLocation);
    if (request.GetFrameType() == WebURLRequest::kFrameTypeTopLevel ||
        request.GetFrameType() == WebURLRequest::kFrameTypeAuxiliary) {
      return RESOURCE_TYPE_MAIN_FRAME;
    }
    if (request.GetFrameType() == WebURLRequest::kFrameTypeNested)
      return RESOURCE_TYPE_SUB_FRAME;
    NOTREACHED();
    return RESOURCE_TYPE_SUB_RESOURCE;
  }

  switch (requestContext) {
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

std::string GetWebURLRequestHeaders(const blink::WebURLRequest& request) {
  HeaderFlattener flattener;
  request.VisitHTTPHeaderFields(&flattener);
  return flattener.GetBuffer();
}

int GetLoadFlagsForWebURLRequest(const blink::WebURLRequest& request) {
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
    const scoped_refptr<ResourceRequestBodyImpl>& input) {
  WebHTTPBody http_body;
  http_body.Initialize();
  http_body.SetIdentifier(input->identifier());
  http_body.SetContainsPasswordData(input->contains_sensitive_info());
  for (const auto& element : *input->elements()) {
    switch (element.type()) {
      case ResourceRequestBodyImpl::Element::TYPE_BYTES:
        http_body.AppendData(WebData(element.bytes(), element.length()));
        break;
      case ResourceRequestBodyImpl::Element::TYPE_FILE:
        http_body.AppendFileRange(
            blink::FilePathToWebString(element.path()), element.offset(),
            (element.length() != std::numeric_limits<uint64_t>::max())
                ? element.length()
                : -1,
            element.expected_modification_time().ToDoubleT());
        break;
      case ResourceRequestBodyImpl::Element::TYPE_FILE_FILESYSTEM:
        http_body.AppendFileSystemURLRange(
            element.filesystem_url(), element.offset(),
            (element.length() != std::numeric_limits<uint64_t>::max())
                ? element.length()
                : -1,
            element.expected_modification_time().ToDoubleT());
        break;
      case ResourceRequestBodyImpl::Element::TYPE_BLOB:
        http_body.AppendBlob(WebString::FromASCII(element.blob_uuid()));
        break;
      case ResourceRequestBodyImpl::Element::TYPE_BYTES_DESCRIPTION:
      case ResourceRequestBodyImpl::Element::TYPE_DISK_CACHE_ENTRY:
      default:
        NOTREACHED();
        break;
    }
  }
  return http_body;
}

scoped_refptr<ResourceRequestBodyImpl> GetRequestBodyForWebURLRequest(
    const blink::WebURLRequest& request) {
  scoped_refptr<ResourceRequestBodyImpl> request_body;

  if (request.HttpBody().IsNull()) {
    return request_body;
  }

  const std::string& method = request.HttpMethod().Latin1();
  // GET and HEAD requests shouldn't have http bodies.
  DCHECK(method != "GET" && method != "HEAD");

  return GetRequestBodyForWebHTTPBody(request.HttpBody());
}

scoped_refptr<ResourceRequestBodyImpl> GetRequestBodyForWebHTTPBody(
    const blink::WebHTTPBody& httpBody) {
  scoped_refptr<ResourceRequestBodyImpl> request_body =
      new ResourceRequestBodyImpl();
  size_t i = 0;
  WebHTTPBody::Element element;
  while (httpBody.ElementAt(i++, element)) {
    switch (element.type) {
      case WebHTTPBody::Element::kTypeData:
        if (!element.data.IsEmpty()) {
          // Blink sometimes gives empty data to append. These aren't
          // necessary so they are just optimized out here.
          request_body->AppendBytes(element.data.Data(),
                                    static_cast<int>(element.data.size()));
        }
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
      case WebHTTPBody::Element::kTypeBlob:
        request_body->AppendBlob(element.blob_uuid.Utf8());
        break;
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
    const blink::WebURLRequest& request) {
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
    const blink::WebURLRequest& request) {
  return static_cast<FetchCredentialsMode>(request.GetFetchCredentialsMode());
}

STATIC_ASSERT_ENUM(FetchRedirectMode::FOLLOW_MODE,
                   WebURLRequest::kFetchRedirectModeFollow);
STATIC_ASSERT_ENUM(FetchRedirectMode::ERROR_MODE,
                   WebURLRequest::kFetchRedirectModeError);
STATIC_ASSERT_ENUM(FetchRedirectMode::MANUAL_MODE,
                   WebURLRequest::kFetchRedirectModeManual);

FetchRedirectMode GetFetchRedirectModeForWebURLRequest(
    const blink::WebURLRequest& request) {
  return static_cast<FetchRedirectMode>(request.GetFetchRedirectMode());
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
    const blink::WebURLRequest& request) {
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
    const blink::WebURLRequest& request) {
  return static_cast<RequestContextType>(request.GetRequestContext());
}

blink::WebMixedContentContextType GetMixedContentContextTypeForWebURLRequest(
    const blink::WebURLRequest& request) {
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
    const blink::WebURLRequest& request) {
  return static_cast<ServiceWorkerMode>(request.GetServiceWorkerMode());
}

blink::WebURLError CreateWebURLError(const blink::WebURL& unreachable_url,
                                     bool stale_copy_in_cache,
                                     int reason) {
  blink::WebURLError error;
  error.domain = WebString::FromASCII(net::kErrorDomain);
  error.reason = reason;
  error.unreachable_url = unreachable_url;
  error.stale_copy_in_cache = stale_copy_in_cache;
  if (reason == net::ERR_ABORTED) {
    error.is_cancellation = true;
  } else if (reason == net::ERR_CACHE_MISS) {
    error.is_cache_miss = true;
  } else if (reason == net::ERR_TEMPORARILY_THROTTLED) {
    error.localized_description =
        WebString::FromASCII(kThrottledErrorDescription);
  } else {
    error.localized_description =
        WebString::FromASCII(net::ErrorToString(reason));
  }
  return error;
}

blink::WebURLError CreateWebURLError(const blink::WebURL& unreachable_url,
                                     bool stale_copy_in_cache,
                                     int reason,
                                     bool was_ignored_by_handler) {
  blink::WebURLError error =
      CreateWebURLError(unreachable_url, stale_copy_in_cache, reason);
  error.was_ignored_by_handler = was_ignored_by_handler;
  return error;
}

}  // namespace content
