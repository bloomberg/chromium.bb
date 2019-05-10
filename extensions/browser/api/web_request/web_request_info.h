// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_INFO_H_
#define EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_INFO_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/values.h"
#include "extensions/browser/api/declarative_net_request/ruleset_manager.h"
#include "extensions/browser/api/web_request/web_request_resource_type.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "ipc/ipc_message.h"
#include "net/http/http_request_headers.h"
#include "net/log/net_log_event_type.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class ResourceContext;
}  // namespace content

namespace net {
class HttpResponseHeaders;
class URLRequest;
}  // namespace net

namespace network {
struct ResourceResponseHead;
}

namespace extensions {

class ExtensionNavigationUIData;

// Helper interface used to delegate event logging operations relevant to an
// in-process web request. This is a transitional interface to move WebRequest
// code away from direct coupling to NetLog and will be removed once event
// logging is done through the tracing subsystem instead of through net.
class WebRequestInfoLogger {
 public:
  virtual ~WebRequestInfoLogger() {}

  virtual void LogEvent(net::NetLogEventType event_type,
                        const std::string& extension_id) = 0;
  virtual void LogBlockedBy(const std::string& blocker_info) = 0;
  virtual void LogUnblocked() = 0;
};

// Helper struct to initialize WebRequestInfo.
struct WebRequestInfoInitParams {
  WebRequestInfoInitParams();
  WebRequestInfoInitParams(WebRequestInfoInitParams&& other);
  WebRequestInfoInitParams& operator=(WebRequestInfoInitParams&& other);

  explicit WebRequestInfoInitParams(net::URLRequest* url_request);

  // Initializes a WebRequestInfoInitParams from information provided over a
  // URLLoaderFactory interface, for use with the Network Service.
  WebRequestInfoInitParams(
      uint64_t request_id,
      int render_process_id,
      int render_frame_id,
      std::unique_ptr<ExtensionNavigationUIData> navigation_ui_data,
      int32_t routing_id,
      content::ResourceContext* resource_context,
      const network::ResourceRequest& request,
      bool is_download,
      bool is_async);

  ~WebRequestInfoInitParams();

  uint64_t id = 0;
  GURL url;
  GURL site_for_cookies;
  int render_process_id = -1;
  int routing_id = MSG_ROUTING_NONE;
  int frame_id = -1;
  std::string method;
  bool is_browser_side_navigation = false;
  base::Optional<url::Origin> initiator;
  base::Optional<content::ResourceType> type;
  WebRequestResourceType web_request_type = WebRequestResourceType::OTHER;
  bool is_async = false;
  net::HttpRequestHeaders extra_request_headers;
  bool is_pac_request = false;
  std::unique_ptr<base::DictionaryValue> request_body_data;
  bool is_web_view = false;
  int web_view_instance_id = -1;
  int web_view_rules_registry_id = -1;
  int web_view_embedder_process_id = -1;
  std::unique_ptr<WebRequestInfoLogger> logger;
  content::ResourceContext* resource_context = nullptr;
  base::Optional<ExtensionApiFrameIdMap::FrameData> frame_data;

 private:
  void InitializeWebViewAndFrameData(
      const ExtensionNavigationUIData* navigation_ui_data);

  DISALLOW_COPY_AND_ASSIGN(WebRequestInfoInitParams);
};

// A URL request representation used by WebRequest API internals. This structure
// carries information about an in-progress request.
struct WebRequestInfo {
  // Initializes a WebRequestInfoInitParams from a net::URLRequest. Should be
  // used sparingly, as we are moving away from direct URLRequest usage and
  // toward using Network Service. Prefer passing and referencing
  // WebRequestInfoInitParams in lieu of exposing any new direct references to
  // net::URLRequest throughout extensions WebRequest-related code.
  explicit WebRequestInfo(net::URLRequest* url_request);

  explicit WebRequestInfo(WebRequestInfoInitParams params);

  ~WebRequestInfo();

  // Fill in response data for this request. Only used when the Network Service
  // is disabled.
  void AddResponseInfoFromURLRequest(net::URLRequest* url_request);

  // Fill in response data for this request. Only used when the Network Service
  // is enabled.
  void AddResponseInfoFromResourceResponse(
      const network::ResourceResponseHead& response);

  // A unique identifier for this request.
  const uint64_t id;

  // The URL of the request.
  const GURL url;
  const GURL site_for_cookies;

  // The ID of the render process which initiated the request, or -1 of not
  // applicable (i.e. if initiated by the browser).
  const int render_process_id;

  // The routing ID of the object which initiated the request, if applicable.
  const int routing_id = MSG_ROUTING_NONE;

  // The render frame ID of the frame which initiated this request, or -1 if
  // the request was not initiated by a frame.
  const int frame_id;

  // The HTTP method used for the request, if applicable.
  const std::string method;

  // Indicates whether the request is for a browser-side navigation.
  const bool is_browser_side_navigation;

  // The origin of the context which initiated the request. May be null for
  // browser-initiated requests such as navigations.
  const base::Optional<url::Origin> initiator;

  // Extension API frame data corresponding to details of the frame which
  // initiate this request. May be null for renderer-initiated requests where
  // some frame details are not known at WebRequestInfo construction time.
  // Mutable since this is lazily computed.
  mutable base::Optional<ExtensionApiFrameIdMap::FrameData> frame_data;

  // The type of the request (e.g. main frame, subresource, XHR, etc). May have
  // no value if the request did not originate from a ResourceDispatcher.
  const base::Optional<content::ResourceType> type;

  // A partially mirrored copy of |type| which is slightly less granular and
  // which also identifies WebSocket requests separately from other types.
  const WebRequestResourceType web_request_type = WebRequestResourceType::OTHER;

  // Indicates if this request is asynchronous.
  const bool is_async;

  const net::HttpRequestHeaders extra_request_headers;

  // Indicates if this request is for a PAC script.
  const bool is_pac_request;

  // HTTP response code for this request if applicable. -1 if not.
  int response_code = -1;

  // All response headers for this request, if set.
  scoped_refptr<net::HttpResponseHeaders> response_headers;

  // The stringified IP address of the host which provided the response to this
  // request, if applicable and available.
  std::string response_ip;

  // Indicates whether or not the request response (if applicable) came from
  // cache.
  bool response_from_cache = false;

  // A dictionary of request body data matching the format expected by
  // WebRequest API consumers. This may have a "formData" key and/or a "raw"
  // key. See WebRequest API documentation for more details.
  std::unique_ptr<base::DictionaryValue> request_body_data;

  // Indicates whether this request was initiated by a <webview> instance.
  const bool is_web_view;

  // If |is_web_view| is true, the instance ID, rules registry ID, and embedder
  // process ID pertaining to the webview instance. Note that for browser-side
  // navigation requests, |web_view_embedder_process_id| is always -1.
  const int web_view_instance_id;
  const int web_view_rules_registry_id;
  const int web_view_embedder_process_id;

  // Helper used to log events relevant to WebRequest processing. This is always
  // non-null.
  const std::unique_ptr<WebRequestInfoLogger> logger;

  // The ResourceContext associated with this request. May be null.
  content::ResourceContext* const resource_context;

  // The Declarative Net Request action associated with this request. Mutable
  // since this is lazily computed. Cached to avoid redundant computations.
  mutable base::Optional<declarative_net_request::RulesetManager::Action>
      dnr_action;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRequestInfo);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_INFO_H_
