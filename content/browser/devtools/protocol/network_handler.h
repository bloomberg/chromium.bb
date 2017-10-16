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
#include "net/base/net_errors.h"
#include "net/cookies/canonical_cookie.h"

namespace net {
class URLRequest;
}  // namespace net

namespace content {

class DevToolsAgentHostImpl;
class DevToolsNetworkConditions;
class RenderFrameHostImpl;
struct BeginNavigationParams;
struct CommonNavigationParams;
struct GlobalRequestID;
class NavigationHandle;
class NavigationThrottle;
struct ResourceRequest;
struct ResourceRequestCompletionStatus;
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

  Response ClearBrowserCache() override;
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
  Response CanEmulateNetworkConditions(bool* result) override;
  Response EmulateNetworkConditions(
      bool offline,
      double latency,
      double download_throughput,
      double upload_throughput,
      Maybe<protocol::Network::ConnectionType> connection_type) override;

  DispatchResponse SetRequestInterceptionEnabled(
      bool enabled,
      Maybe<protocol::Array<std::string>> patterns,
      Maybe<protocol::Array<std::string>> resource_types) override;
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

  void NavigationPreloadRequestSent(int worker_version_id,
                                    const std::string& request_id,
                                    const ResourceRequest& request);
  void NavigationPreloadResponseReceived(int worker_version_id,
                                         const std::string& request_id,
                                         const GURL& url,
                                         const ResourceResponseHead& head);
  void NavigationPreloadCompleted(
      const std::string& request_id,
      const ResourceRequestCompletionStatus& completion_status);
  void NavigationFailed(const CommonNavigationParams& common_params,
                        const BeginNavigationParams& begin_params,
                        net::Error error_code);

  bool enabled() const { return enabled_; }
  std::string UserAgentOverride() const;

  Network::Frontend* frontend() const { return frontend_.get(); }

  static std::unique_ptr<Network::Request> CreateRequestFromURLRequest(
      const net::URLRequest* request);

  std::unique_ptr<NavigationThrottle> CreateThrottleForNavigation(
      NavigationHandle* navigation_handle);
  void InterceptedNavigationRequest(const GlobalRequestID& global_request_id,
                                    const std::string& interception_id);
  void InterceptedNavigationRequestFinished(const std::string& interception_id);
  bool ShouldCancelNavigation(const GlobalRequestID& global_request_id);

 private:
  void SetNetworkConditions(
      std::unique_ptr<DevToolsNetworkConditions> conditions);

  std::unique_ptr<Network::Frontend> frontend_;
  RenderProcessHost* process_;
  RenderFrameHostImpl* host_;
  bool enabled_;
  bool interception_enabled_;
  std::string user_agent_;
  base::flat_map<std::string, GlobalRequestID> navigation_requests_;
  base::flat_set<GlobalRequestID> canceled_navigation_requests_;
  std::string host_id_;
  base::WeakPtrFactory<NetworkHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_NETWORK_HANDLER_H_
