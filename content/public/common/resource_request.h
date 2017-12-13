// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_RESOURCE_REQUEST_H_
#define CONTENT_PUBLIC_COMMON_RESOURCE_REQUEST_H_

#include <stdint.h>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "content/common/content_export.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/request_context_frame_type.h"
#include "content/public/common/request_context_type.h"
#include "content/public/common/resource_request_body.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/service_worker_modes.h"
#include "net/base/request_priority.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/interfaces/fetch_api.mojom.h"
#include "third_party/WebKit/common/page/page_visibility_state.mojom.h"
#include "third_party/WebKit/public/platform/WebMixedContentContextType.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

struct CONTENT_EXPORT ResourceRequest {
  ResourceRequest();
  ResourceRequest(const ResourceRequest& request);
  ~ResourceRequest();

  // The request method: GET, POST, etc.
  std::string method = "GET";

  // The absolute requested URL encoded in ASCII per the rules of RFC-2396.
  GURL url;

  // URL representing the first-party origin for the request, which may be
  // checked by the third-party cookie blocking policy. This is usually the URL
  // of the document in the top-level window. Leaving it empty may lead to
  // undesired cookie blocking. Third-party cookie blocking can be bypassed by
  // setting site_for_cookies = url, but this should ideally only be
  // done if there really is no way to determine the correct value.
  GURL site_for_cookies;

  // The origin of the context which initiated the request, which will be used
  // for cookie checks like 'First-Party-Only'.
  base::Optional<url::Origin> request_initiator;

  // The referrer to use (may be empty).
  GURL referrer;

  // The referrer policy to use.
  blink::WebReferrerPolicy referrer_policy = blink::kWebReferrerPolicyAlways;

  // The frame's visibility state.
  blink::mojom::PageVisibilityState visibility_state =
      blink::mojom::PageVisibilityState::kVisible;

  // Additional HTTP request headers.
  net::HttpRequestHeaders headers;

  // net::URLRequest load flags (0 by default).
  int load_flags = 0;

  // If this request originated from a pepper plugin running in a child
  // process, this identifies which process it came from. Otherwise, it
  // is zero.
  int plugin_child_id = ChildProcessHost::kInvalidUniqueID;

  // What this resource load is for (main frame, sub-frame, sub-resource,
  // object).
  // TODO(qinmin): this is used for legacy code path. With network service, it
  // shouldn't know about resource type.
  ResourceType resource_type = RESOURCE_TYPE_MAIN_FRAME;

  // The priority of this request determined by Blink.
  net::RequestPriority priority = net::IDLE;

  // Used by plugin->browser requests to get the correct net::URLRequestContext.
  uint32_t request_context = 0;

  // Indicates which frame (or worker context) the request is being loaded into,
  // or kAppCacheNoHostId.
  int appcache_host_id;

  // True if corresponding AppCache group should be resetted.
  bool should_reset_appcache = false;

  // https://wicg.github.io/cors-rfc1918/#external-request
  // TODO(toyoshim): The browser should know better than renderers do.
  // This is used to plumb Blink decided information for legacy code path, but
  // eventually we should remove this.
  bool is_external_request = false;

  // A policy to decide if CORS-preflight fetch should be performed.
  network::mojom::CORSPreflightPolicy cors_preflight_policy =
      network::mojom::CORSPreflightPolicy::kConsiderPreflight;

  // Indicates which frame (or worker context) the request is being loaded into,
  // or kInvalidServiceWorkerProviderId.
  int service_worker_provider_id = kInvalidServiceWorkerProviderId;

  // True if the request originated from a Service Worker, e.g. due to a
  // fetch() in the Service Worker script.
  bool originated_from_service_worker = false;

  // The service worker mode that indicates which service workers should get
  // events for this request.
  ServiceWorkerMode service_worker_mode = ServiceWorkerMode::ALL;

  // The request mode passed to the ServiceWorker.
  network::mojom::FetchRequestMode fetch_request_mode =
      network::mojom::FetchRequestMode::kSameOrigin;

  // The credentials mode passed to the ServiceWorker.
  network::mojom::FetchCredentialsMode fetch_credentials_mode =
      network::mojom::FetchCredentialsMode::kOmit;

  // The redirect mode used in Fetch API.
  FetchRedirectMode fetch_redirect_mode = FetchRedirectMode::FOLLOW_MODE;

  // The integrity used in Fetch API.
  std::string fetch_integrity;

  // The request context passed to the ServiceWorker.
  RequestContextType fetch_request_context_type =
      REQUEST_CONTEXT_TYPE_UNSPECIFIED;

  // The mixed content context type to be used for mixed content checks.
  blink::WebMixedContentContextType fetch_mixed_content_context_type =
      blink::WebMixedContentContextType::kBlockable;

  // The frame type passed to the ServiceWorker.
  RequestContextFrameType fetch_frame_type =
      REQUEST_CONTEXT_FRAME_TYPE_AUXILIARY;

  // Optional resource request body (may be null).
  scoped_refptr<ResourceRequestBody> request_body;

  // If true, then the response body will be downloaded to a file and the path
  // to that file will be provided in ResponseInfo::download_file_path.
  bool download_to_file = false;

  // True if the request can work after the fetch group is terminated.
  // https://fetch.spec.whatwg.org/#request-keepalive-flag
  bool keepalive = false;

  // True if the request was user initiated.
  bool has_user_gesture = false;

  // TODO(mmenke): Investigate if enable_load_timing is safe to remove.
  //
  // True if load timing data should be collected for request.
  bool enable_load_timing = false;

  // True if upload progress should be available for request.
  bool enable_upload_progress = false;

  // True if login prompts for this request should be supressed. Cached
  // credentials or default credentials may still be used for authentication.
  bool do_not_prompt_for_login = false;

  // The routing id of the RenderFrame.
  int render_frame_id = 0;

  // True if |frame_id| is the main frame of a RenderView.
  bool is_main_frame = false;

  ui::PageTransition transition_type = ui::PAGE_TRANSITION_LINK;

  // For navigations, whether this navigation should replace the current session
  // history entry on commit.
  bool should_replace_current_entry = false;

  // The following two members identify a previous request that has been
  // created before this navigation has been transferred to a new process.
  // This serves the purpose of recycling the old request.
  // Unless this refers to a transferred navigation, these values are -1 and -1.
  int transferred_request_child_id = -1;
  int transferred_request_request_id = -1;

  // Whether or not we should allow the URL to download.
  bool allow_download = false;

  // Whether to intercept headers to pass back to the renderer.
  bool report_raw_headers = false;

  // Whether or not to request a Preview version of the resource or let the
  // browser decide.
  PreviewsState previews_state = PREVIEWS_UNSPECIFIED;

  // PlzNavigate: the stream url associated with a navigation. Used to get
  // access to the body of the response that has already been fetched by the
  // browser.
  GURL resource_body_stream_url;

  // Wether or not the initiator of this request is a secure context.
  bool initiated_in_secure_context = false;

  // The response should be downloaded and stored in the network cache, but not
  // sent back to the renderer.
  bool download_to_network_cache_only = false;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_RESOURCE_REQUEST_H_
