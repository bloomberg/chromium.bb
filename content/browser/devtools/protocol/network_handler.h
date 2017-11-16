// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_NETWORK_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_NETWORK_HANDLER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/network.h"
#include "content/public/common/network_service.mojom.h"
#include "net/base/net_errors.h"
#include "net/cookies/canonical_cookie.h"

namespace net {
class HttpRequestHeaders;
class URLRequest;
}  // namespace net

namespace network {
struct URLLoaderStatus;
}  // namespace network

namespace content {
class DevToolsAgentHostImpl;
class RenderFrameHostImpl;
struct GlobalRequestID;
class InterceptionHandle;
class NavigationHandle;
class NavigationRequest;
class NavigationThrottle;
struct GlobalRequestID;
struct InterceptedRequestInfo;
struct ResourceRequest;
struct ResourceResponseHead;

namespace protocol {

class NetworkHandler : public DevToolsDomainHandler,
                       public Network::Backend {
 public:
  explicit NetworkHandler(const std::string& host_id);
  ~NetworkHandler() override;

  static std::vector<NetworkHandler*> ForAgentHost(DevToolsAgentHostImpl* host);

  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderer(RenderProcessHost* process_host,
                   RenderFrameHostImpl* frame_host) override;

  Response Enable(Maybe<int> max_total_size,
                  Maybe<int> max_resource_size) override;
  Response Disable() override;

  void ClearBrowserCache(
      std::unique_ptr<ClearBrowserCacheCallback> callback) override;

  void ClearBrowserCookies(
      std::unique_ptr<ClearBrowserCookiesCallback> callback) override;

  void GetCookies(Maybe<protocol::Array<String>> urls,
                  std::unique_ptr<GetCookiesCallback> callback) override;
  void GetAllCookies(std::unique_ptr<GetAllCookiesCallback> callback) override;
  void DeleteCookies(const std::string& name,
                     Maybe<std::string> url,
                     Maybe<std::string> domain,
                     Maybe<std::string> path,
                     std::unique_ptr<DeleteCookiesCallback> callback) override;
  void SetCookie(const std::string& name,
                 const std::string& value,
                 Maybe<std::string> url,
                 Maybe<std::string> domain,
                 Maybe<std::string> path,
                 Maybe<bool> secure,
                 Maybe<bool> http_only,
                 Maybe<std::string> same_site,
                 Maybe<double> expires,
                 std::unique_ptr<SetCookieCallback> callback) override;
  void SetCookies(
      std::unique_ptr<protocol::Array<Network::CookieParam>> cookies,
      std::unique_ptr<SetCookiesCallback> callback) override;

  Response SetUserAgentOverride(const std::string& user_agent) override;
  Response SetExtraHTTPHeaders(
      std::unique_ptr<Network::Headers> headers) override;
  Response CanEmulateNetworkConditions(bool* result) override;
  Response EmulateNetworkConditions(
      bool offline,
      double latency,
      double download_throughput,
      double upload_throughput,
      Maybe<protocol::Network::ConnectionType> connection_type) override;
  Response SetBypassServiceWorker(bool bypass) override;

  DispatchResponse SetRequestInterception(
      std::unique_ptr<protocol::Array<protocol::Network::RequestPattern>>
          patterns) override;
  void ContinueInterceptedRequest(
      const std::string& request_id,
      Maybe<std::string> error_reason,
      Maybe<std::string> base64_raw_response,
      Maybe<std::string> url,
      Maybe<std::string> method,
      Maybe<std::string> post_data,
      Maybe<protocol::Network::Headers> headers,
      Maybe<protocol::Network::AuthChallengeResponse> auth_challenge_response,
      std::unique_ptr<ContinueInterceptedRequestCallback> callback) override;

  void GetResponseBodyForInterception(
      const String& interception_id,
      std::unique_ptr<GetResponseBodyForInterceptionCallback> callback)
      override;

  void NavigationPreloadRequestSent(int worker_version_id,
                                    const std::string& request_id,
                                    const ResourceRequest& request);
  void NavigationPreloadResponseReceived(int worker_version_id,
                                         const std::string& request_id,
                                         const GURL& url,
                                         const ResourceResponseHead& head);
  void NavigationPreloadCompleted(
      const std::string& request_id,
      const network::URLLoaderStatus& completion_status);
  void NavigationFailed(NavigationRequest* navigation_request);

  bool enabled() const { return enabled_; }

  Network::Frontend* frontend() const { return frontend_.get(); }

  static GURL ClearUrlRef(const GURL& url);
  static std::unique_ptr<Network::Request> CreateRequestFromURLRequest(
      const net::URLRequest* request);

  std::unique_ptr<NavigationThrottle> CreateThrottleForNavigation(
      NavigationHandle* navigation_handle);
  bool ShouldCancelNavigation(const GlobalRequestID& global_request_id);
  void AppendDevToolsHeaders(net::HttpRequestHeaders* headers);
  bool ShouldBypassServiceWorker() const;

 private:
  void RequestIntercepted(std::unique_ptr<InterceptedRequestInfo> request_info);
  void SetNetworkConditions(mojom::NetworkConditionsPtr conditions);

  std::unique_ptr<Network::Frontend> frontend_;
  RenderProcessHost* process_;
  RenderFrameHostImpl* host_;
  bool enabled_;
  std::string user_agent_;
  std::vector<std::pair<std::string, std::string>> extra_headers_;
  std::string host_id_;
  std::unique_ptr<InterceptionHandle> interception_handle_;
  bool bypass_service_worker_;
  base::WeakPtrFactory<NetworkHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_NETWORK_HANDLER_H_
