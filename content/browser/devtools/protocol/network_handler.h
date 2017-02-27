// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_NETWORK_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_NETWORK_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/network.h"
#include "net/base/net_errors.h"
#include "net/cookies/canonical_cookie.h"

namespace content {

class DevToolsSession;
class RenderFrameHostImpl;
struct BeginNavigationParams;
struct CommonNavigationParams;
struct ResourceRequest;
struct ResourceRequestCompletionStatus;
struct ResourceResponseHead;

namespace protocol {

class NetworkHandler : public DevToolsDomainHandler,
                       public Network::Backend {
 public:
  NetworkHandler();
  ~NetworkHandler() override;

  static NetworkHandler* FromSession(DevToolsSession* session);

  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderFrameHost(RenderFrameHostImpl* host) override;

  Response Enable(Maybe<int> max_total_size,
                  Maybe<int> max_resource_size) override;
  Response Disable() override;

  Response ClearBrowserCache() override;
  Response ClearBrowserCookies() override;

  void GetCookies(Maybe<protocol::Array<String>> urls,
                  std::unique_ptr<GetCookiesCallback> callback) override;
  void GetAllCookies(std::unique_ptr<GetAllCookiesCallback> callback) override;
  void DeleteCookie(const std::string& cookie_name,
                    const std::string& url,
                    std::unique_ptr<DeleteCookieCallback> callback) override;
  void SetCookie(
      const std::string& url,
      const std::string& name,
      const std::string& value,
      Maybe<std::string> domain,
      Maybe<std::string> path,
      Maybe<bool> secure,
      Maybe<bool> http_only,
      Maybe<std::string> same_site,
      Maybe<double> expires,
      std::unique_ptr<SetCookieCallback> callback) override;

  Response SetUserAgentOverride(const std::string& user_agent) override;
  Response CanEmulateNetworkConditions(bool* result) override;

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

 private:
  std::unique_ptr<Network::Frontend> frontend_;
  RenderFrameHostImpl* host_;
  bool enabled_;
  std::string user_agent_;

  DISALLOW_COPY_AND_ASSIGN(NetworkHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_NETWORK_HANDLER_H_
